//
// mpio endian
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
#ifndef MP_ENDIAN_H__
#define MP_ENDIAN_H__
#ifdef MP_EXPERIMENTAL

#include <arpa/inet.h>
#include <stdint.h>

namespace mp {


#if 0
inline uint16_t htons(uint16_t x) { ::htons(x); }
inline uint16_t ntohs(uint16_t x) { ::ntohs(x); }
inline uint32_t htonl(uint32_t x) { ::htonl(x); }
inline uint32_t ntohl(uint32_t x) { ::ntohl(x); }
#endif


#if defined(__BIG_ENDIAN__) || (!defined(__LITTLE_ENDIAN__) && __BYTE_ORDER == __BIG_ENDIAN)
inline uint64_t htonll(uint64_t x) { return x; }
inline uint64_t ntohll(uint64_t x) { return x; }
#else

#if defined(bswap_64)
inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#elif defined(__DARWIN_OSSwapInt64)
inline uint64_t htonll(uint64_t x) { return __DARWIN_OSSwapInt64(x); }
inline uint64_t ntohll(uint64_t x) { return __DARWIN_OSSwapInt64(x); }
#elif defined(_byteswap_uint64)
inline uint64_t htonll(uint64_t x) { return _byteswap_uint64(x); }
inline uint64_t ntohll(uint64_t x) { return _byteswap_uint64(x); }
#else
inline uint64_t htonll(uint64_t x) {
	return	((x << 56) & 0xff00000000000000ULL ) |
			((x << 40) & 0x00ff000000000000ULL ) |
			((x << 24) & 0x0000ff0000000000ULL ) |
			((x <<  8) & 0x000000ff00000000ULL ) |
			((x >>  8) & 0x00000000ff000000ULL ) |
			((x >> 24) & 0x0000000000ff0000ULL ) |
			((x >> 40) & 0x000000000000ff00ULL ) |
			((x >> 56) & 0x00000000000000ffULL ) ;
}
inline uint64_t ntohll(uint64_t x) { return htonll(x); }
#endif

#endif


}  // namespace mp

#endif /* MP_EXPERIMENTAL */
#endif /* mp/endian.h */

