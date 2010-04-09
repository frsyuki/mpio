//
// mpio utilize
//
// Copyright (C) 2009-2010 FURUHASHI Sadayuki
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
#ifndef MP_UTILIZE_H__
#define MP_UTILIZE_H__


#define MP_UTILIZE \
	struct mp_util; \
	friend struct mp_util

#define MP_UTIL_DEF(name) \
	struct name::mp_util : public name

#define MP_UTIL_IMPL(name) \
	name::mp_util

#define MP_UTIL \
	(*static_cast<mp_util*>(this))

#define MP_UTIL_FROM(self) \
	(*static_cast<mp_util*>(self))


#endif /* mp/utilze.h */

