mpio
====
Multipurpose concurrent I/O framework for C++


## Overview


## Installation

Following libraries are required to build mpio:

  - OS
    - Linux >= 2.6.25 + glibc >= 2.8
	- Mac OS X >= 10.5 Leopard
	- FreeBSD >= ?
	- NetBSD >= ?
  - g++ >= 4.1

Configure and install in the usual way:

    $ ./bootstrap  # if needed
    $ ./configure
    $ make
    $ sudo make install

For OS X, possibly replace `make` by:

    $ make CPPFLAGS='-DMP_FUNCTIONAL_BOOST -DMP_MEMORY_BOOST -I/usr/local/include'



## Libraries

[Test cases](http://github.com/frsyuki/mpio/tree/master/test/) will give you a sample usage.

  - [event handler](http://github.com/frsyuki/mpio/blob/master/test/handler.cc)
  - [listen and connect](http://github.com/frsyuki/mpio/blob/master/test/listen_connect.cc)
  - [timer](http://github.com/frsyuki/mpio/blob/master/test/timer.cc)
  - [signal handling](http://github.com/frsyuki/mpio/blob/master/test/signal.cc)
  - [mp::sync](http://github.com/frsyuki/mpio/blob/master/test/sync.cc)


### Wavy
Wavy is a multithreaded event-driven I/O library.

### sync

### utilize

### shared_buffer

### stream_buffer

### sparse_array

### pthread

### signal

### functional

### memory


## License

    Copyright (C) 2008-2010 FURUHASHI Sadayuki
    
       Licensed under the Apache License, Version 2.0 (the "License");
       you may not use this file except in compliance with the License.
       You may obtain a copy of the License at
    
           http://www.apache.org/licenses/LICENSE-2.0
    
       Unless required by applicable law or agreed to in writing, software
       distributed under the License is distributed on an "AS IS" BASIS,
       WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
       See the License for the specific language governing permissions and
       limitations under the License.

See also NOTICE file.

