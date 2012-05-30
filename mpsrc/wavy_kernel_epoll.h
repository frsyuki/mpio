//
// mpio wavy kernel epoll
//
// Copyright (C) 2008-20010 FURUHASHI Sadayuki
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
#ifndef MP_WAVY_KERNEL_EPOLL_H__
#define MP_WAVY_KERNEL_EPOLL_H__

#include "mp/exception.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/resource.h>

#ifndef DISABLE_TIMERFD
#include <sys/timerfd.h>
#endif

#ifndef DISABLE_SIGNALFD
// work around for glibc header signalfd.h error:expected initializer before ‘throw
#if __GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ <= 8)
#undef __THROW
#define __THROW
#endif
#include <sys/signalfd.h>
#endif

namespace mp {
namespace wavy {


static const short EVKERNEL_READ  = EPOLLIN;
static const short EVKERNEL_WRITE = EPOLLOUT;


/**
 * epoll_* based multiplexing kernel
 * 
 * Provides a means for both blocking and non-blocking listening on
 * file descriptora, timers, signals and another kernels.
 * 
 * Note that kernel's can be added to each other in a tree. In such a
 * situation, the FD event triggered by the root of the tree will be for
 * the kernel the level down, *not* the leaf.
 */
class kernel {
public:
	kernel() : m_ep(epoll_create(MP_WAVY_KERNEL_BACKLOG_SIZE))
	{
		if(m_ep < 0) {
			throw system_error(errno, "failed to initialize epoll");
		}
	}

	~kernel()
	{
		::close(m_ep);
	}

	size_t max() const
	{
		struct rlimit rbuf;
		if(::getrlimit(RLIMIT_NOFILE, &rbuf) < 0) {
			throw system_error(errno, "getrlimit() failed");
		}
		return rbuf.rlim_cur;
	}


	class event {
	public:
		event() { }
		explicit event(uint64_t data) : m_data(data) { }
		~event() { }

		int ident() const { return m_data & 0xffffffff; }

	private:
		uint64_t m_data;

		uint64_t data()   const { return m_data; }
		uint32_t events() const { return m_data >> 32; }
		friend class kernel;
	};


	int add_fd(int fd, short event)
	{
		struct epoll_event ev;
		::memset(&ev, 0, sizeof(ev));  // FIXME valgrind
		ev.events = event | EPOLLONESHOT;
		ev.data.u64 = ((uint64_t)fd) | ((uint64_t)ev.events << 32);
		return epoll_ctl(m_ep, EPOLL_CTL_ADD, fd, &ev);
	}

	int remove_fd(int fd, short event)
	{
		return epoll_ctl(m_ep, EPOLL_CTL_DEL, fd, NULL);
	}


#ifndef DISABLE_TIMERFD
	class timer {
	public:
		timer() : fd(-1) { }
		~timer() {
			if(fd >= 0) { ::close(fd); }
		}

		int ident() const { return fd; }

	private:
		int fd;
		friend class kernel;
		timer(const timer&);
	};

	int add_timer(timer* tm, const timespec* value, const timespec* interval)
	{
		int fd = timerfd_create(CLOCK_REALTIME, 0);
		if(fd < 0) {
			return -1;
		}

		if(::fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
			::close(fd);
			return -1;
		}

		struct itimerspec itimer;
		::memset(&itimer, 0, sizeof(itimer));
		if(interval) {
			itimer.it_interval = *interval;
		}
		if(value) {
			itimer.it_value = *value;
		} else {
			itimer.it_value = itimer.it_interval;
		}

		if(timerfd_settime(fd, 0, &itimer, NULL) < 0) {
			::close(fd);
			return -1;
		}

		if(add_fd(fd, EVKERNEL_READ) < 0) {
			::close(fd);
			return -1;
		}

		tm->fd = fd;
		return fd;
	}

	int remove_timer(int ident)
	{
		return remove_fd(ident, EVKERNEL_READ);
	}

	static int read_timer(event e)
	{
		uint64_t exp;
		if(read(e.ident(), &exp, sizeof(uint64_t)) <= 0) {
			return -1;
		}
		return 0;
	}
#else // DISABLE_TIMERFD
	class timer {
	public:
		timer() : fd(-1) { }
		~timer() {
			if(fd >= 0) {
				timer_delete(timer_id);
				::close(fd);
			}
		}

		int ident() const { return fd; }

	private:
		int fd;
		timer_t timer_id;
		friend class kernel;
		timer(const timer&);
	};

	static void timer_thread_handler(sigval_t val)
	{
		int pipefd = val.sival_int;
		const char *p = (const char*)&val;
		const char * const endp = p + sizeof(sigval_t);
		while(p < endp) {
			ssize_t wl = ::write(pipefd, p, endp - p);
			if(wl < 0) {
				if(errno == EINTR || errno == EAGAIN) continue;
				::close(pipefd);
				return;
			}
			p += wl;
		}
	}

