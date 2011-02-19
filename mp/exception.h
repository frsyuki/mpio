//
// mpio exception
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
#ifndef MP_EXCEPTION_H__
#define MP_EXCEPTION_H__

#include <stdexcept>
#include <errno.h>
#include <string.h>
#include <stdio.h>

namespace mp {


struct system_error : std::runtime_error {
	static std::string errno_string(int errno_)
	{
		char buf[512];
#if defined(__linux__)
		char *ret;
		ret = strerror_r(errno_, buf, sizeof(buf)-1);
		return std::string(ret);
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__) || defined(__SunOS__)
		strerror_r(errno_, buf, sizeof(buf)-1);
		return std::string(buf);
#else
		return std::string(strerror(errno_));
#endif
	}

	system_error(int errno_, const std::string& msg) :
		std::runtime_error(msg + ": " + errno_string(errno_)),
		code(errno_) { }

	int code;
};


struct event_error : system_error {
	event_error(int errno_, const std::string& msg) :
		system_error(errno_, msg) {}
};


}  // namespace mp

#endif /* mp/exception.h */

