//
// mpio wavy loop
//
// Copyright (C) 2008-2010 FURUHASHI Sadayuki
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#include "wavy_loop.h"
#include "wavy_out.h"
#include <glog/logging.h>
#include <sys/eventfd.h>
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

	m_eventfd = eventfd(0, 0);
	fcntl(m_eventfd, F_SETFL, O_NONBLOCK);
	get_kernel().add_fd(m_eventfd, EVKERNEL_READ);

	// add out handler
	{
		m_out.reset(new out);
		set_handler(m_out);
		get_kernel().add_kernel(&m_out->get_kernel());
	}
}

loop_impl::~loop_impl()
{
	DVLOG(20) << "loop_impl::~loop_impl";
	end();
	join();  // FIXME detached?
	delete[] m_state;
	close(m_eventfd);
}

void loop_impl::wake_epoll()
{
	DVLOG(20) << "loop_impl::wake_epoll";
	eventfd_t val = 1;
	int ret = eventfd_write(m_eventfd, val);
	DVLOG(20) << "eventfd_write() returned: " << ret;
}

void loop_impl::end()
{
	DVLOG(20) << "loop_impl::end";
	pthread_scoped_lock lk(m_mutex);
	m_end_flag = true;
	wake_epoll();  // Bring on the thundering herd. :)
}

bool loop_impl::is_end() const
{
	return m_end_flag;
}


void loop_impl::join()
{
	DVLOG(20) << "loop_impl::join";

	// Clean up - we should have no threads running event loops now.
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		try {
			it->join();
		} catch (mp::pthread_error& e) {
			DLOG(INFO) << "Pthread Error for:" << &(*it);
			if(e.code == EDEADLK) {
				DLOG(ERROR) << "Deadlock joining worker thread. Detaching.";
				it->detach();
			} else {
				throw e;
			}
		}
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
	DVLOG(20) << "loop_impl::add_thread";
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
	DVLOG(20) << "loop_impl::submit_impl";
	pthread_scoped_lock lk(m_mutex);
	m_task_queue.push(f);
	wake_epoll();
}


shared_ptr<basic_handler> loop_impl::add_handler_impl(shared_ptr<basic_handler> sh)
{
	DVLOG(20) << "loop_impl::add_handler_impl for fd " << sh->ident();
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
	DVLOG(20) << "loop_impl::remove_handler for fd " << fd;
	reset_handler(fd);
	m_kernel.remove_fd(fd, EVKERNEL_READ);
}


void loop_impl::do_task(pthread_scoped_lock& lk)
{
	DVLOG(20) << "loop_impl::do_task";
	task_t ev = m_task_queue.front();
	m_task_queue.pop();

	lk.unlock();
	try {
		ev();
	} catch (...) { }
	lk.relock(m_mutex);

	if(!m_task_queue.empty()) {
		wake_epoll();
	}
}

void loop_impl::do_out(pthread_scoped_lock& lk)
{
	DVLOG(20) << "loop_impl::do_out";
	kernel::event ke = m_out->next();

	lk.unlock();
	bool ret = m_out->write_event(ke);
	lk.relock(m_mutex);
	if(ret) {
		wake_epoll();
	}
}

void loop_impl::thread_main()
{
	pthread_scoped_lock lk(m_mutex);
	while(!m_end_flag) {
		run_once(lk, 1000);
	}  // while(true)
}


inline void loop_impl::run_once()
{
	DVLOG(20) << "loop_impl::run_once";
	pthread_scoped_lock lk(m_mutex);
	run_once(lk, 1000);
}

inline void loop_impl::run_nonblock()
{
	DVLOG(20) << "loop_impl::run_nonblock";
	pthread_scoped_lock lk(m_mutex);
	run_once(lk, 0);
}

bool loop_impl::run_once(pthread_scoped_lock& lk, int timeout_ms)
{
	DVLOG(20) << "loop_impl::run_once with timeout " << timeout_ms << "ms";
	if(m_end_flag) { 
		return true; 
	}

	kernel::event ke;

	if(!m_task_queue.empty()) {
		DVLOG(20) << "Queued task found. Running.";
		do_task(lk);
		return true;
	}
	if(m_out->has_queue()) {
		DVLOG(20) << "Queued pending write found. Executing.";
		do_out(lk);  // FIXME
		return true;
	}
	if(m_num == m_off) {
		lk.unlock();
		int num = m_kernel.wait(&m_backlog, timeout_ms);
		lk.relock(m_mutex);
		if(m_end_flag) return false; // TODO(aarond10): remove me
		if(num <= 0) {
			if(num != 0 && errno != EINTR && errno != EAGAIN) {
				throw system_error(errno, "wavy kernel event failed");
			}
			return false;
		}

		m_off = 0;
		m_num = num;
	}

	ke = m_backlog[m_off++];

	int ident = ke.ident();

	if(ident == m_eventfd) {
		// We just use this to wake up an epoll thread when we have an event to do.
		// The actual data doesn't matter.
		eventfd_t val;
		int ret = eventfd_read(m_eventfd, &val);
		DVLOG(20) << "eventfd_read returned " << ret << " with value: " << val;
		m_kernel.reactivate(ke);
	} else if(ident == m_out->ident()) {
		// The asynchronous write events are handled in a separate event kernel.
		// We add this second kernel to our main event queue and poll it whenever an
		// event occurs on it.
		DVLOG(20) << "Activity on output event kernel. Polling.";
		m_out->poll_event();
		DVLOG(20) << "m_out has " << (m_out->has_queue() ? "pending events" : "no events");
		m_kernel.reactivate(ke);
	} else {

		event_impl e(this, ke);
		shared_handler h = m_state[ident];

		bool cont = false;
		if(h) {
			DVLOG(20) << "Read event with state handler for fd " << ident << ". Executing.";
			lk.unlock();
			try {
				cont = (*h)(e);
			} catch (...) { }
			lk.relock(m_mutex);
		} else {
			DLOG(INFO) << "Read event WITHOUT state handler for fd " << ident << ".";
		}

		if(!e.is_reactivated()) {
			if(e.is_removed()) {
				return true;
			}
			if(!cont) {
				m_kernel.remove(ke);
				reset_handler(ident);
				return true;
			}
			m_kernel.reactivate(ke);
		} 
	}
	return true;
}


void loop_impl::flush()
{
	DVLOG(20) << "loop_impl::flush";
	pthread_scoped_lock lk(m_mutex);
	while(!m_out->empty() || !m_task_queue.empty()) {
		if(is_running()) {
			m_flush_cond.wait(m_mutex);
		} else {
			run_once(lk, 1000);
		}
	}
}

void loop_impl::event_remove(kernel::event ke)
{
	DVLOG(20) << "loop_impl::event_remove for fd " << ke.ident();
	m_kernel.remove(ke);
	reset_handler(ke.ident());
}


}  // noname namespace

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

void loop::run_nonblock()
	{ ANON_impl->run_nonblock(); }

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

