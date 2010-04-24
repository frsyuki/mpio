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

	bool operator() (event& e)
	{
		throw std::logic_error("out::on_read is called");
	}

	bool has_queue() const
	{
		return !m_queue.empty();
	}

	void poll_event();

	bool write_event(kernel::event e);

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
	void* m_fdctx;

private:
	out(const out&);
};


}  // noname namespace
}  // namespace wavy
}  // namespace mp

#endif /* wavy_out.h */

