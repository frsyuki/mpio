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
#ifndef WAVY_LOOP_H__
#define WAVY_LOOP_H__

#include "mp/wavy.h"
#include "mp/pthread.h"
#include "wavy_kernel.h"
#include <queue>

namespace mp {
namespace wavy {
namespace {


class out;


class loop_impl {
public:
	loop_impl(function<void ()> thread_init_func = function<void ()>());
	~loop_impl();

	typedef shared_ptr<basic_handler> shared_handler;
	typedef function<void ()> task_t;

public:
	void start(size_t num);
	void start(size_t num, size_t max);

	bool is_running() const;

	void end();
	bool is_end() const;

	void run_once();
	void run_once(pthread_scoped_lock& lk);

	void join();
	void detach();

	void add_thread(size_t num);

	shared_handler add_handler_impl(shared_handler sh);

	void remove_handler(int fd);

	void submit_impl(task_t& f);

	void set_handler(shared_handler sh)
	{
		m_state[sh->ident()] = sh;
	}

	void reset_handler(int ident)
	{
		m_state[ident].reset();
	}

	kernel& get_kernel()
	{
		return m_kernel;
	}

	void flush();

public:
	void thread_main();
	inline void do_task(pthread_scoped_lock& lk);
	inline void do_out(pthread_scoped_lock& lk);
	inline void event_more(kernel::event ke);
	inline void event_next(kernel::event ke);
	inline void event_remove(kernel::event ke);

private:
	volatile size_t m_off;
	volatile size_t m_num;
	volatile bool m_pollable;
//	volatile pthread_t m_poll_thread;  // FIXME signal_stop

	kernel::backlog m_backlog;

	shared_handler* m_state;

	kernel m_kernel;

	pthread_mutex m_mutex;
	pthread_cond m_cond;

	typedef std::queue<task_t> task_queue_t;
	task_queue_t m_task_queue;

	typedef std::queue<kernel::event> more_queue_t;
	more_queue_t m_more_queue;

	function<void ()> m_thread_init_func;

	pthread_cond m_flush_cond;

private:
	shared_ptr<out> m_out;
	friend class wavy::loop;

private:
	volatile bool m_end_flag;

	typedef std::vector<pthread_thread> workers_t;
	workers_t m_workers;

private:
	loop_impl(const loop_impl&);
};


#define ANON_impl static_cast<loop_impl*>(m_impl)

class event_impl : public event {
public:
	event_impl(loop_impl* lo, kernel::event ke) :
		m_flags(0),
		m_loop(lo),
		m_pe(ke) { }

	~event_impl() { }

	bool is_reactivated()
	{
		return (m_flags & FLAG_REACTIVATED) != 0;
	}

	bool is_removed()
	{
		return (m_flags & FLAG_REMOVED) != 0;
	}

	const kernel::event& get_kernel_event() const
	{
		return m_pe;
	}

private:
	enum {
		FLAG_REACTIVATED = 0x01,
		FLAG_REMOVED     = 0x02,
	};
	int m_flags;
	loop_impl* m_loop;
	kernel::event m_pe;
	friend class event;
};


}  // noname namespace
}  // namespace wavy
}  // namespace mp

#endif /* wavy_loop.h */

