//
// mpio pthread
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
#ifndef MP_PTHREAD_H__
#define MP_PTHREAD_H__

#include "mp/exception.h"
#include "mp/functional.h"
#include <pthread.h>
#include <memory>

namespace mp {


struct pthread_error : system_error {
	pthread_error(int errno_, const std::string& msg) :
		system_error(errno_, msg) {}
};


struct pthread_thread {
private:
	typedef function<void ()> function_t;
	
public:
	void create(void* (*func)(void*), void* user)
	{
		int err = pthread_create(&m_thread, NULL,
				func, user);
		if(err) { throw pthread_error(err, "failed to create thread"); }
	}

	void run(function_t func)
	{
		std::auto_ptr<function_t> f(new function_t(func));
		create(&trampoline, reinterpret_cast<void*>(f.get()));
		f.release();
	}

	void detach()
	{
		int err = pthread_detach(m_thread);
		if(err) { throw pthread_error(err, "failed to detach thread"); }
	}

	void* join()
	{
		void* ret;
		int err = pthread_join(m_thread, &ret);
		if(err) { throw pthread_error(err, "failed to join thread"); }
		return ret;
	}

	void cancel()
	{
		pthread_cancel(m_thread);
	}


	bool operator== (const pthread_thread& other) const
	{
		return pthread_equal(m_thread, other.m_thread);
	}

	bool operator!= (const pthread_thread& other) const
	{
		return !(*this == other);
	}


	static void exit(void* retval = NULL)
	{
		pthread_exit(retval);
	}

private:
	pthread_t m_thread;

	static void* trampoline(void* user);
};


template <typename IMPL>
struct pthread_thread_impl : public pthread_thread {
	pthread_thread_impl() : pthread_thread(this) { }
	virtual ~pthread_thread_impl() { }
};


class pthread_mutex {
public:
	pthread_mutex(const pthread_mutexattr_t *attr = NULL)
	{
		pthread_mutex_init(&m_mutex, attr);
	}

	pthread_mutex(int kind)
	{
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, kind);
		pthread_mutex_init(&m_mutex, &attr);
	}

	~pthread_mutex()
	{
		pthread_mutex_destroy(&m_mutex);
	}

public:
	void lock()
	{
		int err = pthread_mutex_lock(&m_mutex);
		if(err != 0) { throw pthread_error(-err, "failed to lock pthread mutex"); }
	}

	bool trylock()
	{
		int err = pthread_mutex_trylock(&m_mutex);
		if(err != 0) {
			if(err == EBUSY) { return false; }
			throw pthread_error(-err, "failed to trylock pthread mutex");
		}
		return true;
	}

	void unlock()
	{
		int err = pthread_mutex_unlock(&m_mutex);
		if(err != 0) { throw pthread_error(-err, "failed to unlock pthread mutex"); }
	}

public:
	pthread_mutex_t* get() { return &m_mutex; }
private:
	pthread_mutex_t m_mutex;
private:
	pthread_mutex(const pthread_mutex&);
};


class pthread_rwlock {
public:
	pthread_rwlock(const pthread_rwlockattr_t *attr = NULL)
	{
		pthread_rwlock_init(&m_mutex, attr);
	}

	// FIXME kind
	//pthread_rwlock(int kind)
	//{
	//	pthread_rwlockattr_t attr;
	//	pthread_rwlockattr_init(&attr);
	//	pthread_rwlockattr_settype(&attr, kind);
	//	pthread_rwlock_init(&m_mutex, &attr);
	//}

	~pthread_rwlock()
	{
		pthread_rwlock_destroy(&m_mutex);
	}

public:
	void rdlock()
	{
		int err = pthread_rwlock_rdlock(&m_mutex);
		if(err != 0) { throw pthread_error(-err, "failed to read lock pthread rwlock"); }
	}

	bool tryrdlock()
	{
		int err = pthread_rwlock_tryrdlock(&m_mutex);
		if(err != 0) {
			if(err == EBUSY) { return false; }
			throw pthread_error(-err, "failed to read trylock pthread rwlock");
		}
		return true;
	}

	void wrlock()
	{
		int err = pthread_rwlock_wrlock(&m_mutex);
		if(err != 0) { throw pthread_error(-err, "failed to write lock pthread rwlock"); }
	}

	bool trywrlock()
	{
		int err = pthread_rwlock_trywrlock(&m_mutex);
		if(err != 0) {
			if(err == EBUSY) { return false; }
			throw pthread_error(-err, "failed to write trylock pthread rwlock");
		}
		return true;
	}

	void unlock()
	{
		int err = pthread_rwlock_unlock(&m_mutex);
		if(err != 0) { throw pthread_error(-err, "failed to unlock pthread rwlock"); }
	}

public:
	pthread_rwlock_t* get() { return &m_mutex; }
private:
	pthread_rwlock_t m_mutex;
private:
	pthread_rwlock(const pthread_rwlock&);
};


class pthread_scoped_lock {
public:
	pthread_scoped_lock() : m_mutex(NULL) { }

