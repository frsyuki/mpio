//
// mpio wavy signal
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
#ifndef DISABLE_SIGNALFD
#include "wavy_signal.h"

namespace mp {
namespace wavy {


int loop::add_signal(int signo, function<bool ()> callback)
{
	kernel& kern(ANON_impl->get_kernel());

	shared_handler sh(new signal_handler(kern, signo, callback));
	ANON_impl->set_handler(sh);

	return sh->ident();
}


void loop::remove_signal(int ident)
{
	kernel& kern(ANON_impl->get_kernel());
	kern.remove_signal(ident);
	ANON_impl->reset_handler(ident);  // FIXME?
}


}  // namespace wavy
}  // namespace mp
#endif

