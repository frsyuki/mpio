//
// mpio wavy loop
//
// Copyright (C) 2008-2010 FURUHASHI Sadayuki
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#include "wavy_loop.h"
#include "wavy_out.h"
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

// linkage hack for out::out, out::poll_event and out::write_event
#include "wavy_out.cc"

#ifndef MP_WAVY_TASK_QUEUE_LIMIT
#define MP_WAVY_TASK_QUEUE_LIMIT 16
#endif

namespace mp {
namespace wavy {
namespace {


loop_impl::loop_impl(function<void ()> thread_init_func) :
	m_off(0), m_num(0), m_pollable(true),
	m_thread_init_func(thread_init_func),
	m_end_flag(false)
{
	m_state = new shared_handler[m_kernel.max()];

	// add out handler
	{
		m_out.reset(new out);
		set_handler(m_out);
		get_kernel().add_kernel(&m_out->get_kernel());
	}
}

loop_impl::~loop_impl()
{
	end();
	join();  // FIXME detached?
	{
		pthread_scoped_lock lk(m_mutex);
		m_cond.broadcast();
	}
	delete[] m_state;
}

void loop_impl::end()
{
	m_end_flag = true;
	{
		pthread_scoped_lock lk(m_mutex);
		m_cond.broadcast();
//		if(m_poll_thread) {  // FIXME signal_stop
//			pthread_kill(m_poll_thread, SIGALRM);
//		}
	}
}

bool loop_impl::is_end() const
{
	return m_end_flag;
}


void loop_impl::join()
{
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		it->join();
	}
	m_workers.clear();
}

void loop_impl::detach()
{
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		it->detach();
	}
}


void loop_impl::start(size_t num)
{
	pthread_scoped_lock lk(m_mutex);
	if(is_running()) {
		// FIXME exception
		throw std::runtime_error("loop is already running");
	}
	add_thread(num);
}

void loop_impl::add_thread(size_t num)
{
	for(size_t i=0; i < num; ++i) {
		m_workers.push_back( pthread_thread() );
		try {
			m_workers.back().run(
					bind(&loop_impl::thread_main, this));
		} catch (...) {
			m_workers.pop_back();
			throw;
		}
	}
}

bool loop_impl::is_running() const
{
	return !m_workers.empty();
}

void loop_impl::submit_impl(task_t& f)
{
	pthread_scoped_lock lk(m_mutex);
	m_task_queue.push(f);
	m_cond.signal();
}


shared_ptr<basic_handler> loop_impl::add_handler_impl(shared_ptr<basic_handler> sh)
{
	int fd = sh->ident();
	if(::fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		throw system_error(errno, "failed to set nonblock flag");
	}

	set_handler(sh);
	get_kernel().add_fd(fd, EVKERNEL_READ);

	return sh;
}

void loop_impl::remove_handler(int fd)
{
	reset_handler(fd);
	m_kernel.remove_fd(fd, EVKERNEL_READ);
}


void loop_impl::do_task(pthread_scoped_lock& lk)
{
	task_t ev = m_task_queue.front();
	m_task_queue.pop();

	bool last = m_task_queue.empty();
	if(!last) { m_cond.signal(); }

	lk.unlock();

	try {
		ev();
	} catch (...) { }

	if(last) {
		lk.relock(m_mutex);
		m_flush_cond.broadcast();
	}
}

void loop_impl::do_out(pthread_scoped_lock& lk)
{
	kernel::event ke = m_out->next();

	lk.unlock();

	if(m_out->write_event(ke)) {
		lk.relock(m_mutex);
		m_flush_cond.broadcast();
	}
}

void loop_impl::thread_main()
{
	retry:
	while(true) {
		pthread_scoped_lock lk(m_mutex);

		retry_task:
		if(m_end_flag) { break; }

		kernel::event ke;

		if(!m_more_queue.empty()) {
			ke = m_more_queue.front();
			m_more_queue.pop();
			goto process_handler;
		}

		if(!m_pollable) {
			if(m_out->has_queue()) {
				do_out(lk);
				goto retry;
			} else if(!m_task_queue.empty()) {
				do_task(lk);
				goto retry;
			} else {
				m_cond.wait(m_mutex);
				goto retry_task;
			}
		} else if(m_task_queue.size() > MP_WAVY_TASK_QUEUE_LIMIT) {
			do_task(lk);
			goto retry;
		}

		if(m_num == m_off) {
			m_pollable = false;
//m_poll_thread = pthread_self();  // FIXME signal_stop
			lk.unlock();

			retry_poll:
			int num = m_kernel.wait(&m_backlog, 1000);

			if(num <= 0) {
				if(num == 0 || errno == EINTR || errno == EAGAIN) {
					if(m_end_flag) {
						m_pollable = true;
						break;
					}
					goto retry_poll;
				} else {
					throw system_error(errno, "wavy kernel event failed");
				}
			}

			lk.relock(m_mutex);
			m_off = 0;
			m_num = num;

//m_poll_thread = 0;  // FIXME signal_stop
			m_pollable = true;
			m_cond.signal();
		}

		ke = m_backlog[m_off++];

		process_handler:
		int ident = ke.ident();

		if(ident == m_out->ident()) {
			m_out->poll_event();
			lk.unlock();

			m_kernel.reactivate(ke);

		} else {
			lk.unlock();

			event_impl e(this, ke);
			shared_handler h = m_state[ident];

			bool cont = false;
			if(h) {
				try {
					cont = (*h)(e);
				} catch (...) { }
			}

			if(!e.is_reactivated()) {
				if(e.is_removed()) {
					goto retry;
				}
				if(!cont) {
					m_kernel.remove(ke);
					reset_handler(ident);
					goto retry;
				}
				m_kernel.reactivate(ke);
			}
		}

	}  // while(true)
}


inline void loop_impl::run_once()
{
	pthread_scoped_lock lk(m_mutex);
	run_once(lk);
}

void loop_impl::run_once(pthread_scoped_lock& lk)
{
	if(m_end_flag) { return; }

	kernel::event ke;

	if(!m_more_queue.empty()) {
		ke = m_more_queue.front();
		m_more_queue.pop();
		goto process_handler;
	}

	if(!m_pollable) {
		if(m_out->has_queue()) {
			do_out(lk);
		} else if(!m_task_queue.empty()) {
			do_task(lk);
		} else {
			m_cond.wait(m_mutex);
		}
		return;
	} else if(!m_task_queue.empty()) {
		do_task(lk);
		return;
	} else if(m_out->has_queue()) {
		do_out(lk);  // FIXME
		return;
	}

	if(m_num == m_off) {
		m_pollable = false;
		lk.unlock();

		int num = m_kernel.wait(&m_backlog, 1000);

		if(num <= 0) {
			if(num == 0 || errno == EINTR || errno == EAGAIN) {
				m_pollable = true;
				return;
			} else {
				throw system_error(errno, "wavy kernel event failed");
			}
		}

		lk.relock(m_mutex);
		m_off = 0;
		m_num = num;

		m_pollable = true;
		m_cond.signal();
	}

	ke = m_backlog[m_off++];

	process_handler:
	int ident = ke.ident();

	if(ident == m_out->ident()) {
		m_out->poll_event();
		lk.unlock();

		m_kernel.reactivate(ke);

	} else {
		lk.unlock();

		event_impl e(this, ke);
		shared_handler h = m_state[ident];

		bool cont = false;
		if(h) {
			try {
				cont = (*h)(e);
			} catch (...) { }
		}

		if(!e.is_reactivated()) {
			if(e.is_removed()) {
				return;
			}
			if(!cont) {
				m_kernel.remove(ke);
				reset_handler(ident);
				return;
			}
			m_kernel.reactivate(ke);
		}
	}
}


void loop_impl::flush()
{
	pthread_scoped_lock lk(m_mutex);
	while(!m_out->empty() || !m_task_queue.empty()) {
		if(is_running()) {
			m_flush_cond.wait(m_mutex);
		} else {
			run_once(lk);
			if(!lk.owns()) {
				lk.relock(m_mutex);
			}
		}
	}
}


void loop_impl::event_more(kernel::event ke)
{
	pthread_scoped_lock lk(m_mutex);
	m_more_queue.push(ke);
	m_cond.signal();
}

void loop_impl::event_next(kernel::event ke)
{
	m_kernel.reactivate(ke);
}

void loop_impl::event_remove(kernel::event ke)
{
	m_kernel.remove(ke);
	reset_handler(ke.ident());
}


}  // noname namespace


