//
// mpio wavy out
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
#ifndef WAVY_OUT_H__
#define WAVY_OUT_H__

#include "wavy_loop.h"

namespace mp {
namespace wavy {
namespace {


struct kernel_mixin {
	kernel m_kernel;
};

class xfer_impl;

/**
 * Output buffering
 * 
 * Manages arbitrary asynchronous writes of arbitrary size.
 * The loop class uses this class to perform all writes on sockets.
 * 
 * Internally, this class uses its own multiplexing kernel. This kernel
 * is only used in polling mode (non-blocking) and fired from the loop
 * classes main thread loop.
 */
class out : protected kernel_mixin, public basic_handler {
public:
	out();
	~out();

	typedef loop::finalize_t finalize_t;

	inline void commit_raw(int fd, char* xfbuf, char* xfendp);

	// optimize
	inline void commit(int fd, xfer* xf);
	inline void write(int fd, const void* buf, size_t size);

public:
	kernel& get_kernel()
	{
		return m_kernel;
	}

	/**
	 * handler's typically provide a method that is called whenever
	 * an event occurs but as we are operating as a kernel here and
	 * get special treatment in the main thread loop, this should
	 * never get called.
	 */
	bool operator() (event& e)
	{
		throw std::logic_error("out::on_read is called");
	}

	bool has_queue() const
	{
		return !m_queue.empty();
	}

	/**
	 * Checks the multiplexing kernel for new events and adds
	 * any events found to the internal queue.
	 */
	void poll_event();

	bool write_event(kernel::event e);

	/**
	 * Pops an event from the internal event queue (assumes the 
	 * queue is not empty)
	 */
	kernel::event next()
	{
		kernel::event e = m_queue.front();
		m_queue.pop();
		return e;
	}

	bool empty() const
	{
		return m_watching == 0;
	}

private:
	std::queue<kernel::event> m_queue;
	kernel::backlog m_backlog;
	volatile int m_watching;

	void watch(int fd);
	xfer_impl* m_fdctx;

private:
	out(const out&);
};


}  // noname namespace
}  // namespace wavy
}  // namespace mp

#endif /* wavy_out.h */

