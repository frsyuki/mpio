//
// mp::unordered
//
// Copyright (C) 2010 FURUHASHI Sadayuki
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

#ifndef MP_UNORDERED_H__
#define MP_UNORDERED_H__

#if   defined(MP_UNORDERED_MAP_BOOST)
#include <boost/tr1/unordered_map.hpp>
#include <boost/tr1/unordered_set.hpp>
#include <boost/tr1/functional.hpp>
namespace mp {
	using std::tr1::unordered_map;
	using std::tr1::unordered_set;
	using std::tr1::unordered_multimap;
	using std::tr1::unordered_multiset;
	//using std::tr1::hash_range;
	//using std::tr1::hash_combine;
	template <typename T> struct hash : public std::tr1::hash<T> { };
}

#elif defined(MP_UNORDERED_MAP_BOOST_ORG)
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
namespace mp {
	using boost::unordered_map;
	using boost::unordered_set;
	using boost::unordered_multimap;
	using boost::unordered_multiset;
	template <typename T> struct hash : public boost::hash<T> { };
}

#elif defined(MP_UNORDERED_MAP_STANDARD)
#include <unordered_map>
#include <unordered_set>
namespace mp {
	using std::unordered_map;
	using std::unordered_set;
	using std::unordered_multimap;
	using std::unordered_multiset;
	template <typename T> struct hash : public std::hash<T> { };
}

#else
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include <tr1/functional>
namespace mp {
	using std::tr1::unordered_map;
	using std::tr1::unordered_set;
	using std::tr1::unordered_multimap;
	using std::tr1::unordered_multiset;
	template <typename T> struct hash : public std::tr1::hash<T> { };
}
#endif

#endif /* mp/unordered.h */

