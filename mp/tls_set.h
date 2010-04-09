//
// mpio tls_set
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
#ifndef MP_TLS_SET_H__
#define MP_TLS_SET_H__
#ifdef MP_EXPERIMENTAL

#include "mp/pthread.h"
#include "mp/sync.h"
#include <memory>
#include <vector>

namespace mp {


class pthread_scoped_lock_multi {
public:
	pthread_scoped_lock_multi() : m_vec(NULL) { }

	pthread_scoped_lock_multi(size_t size) :
		m_vec(new pthread_scoped_lock[size]) { }

	~pthread_scoped_lock_multi() { delete[] m_vec; }

	void reset(size_t size)
	{
		if(m_vec) { delete[] m_vec; }
		m_vec = NULL;
		m_vec = new pthread_scoped_lock[size];
	}

	pthread_scoped_lock& operator[] (size_t index)
	{
		return m_vec[index];
	}

	const pthread_scoped_lock& operator[] (size_t index) const
	{
		return m_vec[index];
	}

private:
	pthread_scoped_lock* m_vec;

private:
	pthread_scoped_lock_multi(const pthread_scoped_lock_multi&);
};


template <typename T>
class tls_set {
public:
	tls_set()
	{
		int err = pthread_key_create(&m_key, &mp::object_destructor<T>);
		if(err) { throw pthread_error(err, "failed to create TLS key"); }
	}

	~tls_set()
	{ }

	void init_thread(const T& data = T())
	{
		all_vec_ref ref(m_all_vec);

		std::auto_ptr<element> e(new element(data));

		ref->push_back(e.get());

		int err = pthread_setspecific(m_key, (void*)e.get());
		if(err) {
			ref->pop_back();
			throw pthread_error(err, "failed to init TLS");
		}

		e.release();
	}

	void update_self(const T& data)
	{
		element* e = (element*)pthread_getspecific(m_key);
		pthread_scoped_lock lk(e->mutex);
		e->data = data;
	}

	T& get_self(pthread_scoped_lock* lk)
	{
		element* e = (element*)pthread_getspecific(m_key);
		lk->relock(e->mutex);
		return e->data;
	}

	T& get_self(const pthread_scoped_lock_multi& mlk)
	{
		element* e = (element*)pthread_getspecific(m_key);
		return e->data;
	}

	void update_all(const T& data)
	{
		all_vec_ref ref(m_all_vec);
		for(typename all_vec_t::iterator it(ref->begin()),
				it_end(ref->end()); it != it_end; ++it) {
			pthread_scoped_lock lk(it->mutex);
			it->data = data;
		}
	}

	template <typename F>
	void get_all(F func)
	{
		all_vec_ref ref(m_all_vec);
		for(typename all_vec_t::iterator it(ref->begin()),
				it_end(ref->end()); it != it_end; ++it) {
			pthread_scoped_lock lk(it->mutex);
			func(it->data);
		}
	}

	void update_all(const T& data, const pthread_scoped_lock_multi& mlk)
	{
		all_vec_ref ref(m_all_vec);
		for(typename all_vec_t::iterator it(ref->begin()),
				it_end(ref->end()); it != it_end; ++it) {
			it->data = data;
		}
	}

	template <typename F>
	void get_all(F func, const pthread_scoped_lock_multi& mlk)
	{
		all_vec_ref ref(m_all_vec);
		for(typename all_vec_t::iterator it(ref->begin()),
				it_end(ref->end()); it != it_end; ++it) {
			func(it->data);
		}
	}

	void lock_all(pthread_scoped_lock_multi* mlk)
	{
		all_vec_ref ref(m_all_vec);
		mlk->reset(ref->size());
		for(size_t i=0; i < ref->size(); ++i) {
			(*mlk)[i].relock((*ref)[i]->mutex);
		}
	}

private:
	struct element {
		element(const T& data) : data(data) { }
		~element() { }
		T data;
		pthread_mutex mutex;
	};

	pthread_key_t m_key;

	typedef std::vector<element*> all_vec_t;
	typedef typename mp::sync<all_vec_t>::ref all_vec_ref;
	mp::sync<all_vec_t> m_all_vec;
};


}  // namespace mp

#endif /* MP_EXPERIMENTAL */
#endif /* mp/tls_set.h */