	int add_timer(timer* tm, const timespec* value, const timespec* interval)
	{
		int pipefd[2];
		if(pipe(pipefd) < 0) {
			return -1;
		}

		if(::fcntl(pipefd[0], F_SETFL, O_NONBLOCK) < 0) {
			::close(pipefd[1]);
			::close(pipefd[0]);
			return -1;
		}

		struct itimerspec itimer;
		::memset(&itimer, 0, sizeof(itimer));
		if(interval) {
			itimer.it_interval = *interval;
		}
		if(value) {
			itimer.it_value = *value;
		} else {
			itimer.it_value = itimer.it_interval;
		}

		timer_t timer_id;
		struct sigevent ev;
		ev.sigev_notify            = SIGEV_THREAD;
		ev.sigev_value.sival_int   = pipefd[1];
		ev.sigev_notify_function   = kernel::timer_thread_handler;
		ev.sigev_notify_attributes = 0;

		if(timer_create(CLOCK_MONOTONIC, &ev, &timer_id) < 0) {
			::close(pipefd[1]);
			::close(pipefd[0]);
			return -1;
		}

		if(timer_settime(timer_id, 0, &itimer, 0) < 0) {
			::close(pipefd[1]);
			::close(pipefd[0]);
			return -1;
		}

		if(add_fd(pipefd[0], EVKERNEL_READ) < 0) {
			::close(pipefd[1]);
			::close(pipefd[0]);
			return -1;
		}

		tm->fd = pipefd[0];
		tm->timer_id = timer_id;

		return pipefd[0];
	}

	int remove_timer(int ident)
	{
		return remove_fd(ident, EVKERNEL_READ);
	}

	static int read_timer(event e)
	{
		sigval_t val;
		if(read(e.ident(), &val, sizeof(sigval_t)) <= 0) {
			return -1;
		}
		return 0;
	}
#endif


#ifndef DISABLE_SIGNALFD
	class signal {
	public:
		signal() : fd(-1) { }
		~signal() {
			if(fd >= 0) { ::close(fd); }
		}

		int ident() const { return fd; }

	private:
		int fd;
		friend class kernel;
		signal(const signal&);
	};

	int add_signal(signal* sg, int signo)
	{
		sigset_t mask;
		sigemptyset(&mask);
		sigaddset(&mask, signo);

		int fd = signalfd(-1, &mask, 0);
		if(fd < 0) {
			return -1;
		}

		if(::fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
			::close(fd);
			return -1;
		}

		if(add_fd(fd, EVKERNEL_READ) < 0) {
			::close(fd);
			return -1;
		}

		sg->fd = fd;
		return fd;
	}

	int remove_signal(int ident)
	{
		return remove_fd(ident, EVKERNEL_READ);
	}

	static int read_signal(event e)
	{
		signalfd_siginfo info;
		if(read(e.ident(), &info, sizeof(info)) <= 0) {
			return -1;
		}
		return 0;
	}
#endif


	int add_kernel(kernel* kern)
	{
		if(add_fd(kern->m_ep, EVKERNEL_READ) < 0) {
			return -1;
		}
		return kern->m_ep;
	}

	int ident() const
	{
		return m_ep;
	}


	class backlog {
	public:
		backlog()
		{
			buf = (struct epoll_event*)::calloc(
					sizeof(struct epoll_event),
					MP_WAVY_KERNEL_BACKLOG_SIZE);
			if(!buf) { throw std::bad_alloc(); }
		}

		~backlog()
		{
			::free(buf);
		}

		event operator[] (int n)
		{
			return event(buf[n].data.u64);
		}

	private:
		struct epoll_event* buf;
		friend class kernel;
		backlog(const backlog&);
	};

	int wait(backlog* result)
	{
		return wait(result, -1);
	}

	int wait(backlog* result, int timeout_msec)
	{
		return epoll_wait(m_ep, result->buf,
				MP_WAVY_KERNEL_BACKLOG_SIZE, timeout_msec);
	}

	int reactivate(event e)
	{
		struct epoll_event ev;
		::memset(&ev, 0, sizeof(ev));  // FIXME valgrind
		ev.events = e.events();
		ev.data.u64 = e.data();
		return epoll_ctl(m_ep, EPOLL_CTL_MOD, e.ident(), &ev);
	}

	int remove(event e)
	{
		return epoll_ctl(m_ep, EPOLL_CTL_DEL, e.ident(), NULL);
	}

private:
	int m_ep;

private:
	kernel(const kernel&);
};


}  // namespace wavy
}  // namespace mp

#endif /* wavy_kernel_epoll.h */