	pthread_scoped_lock(pthread_mutex& mutex) : m_mutex(NULL)
	{
		mutex.lock();
		m_mutex = &mutex;
	}

	~pthread_scoped_lock()
	{
		if(m_mutex) {
			m_mutex->unlock();
		}
	}

public:
	void unlock()
	{
		if(m_mutex) {
			m_mutex->unlock();
			m_mutex = NULL;
		}
	}

	void relock(pthread_mutex& mutex)
	{
		unlock();
		mutex.lock();
		m_mutex = &mutex;
	}

	bool owns() const
	{
		return m_mutex != NULL;
	}

private:
	pthread_mutex* m_mutex;
private:
	pthread_scoped_lock(const pthread_scoped_lock&);
};


class pthread_scoped_rdlock {
public:
	pthread_scoped_rdlock() : m_mutex(NULL) { }

	pthread_scoped_rdlock(pthread_rwlock& mutex) : m_mutex(NULL)
	{
		mutex.rdlock();
		m_mutex = &mutex;
	}

	~pthread_scoped_rdlock()
	{
		if(m_mutex) {
			m_mutex->unlock();
		}
	}

public:
	void unlock()
	{
		if(m_mutex) {
			m_mutex->unlock();
			m_mutex = NULL;
		}
	}

	void relock(pthread_rwlock& mutex)
	{
		unlock();
		mutex.rdlock();
		m_mutex = &mutex;
	}

	bool owns() const
	{
		return m_mutex != NULL;
	}

private:
	pthread_rwlock* m_mutex;
private:
	pthread_scoped_rdlock(const pthread_scoped_rdlock&);
};


class pthread_scoped_wrlock {
public:
	pthread_scoped_wrlock() : m_mutex(NULL) { }

	pthread_scoped_wrlock(pthread_rwlock& mutex) : m_mutex(NULL)
	{
		mutex.wrlock();
		m_mutex = &mutex;
	}

	~pthread_scoped_wrlock()
	{
		if(m_mutex) {
			m_mutex->unlock();
		}
	}

public:
	void unlock()
	{
		if(m_mutex) {
			m_mutex->unlock();
			m_mutex = NULL;
		}
	}

	void relock(pthread_rwlock& mutex)
	{
		unlock();
		mutex.wrlock();
		m_mutex = &mutex;
	}

	bool owns() const
	{
		return m_mutex != NULL;
	}

private:
	pthread_rwlock* m_mutex;
private:
	pthread_scoped_wrlock(const pthread_scoped_wrlock&);
};


class pthread_cond {
public:
	pthread_cond(const pthread_condattr_t *attr = NULL)
	{
		pthread_cond_init(&m_cond, attr);
	}

	~pthread_cond()
	{
		pthread_cond_destroy(&m_cond);
	}

public:
	void signal()
	{
		int err = pthread_cond_signal(&m_cond);
		if(err != 0) { throw pthread_error(-err, "failed to signal pthread cond"); }
	}

	void broadcast()
	{
		int err = pthread_cond_broadcast(&m_cond);
		if(err != 0) { throw pthread_error(-err, "failed to broadcast pthread cond"); }
	}

	void wait(pthread_mutex& mutex)
	{
		int err = pthread_cond_wait(&m_cond, mutex.get());
		if(err != 0) { throw pthread_error(-err, "failed to wait pthread cond"); }
	}

	bool timedwait(pthread_mutex& mutex, const struct timespec *abstime)
	{
		int err = pthread_cond_timedwait(&m_cond, mutex.get(), abstime);
		if(err != 0) {
			if(err == ETIMEDOUT) { return false; }
			throw pthread_error(-err, "failed to timedwait pthread cond");
		}
		return true;
	}

public:
	pthread_cond_t* get() { return &m_cond; }
private:
	pthread_cond_t m_cond;
private:
	pthread_cond(const pthread_cond&);
};


}  // namespace mp


#include <iostream>
#include <typeinfo>
#ifndef MP_NO_CXX_ABI_H
#include <cxxabi.h>
#endif

namespace mp {


inline void* pthread_thread::trampoline(void* user)
try {
	std::auto_ptr<function_t> f(reinterpret_cast<function_t*>(user));
	(*f)();
	return NULL;

} catch (std::exception& e) {
	try {
#ifndef MP_NO_CXX_ABI_H
		int status;
		std::cerr
			<< "thread terminated with throwing an instance of '"
			<< abi::__cxa_demangle(typeid(e).name(), 0, 0, &status)
			<< "'\n"
			<< "  what():  " << e.what() << std::endl;

		std::cerr
			<< "thread terminated with throwing an instance of '"
			<< typeid(e).name()
			<< "'\n"
			<< "  what():  " << e.what() << std::endl;
#endif
	} catch (...) {}
	throw;

} catch (...) {
	try {
		std::cerr << "thread terminated with throwing an unknown object" << std::endl;
	} catch (...) {}
	throw;
}


}  // namespace mp

#endif /* mp/pthread.h */

