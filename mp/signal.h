//
// mpio signal
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
#ifndef MP_SIGNAL_H__
#define MP_SIGNAL_H__

#include "mp/exception.h"
#include "mp/pthread.h"
#include <signal.h>

namespace mp {


class sigset {
public:
	sigset(const sigset_t& set) :
		m_set(set) { }

	sigset()
		{ set_empty(); }

	sigset& add(int signo)
		{ sigaddset(&m_set, signo); return* this; }

	sigset& del(int signo)
		{ sigdelset(&m_set, signo); return* this; }

	sigset& set_empty()
		{ sigemptyset(&m_set); return* this; }

	sigset& set_fill()
		{ sigfillset(&m_set); return* this; }

	sigset_t* get()
	{
		return &m_set;
	}

	const sigset_t* get() const
	{
		return &m_set;
	}

private:
	sigset_t m_set;
};


class scoped_sigprocmask {
public:
	scoped_sigprocmask(const sigset& set) : m_set(set)
	{
		if(sigprocmask(SIG_BLOCK, m_set.get(), NULL) < 0) {
			throw system_error(errno, "failed to set sigprocmask");
		}
	}

	scoped_sigprocmask(const sigset_t& set) : m_set(set)
	{
		if(sigprocmask(SIG_BLOCK, m_set.get(), NULL) < 0) {
			throw system_error(errno, "failed to set sigprocmask");
		}
	}

	~scoped_sigprocmask()
	{
		sigprocmask(SIG_UNBLOCK, m_set.get(), NULL);
	}

	const sigset& get_sigset() const { return m_set; }

private:
	sigset m_set;

private:
	scoped_sigprocmask();
	scoped_sigprocmask(const scoped_sigprocmask&);
};


class scoped_signal {
public:
	scoped_signal(int signo, void (*handler)(int)) :
		m_signo(signo)
	{
		struct sigaction act;
		memset(&act, 0, sizeof(act));
		act.sa_handler = handler;
		sigemptyset(&act.sa_mask);
		act.sa_flags = SA_RESTART;
		if(sigaction(m_signo, &act, &m_save) < 0) {
			throw system_error(errno, "failed to set signal handler");
		}
	}

	~scoped_signal()
	{
		sigaction(m_signo, &m_save, NULL);
	}

	//FIXME
	//scoped_signal(int signo, function<void (int)> func);

private:
	int m_signo;
	struct sigaction m_save;

private:
	scoped_signal();
	scoped_signal(const scoped_signal&);
};


class pthread_signal : public pthread_thread {
public:
	typedef function<bool ()> handler_t;

	pthread_signal(int signo, handler_t handler) :
		m_signal(signo, SIG_IGN),
		m_sigmask(sigset().add(signo))
	{
		run(mp::bind(&pthread_signal::thread_main, signo, handler));
	}

	~pthread_signal() { }

public:
	static void thread_main(int signo, handler_t handler)
	{
		sigset set;
		set.add(signo);
		while(true) {
			if(sigwait(set.get(), &signo) != 0) {
				signo = -1;
			}
			if(!handler()) {
				return;
			}
		}
	}

private:
	scoped_signal m_signal;
	scoped_sigprocmask m_sigmask;

private:
	pthread_signal();
	pthread_signal(const pthread_signal&);
};


}  // namespace mp

#endif /* mp/signal.h */

