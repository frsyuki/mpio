//
// mpio wavy listen
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
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

namespace mp {
namespace wavy {

namespace {


class listen_handler : public handler {
public:
	typedef loop::listen_callback_t listen_callback_t;

	listen_handler(int fd, listen_callback_t callback) :
		handler(fd), m_callback(callback) { }

	~listen_handler() { }

	void on_read(event& e)
	{
		while(true) {
			int err = 0;
			int sock = ::accept(fd(), NULL, NULL);
			if(sock < 0) {
				if(errno == EAGAIN || errno == EINTR) {
					return;
				}
				err = errno;

				m_callback(sock, err);

				throw system_error(errno, "accept failed");
			}

			try {
				m_callback(sock, err);
			} catch (...) {
				::close(sock);
			}
		}
	}

private:
	listen_callback_t m_callback;

private:
	listen_handler();
	listen_handler(const listen_handler&);
};


}  // noname namespace


int loop::listen(
		int socket_family, int socket_type, int protocol,
		const sockaddr* addr, socklen_t addrlen,
		listen_callback_t callback,
		int backlog)
{
	int lsock = ::socket(socket_family, socket_type, protocol);
	if(lsock < 0) {
		throw mp::system_error(errno, "socket() failed");
	}

	int on = 1;
	if(::setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		::close(lsock);
		throw system_error(errno, "setsockopt failed");
	}
	
	if(::bind(lsock, addr, addrlen) < 0) {
		::close(lsock);
		throw system_error(errno, "bind failed");
	}
	
	if(::listen(lsock, backlog) < 0) {
		::close(lsock);
		throw system_error(errno, "listen failed");
	}

	try {
		add_handler<listen_handler>(lsock, callback);
	} catch (...) {
		::close(lsock);
		throw;
	}

	return lsock;
}


}  // namespace wavy
}  // namespace mp

