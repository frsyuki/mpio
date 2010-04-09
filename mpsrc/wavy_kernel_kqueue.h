//
// mpio wavy kernel kqueue
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
#ifndef MP_WAVY_KERNEL_KQUEUE_H__
#define MP_WAVY_KERNEL_KQUEUE_H__

#include "mp/exception.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifndef MP_WAVY_KERNEL_KQUEUE_XIDENT_MAX
#define MP_WAVY_KERNEL_KQUEUE_XIDENT_MAX 256
#endif

namespace mp {
namespace wavy {


static const short EVKERNEL_READ  = EVFILT_READ;
static const short EVKERNEL_WRITE = EVFILT_WRITE;


class kernel {
public:
	kernel() : m_kq(kqueue())
	{
		struct rlimit rbuf;
		if(::getrlimit(RLIMIT_NOFILE, &rbuf) < 0) {
			::close(m_kq);
			throw system_error(errno, "getrlimit() failed");
		}
		m_fdmax = rbuf.rlim_cur;

		if(m_kq < 0) {
			throw system_error(errno, "failed to initialize kqueue");
		}

		memset(m_xident, 0, sizeof(m_xident));
		m_xident_index = 0;
	}

	~kernel()
	{
		::close(m_kq);
	}

	size_t max() const
	{
		return m_fdmax + MP_WAVY_KERNEL_KQUEUE_XIDENT_MAX;
	}


private:
	int alloc_xident()
	{
		for(unsigned int i=0; i < MP_WAVY_KERNEL_KQUEUE_XIDENT_MAX*2; ++i) {
			unsigned int ident = __sync_fetch_and_add(&m_xident_index, 1) % MP_WAVY_KERNEL_KQUEUE_XIDENT_MAX;
			if(__sync_bool_compare_and_swap(&m_xident[ident], false, true)) {
				return ident;
			}
		}
		errno = EMFILE;
		return -1;
	}

	bool free_xident(int ident)
	{
		bool* xv = m_xident + ident;
		// FIXME cas?
		if(*xv) {
			*xv = false;
			return true;
		}
		return false;
	}

	int set_event(uintptr_t ident, short filter, u_short flags,
			u_int fflags, intptr_t data, void* udata)
	{
		struct kevent kev;
		EV_SET(&kev, ident, filter, flags, fflags, data, udata);
		return kevent(m_kq, &kev, 1, NULL, 0, NULL);
	}


public:
	class event {
	public:
		event() { }
		explicit event(struct kevent kev_) : kev(kev_) { }
		~event() { }

		int ident() const {
			if(kev.filter == EVFILT_SIGNAL) {
				return (int)(intptr_t)kev.udata;
			}
			return kev.ident;
		}

	private:
		struct kevent kev;
		friend class kernel;
	};


	int add_fd(int fd, short event)
	{
		return set_event(fd, event, EV_ADD|EV_ONESHOT, 0, 0, NULL);
	}

	int remove_fd(int fd, short event)
	{
		return set_event(fd, event, EV_DELETE, 0, 0, NULL);
	}


	class timer {
	public:
		timer() : xident(-1) { }
		~timer() {
			if(xident >= 0) {
				kern->remove_timer(xident);
				kern->free_xident(xident);
			}
		}

		int ident() const { return xident; }

	private:
		int xident;
		kernel* kern;
		friend class kernel;
		timer(const timer&);
	};

	friend class timer;

	int add_timer(timer* tm, const timespec* value, const timespec* interval)
	{
		int xident = alloc_xident();
		if(xident < 0) {
			return -1;
		}

		unsigned long data;
		unsigned long udata;
		if(interval) {
			udata = interval->tv_sec*1000 + interval->tv_nsec/1000/1000;
		} else {
			udata = 0;
		}
		if(value) {
			data = value->tv_sec*1000 + value->tv_nsec/1000/1000;
		} else {
			data = udata;
		}

		if(set_event(xident, EVFILT_TIMER, EV_ADD|EV_ONESHOT, 0,
					data, (void*)udata) < 0) {
			free_xident(xident);
			return -1;
		}

		tm->xident = xident;
		tm->kern = this;
		return xident;
	}

