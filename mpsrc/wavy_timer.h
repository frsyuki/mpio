//
// mpio wavy timer
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
#ifndef DISABLE_TIMERFD
#ifndef WAVY_TIMER_H__
#define WAVY_TIMER_H__

#include "wavy_loop.h"
#include <time.h>

namespace mp {
namespace wavy {
namespace {


struct kernel_timer {
	kernel_timer(kernel& kern, const timespec* value, const timespec* interval)
	{
		if(kern.add_timer(&m_timer, value, interval) < 0) {
			throw system_error(errno, "failed to create timer event");
		}
	}

	~kernel_timer() { }

protected:
	int timer_ident() const
	{
		return m_timer.ident();
	}

	int read_timer(event& e)
	{
		return kernel::read_timer( static_cast<event_impl&>(e).get_kernel_event() );
	}

private:
	kernel::timer m_timer;
};


class timer_handler : public kernel_timer, public basic_handler {
public:
	timer_handler(kernel& kern, const timespec* value, const timespec* interval,
			function<bool ()> callback) :
		kernel_timer(kern, value, interval),
		basic_handler(timer_ident(), this),
		m_periodic(interval && (interval->tv_sec != 0 || interval->tv_nsec != 0)),
		m_callback(callback)
	{ }

	~timer_handler() { }

	bool operator() (event& e)
	{
		read_timer(e);
		return m_callback() && m_periodic;
	}

private:
	bool m_periodic;
	function<bool ()> m_callback;
};


}  // noname namespace
}  // namespace wavy
}  // namespace mp

#endif /* wavy_timer.h */
#endif

