//
// mpio wavy connect
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
#include "wavy_timer.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

namespace mp {
namespace wavy {

namespace {


class connect_handler : public basic_handler {
public:
	typedef loop::connect_callback_t connect_callback_t;

	connect_handler(int ident, connect_callback_t callback) :
		basic_handler(ident, this),
		m_done(false), m_callback(callback)
	{ }

	~connect_handler()
	{ }

	bool operator() (event& e)
	{
		int fd = ident();

		if(!__sync_bool_compare_and_swap(&m_done, false, true)) {
			::close(fd);
			return false;
		}

		int err = 0;

		int value = 0;
		int len = sizeof(value);

		if(::getsockopt(fd, SOL_SOCKET, SO_ERROR,
				&value, (socklen_t*)&len) < 0) {
			goto errno_error;
		}

		if(value != 0) {
			err = value;
			goto specific_error;
		}

		goto out;

	errno_error:
		err = errno;

	specific_error:
		::close(fd);
		fd = -1;

	out:
		e.remove();
		m_callback(fd, err);
		return false;
	}

	class timeout_handler : kernel_timer, public basic_handler {
	public:
		timeout_handler(kernel& kern, const timespec* timeout,
				shared_handler handler) :
			kernel_timer(kern, timeout, NULL),
			basic_handler(timer_ident(), this),
			m_handler( static_pointer_cast<connect_handler>(handler) )
		{ }

		~timeout_handler() { }

		bool operator() (event& e)
		{
			shared_ptr<connect_handler> h(m_handler.lock());
			if(h) {
				h->fail(ETIMEDOUT);
			}
			return false;
		}

	private:
		weak_ptr<connect_handler> m_handler;
	};

	void set_timer(loop* lo, shared_ptr<timeout_handler>& timer)
	{
		m_loop = lo;
		m_timer = weak_ptr<timeout_handler>(timer);
	}

	void fail(int err)
	{
		if(!__sync_bool_compare_and_swap(&m_done, false, true)) {
			return;
		}
		m_callback(-1, err);
	}

private:
	bool m_done;
	connect_callback_t m_callback;

	loop* m_loop;
	weak_ptr<timeout_handler> m_timer;
};


}  // noname namespace


void loop::connect(
		int socket_family, int socket_type, int protocol,
		const sockaddr* addr, socklen_t addrlen,
		const timespec* timeout, connect_callback_t callback)
{
	shared_ptr<connect_handler> sh;

	int err = 0;
	int fd = ::socket(socket_family, socket_type, protocol);
	if(fd < 0) {
		err = errno;
		goto out;
	}

	if(::fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		goto errno_error;
	}

	if(::connect(fd, addr, addrlen) >= 0) {
		// connect success
		goto out;
	}

	if(errno != EINPROGRESS) {
		goto errno_error;
	}

	try {
		sh = add_handler<connect_handler>(fd, callback);
	} catch (...) {
		err = 0;
		goto specific_error;
	}

	if(timeout == NULL || (timeout->tv_sec == 0 &&
				timeout->tv_nsec == 0)) {
		return;
	}

	try {
		shared_ptr<connect_handler::timeout_handler> timer(
				new connect_handler::timeout_handler(
					ANON_impl->get_kernel(), timeout, sh));

		ANON_impl->set_handler(timer);

		sh->set_timer(this, timer);

		return;

	} catch (const system_error& e) {
		sh->fail(0);
		return;

	} catch (...) {
		sh->fail(0);
		return;
	}

errno_error:
	err = errno;

specific_error:
	::close(fd);
	fd = -1;

out:
	submit(callback, fd, err);
}


void loop::connect(
		int socket_family, int socket_type, int protocol,
		const sockaddr* addr, socklen_t addrlen,
		double timeout_sec, connect_callback_t callback)
{
	struct timespec timeout = { (time_t)timeout_sec, (long)(timeout_sec * 1e9) };
	return connect(socket_family, socket_type, protocol,
			addr, addrlen, &timeout, callback);
}


}  // namespace wavy
}  // namespace mp

