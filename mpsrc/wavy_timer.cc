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
#include "wavy_timer.h"

namespace mp {
namespace wavy {


int loop::add_timer(const timespec* value, const timespec* interval,
		function<bool ()> callback)
{
	kernel& kern(ANON_impl->get_kernel());

	shared_handler sh(new timer_handler(kern, value, interval, callback));
	ANON_impl->set_handler(sh);

	return sh->ident();
}


static inline struct timespec sec2spec(double sec)
{
	struct timespec spec = {
		sec, ((sec - (double)(time_t)sec) * 1e9) };
	return spec;
}

int loop::add_timer(double value_sec, double interval_sec,
		function<bool ()> callback)
{
	if(value_sec >= 0.0) {
		if(interval_sec > 0.0) {
			struct timespec value = sec2spec(value_sec);
			struct timespec interval = sec2spec(interval_sec);
			return add_timer(&value, &interval, callback);
		} else {
			struct timespec value = sec2spec(value_sec);
			return add_timer(&value, NULL, callback);
		}
	} else {
		if(interval_sec > 0.0) {
			struct timespec interval = sec2spec(interval_sec);
			return add_timer(NULL, &interval, callback);
		} else {
			// FIXME ambiguous overload
			return add_timer(NULL, (const timespec*)NULL, callback);
		}
	}
}


void loop::remove_timer(int ident)
{
	ANON_impl->reset_handler(ident);
	kernel& kern(ANON_impl->get_kernel());
	kern.remove_timer(ident);  // FIXME?
}


}  // namespace wavy
}  // namespace mp
#endif