	int remove_timer(int ident)
	{
		return set_event(ident, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
	}

	static int read_timer(event e)
	{
		return 0;
	}


	class signal {
	public:
		signal() : xident(-1) { }
		~signal() {
			if(xident >= 0) {
				kern->remove_signal(xident);
				kern->free_xident(xident);
			}
		}

		int ident() const { return xident; }

	private:
		int xident;
		kernel* kern;
		friend class kernel;
		signal(const signal&);
	};

	friend class signal;

	int add_signal(signal* sg, int signo)
	{
		int xident = alloc_xident();
		if(xident < 0) {
			return -1;
		}

		if(set_event(signo,EVFILT_SIGNAL, EV_ADD|EV_ONESHOT, 0,
					0, (void*)xident) < 0) {
			free_xident(xident);
			return -1;
		}

		sg->xident = xident;
		sg->kern = this;
		return xident;
	}

	int remove_signal(int ident)
	{
		return set_event(ident, EVFILT_SIGNAL, EV_DELETE, 0, 0, NULL);
	}

	static int read_signal(event e)
	{
		return 0;
	}


	int add_kernel(kernel* pt)
	{
		if(add_fd(pt->m_kq, EVKERNEL_READ) < 0) {
			return -1;
		}
		return pt->m_kq;
	}

	int ident() const
	{
		return m_kq;
	}


	class backlog {
	public:
		backlog()
		{
			buf = (struct kevent*)::calloc(
					sizeof(struct kevent),
					MP_WAVY_KERNEL_BACKLOG_SIZE);
			if(!buf) { throw std::bad_alloc(); }
		}

		~backlog()
		{
			::free(buf);
		}

		event operator[] (int n) const
		{
			return event(buf[n]);
		}

	private:
		struct kevent* buf;
		friend class kernel;
		backlog(const backlog&);
	};

	int wait(backlog* result)
	{
		return kevent(m_kq, NULL, 0, result->buf,
				MP_WAVY_KERNEL_BACKLOG_SIZE, NULL);
	}

	int wait(backlog* result, int timeout_msec)
	{
		struct timespec ts;
		ts.tv_sec  = timeout_msec / 1000;
		ts.tv_nsec = (timeout_msec % 1000) * 1000000;
		return kevent(m_kq, NULL, 0, result->buf,
				MP_WAVY_KERNEL_BACKLOG_SIZE, &ts);
	}

	int reactivate(event e)
	{
		switch(e.kev.filter) {
		case EVFILT_READ:
			return add_fd(e.ident(), EVFILT_READ);

		case EVFILT_WRITE:
			return add_fd(e.ident(), EVFILT_WRITE);

		case EVFILT_TIMER: {
				unsigned long data = (uintptr_t)e.kev.udata;
				return set_event(e.ident(), EVFILT_TIMER,
						EV_ADD|EV_ONESHOT, 0, data, (void*)data);
			}

		case EVFILT_SIGNAL: {
				int signo = (long)e.kev.ident;
				void* xident = e.kev.udata;
				return set_event(signo, EVFILT_SIGNAL,
						EV_ADD|EV_ONESHOT, 0, 0, xident);
			}

		default:
			return -1;
		}
	}

	int remove(event e)
	{
		switch(e.kev.filter) {
		case EVFILT_READ:
		case EVFILT_WRITE:
			return 0;

		case EVFILT_TIMER:
		case EVFILT_SIGNAL:
			return 0;

		default:
			return -1;
		}
	}

private:
	int m_kq;
	size_t m_fdmax;

	bool m_xident[MP_WAVY_KERNEL_KQUEUE_XIDENT_MAX];
	unsigned int m_xident_index;

	kernel(const kernel&);
};


}  // namespace wavy
}  // namespace mp

#endif /* wavy_kernel_kqueue.h */