void event::more()
{
	event_impl* self = static_cast<event_impl*>(this);
	if(!self->is_reactivated()) {
		self->m_loop->event_more(self->m_pe);
		self->m_flags |= 0x01;
	}
}

void event::next()
{
	event_impl* self = static_cast<event_impl*>(this);
	if(!self->is_reactivated()) {
		self->m_loop->event_next(self->m_pe);
		self->m_flags |= event_impl::FLAG_REACTIVATED;
	}
}

void event::remove()
{
	event_impl* self = static_cast<event_impl*>(this);
	if(!self->is_removed()) {
		self->m_loop->event_remove(self->m_pe);
		self->m_flags |= event_impl::FLAG_REMOVED;
	}
}


loop::loop() : m_impl(new loop_impl()) { }

loop::~loop() { delete ANON_impl; }

void loop::run(size_t num)
{
	start(num);
	join();
}

void loop::start(size_t num)
	{ ANON_impl->start(num); }

bool loop::is_running() const
	{ return ANON_impl->is_running(); }

void loop::run_once()
	{ ANON_impl->run_once(); }

void loop::end()
	{ ANON_impl->end(); }

bool loop::is_end() const
	{ return ANON_impl->is_end(); }

void loop::join()
	{ ANON_impl->join(); }

void loop::detach()
	{ ANON_impl->detach(); }

void loop::add_thread(size_t num)
	{ ANON_impl->add_thread(num); }

shared_handler loop::add_handler_impl(shared_handler newh)
	{ return ANON_impl->add_handler_impl(newh); }

void loop::remove_handler(int fd)
	{ ANON_impl->remove_handler(fd); }

void loop::submit_impl(task_t f)
	{ ANON_impl->submit_impl(f); }

void loop::flush()
	{ ANON_impl->flush(); }

}  // namespace wavy
}  // namespace mp

