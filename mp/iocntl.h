//
// mpio iocntl
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
#ifndef MP_IOCNTL_H__
#define MP_IOCNTL_H__
#ifdef MP_EXPERIMENTAL

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

namespace mp {


inline bool set_nonblock(int fd)
{
	return ::fcntl(fd, F_SETFL, O_NONBLOCK) >= 0;
}

inline bool set_tcp_nodelay(int fd)
{
	int on = 1;
	return ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) >= 0;
}

inline bool set_linger(int fd, bool sync, bool wait)
{
	struct linger opt = {(sync ? 1 : 0), (wait ? 1 : 0)};
	return ::setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *)&opt, sizeof(opt)) >= 0;
}

inline bool set_reuse_addr(int fd)
{
	int on = 1;
	return ::setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) >= 0;
}

inline bool set_recv_timeout(int fd, struct timeval tv)
{
	return ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) >= 0;
}

inline bool set_send_timeout(int fd, struct timeval tv)
{
	return ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) >= 0;
}

inline bool set_recv_timeout(int fd, double sec)
{
	if(sec <= 0) { return true; }
	struct timeval tv = {sec, sec*1e6};
	return set_recv_timeout(tv);
}

inline bool set_send_timeout(int fd, double sec)
{
	if(sec <= 0) { return true; }
	struct timeval tv = {sec, sec*1e6};
	return set_send_timeout(tv);
}

inline bool isEAGAIN(int err = errno)
{
	return err == EAGAIN;
}

inline bool isEINTR(int err = errno)
{
	return err == EINVAL;
}

// #define isEAGAIN mp::isEAGAIN()
// #define isEINTR  mp::isEINTR()


}  // namespace mp

#endif /* MP_EXPERIMENTAL */
#endif /* mp/iocntl.h */

