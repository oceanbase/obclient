/*
 * Copyright (C) 2019 Codership Oy <info@codership.com>
 *
 * This file is part of wsrep-lib.
 *
 * Wsrep-lib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Wsrep-lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wsrep-lib.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "thread_service_v1.hpp"

#include "wsrep/thread_service.hpp"
#include "wsrep/logger.hpp"
#include "v26/wsrep_thread_service.h"

#include <cassert>
#include <dlfcn.h>
#include <cerrno>

namespace wsrep_thread_service_v1
{
    //
    // Thread service callbacks
    //

    // Pointer to thread service implementation provided by
    // the application.
    static wsrep::thread_service* thread_service_impl{ 0 };

    static const wsrep_thread_key_t* thread_key_create_cb(const char* name)
    {
        assert(thread_service_impl);
        return reinterpret_cast<const wsrep_thread_key_t*>(
            thread_service_impl->create_thread_key(name));
        ;
    }

    static int thread_create_cb(const wsrep_thread_key_t* key,
                                wsrep_thread_t** thread,
                                void* (*fn)(void*), void* args)
    {
        assert(thread_service_impl);
        return thread_service_impl->create_thread(
            reinterpret_cast<const wsrep::thread_service::thread_key*>(key),
            reinterpret_cast<wsrep::thread_service::thread**>(thread), fn,
            args);
    }

    int thread_detach_cb(wsrep_thread_t* thread)
    {
        assert(thread_service_impl);
        return thread_service_impl->detach(
            reinterpret_cast<wsrep::thread_service::thread*>(thread));
    }

    int thread_equal_cb(wsrep_thread_t* thread_1, wsrep_thread_t* thread_2)
    {
        assert(thread_service_impl);
        return thread_service_impl->equal(
            reinterpret_cast<wsrep::thread_service::thread*>(thread_1),
            reinterpret_cast<wsrep::thread_service::thread*>(thread_2));
    }

    __attribute__((noreturn))
    void thread_exit_cb(wsrep_thread_t* thread, void* retval)
    {
        assert(thread_service_impl);
        thread_service_impl->exit(
            reinterpret_cast<wsrep::thread_service::thread*>(thread), retval);
        throw; // Implementation broke the contract and returned.
    }

    int thread_join_cb(wsrep_thread_t* thread, void** retval)
    {
        assert(thread_service_impl);
        return thread_service_impl->join(
            reinterpret_cast<wsrep::thread_service::thread*>(thread), retval);
    }

    wsrep_thread_t* thread_self_cb(void)
    {
        assert(thread_service_impl);
        return reinterpret_cast<wsrep_thread_t*>(thread_service_impl->self());
    }

    int thread_setschedparam_cb(wsrep_thread_t* thread, int policy,
                                const struct sched_param* sp)
    {
        assert(thread_service_impl);
        return thread_service_impl->setschedparam(
            reinterpret_cast<wsrep::thread_service::thread*>(thread),
            policy, sp);
    }

    int thread_getschedparam_cb(wsrep_thread_t* thread, int* policy,
                                struct sched_param* sp)
    {
        assert(thread_service_impl);
        return thread_service_impl->getschedparam(
            reinterpret_cast<wsrep::thread_service::thread*>(thread), policy,
            sp);
    }

    const wsrep_mutex_key_t* mutex_key_create_cb(const char* name)
    {
        assert(thread_service_impl);
        return reinterpret_cast<const wsrep_mutex_key_t*>(
            thread_service_impl->create_mutex_key(name));
    }

    wsrep_mutex_t* mutex_init_cb(const wsrep_mutex_key_t* key, void* memblock,
                                 size_t memblock_size)
    {
        assert(thread_service_impl);
        return reinterpret_cast<wsrep_mutex_t*>(
            thread_service_impl->init_mutex(
                reinterpret_cast<const wsrep::thread_service::mutex_key*>(key),
                memblock, memblock_size));
    }

    int mutex_destroy_cb(wsrep_mutex_t* mutex)
    {
        assert(thread_service_impl);
        return thread_service_impl->destroy(
            reinterpret_cast<wsrep::thread_service::mutex*>(mutex));
    }
    int mutex_lock_cb(wsrep_mutex_t* mutex)
    {
        assert(thread_service_impl);
        return thread_service_impl->lock(
            reinterpret_cast<wsrep::thread_service::mutex*>(mutex));
    }

    int mutex_trylock_cb(wsrep_mutex_t* mutex)
    {
        assert(thread_service_impl);
        return thread_service_impl->trylock(
            reinterpret_cast<wsrep::thread_service::mutex*>(mutex));
    }

    int mutex_unlock_cb(wsrep_mutex_t* mutex)
    {
        assert(thread_service_impl);
        return thread_service_impl->unlock(
            reinterpret_cast<wsrep::thread_service::mutex*>(mutex));
    }

    const wsrep_cond_key_t* cond_key_create_cb(const char* name)
    {
        assert(thread_service_impl);
        return reinterpret_cast<const wsrep_cond_key_t*>(
            thread_service_impl->create_cond_key(name));
    }

    wsrep_cond_t* cond_init_cb(const wsrep_cond_key_t* key, void* memblock,
                               size_t memblock_size)
    {
        assert(thread_service_impl);
        return reinterpret_cast<wsrep_cond_t*>(thread_service_impl->init_cond(
            reinterpret_cast<const wsrep::thread_service::cond_key*>(key),
            memblock, memblock_size));
    }

    int cond_destroy_cb(wsrep_cond_t* cond)
    {
        assert(thread_service_impl);
        return thread_service_impl->destroy(
            reinterpret_cast<wsrep::thread_service::cond*>(cond));
    }

    int cond_wait_cb(wsrep_cond_t* cond, wsrep_mutex_t* mutex)
    {
        assert(thread_service_impl);
        return thread_service_impl->wait(
            reinterpret_cast<wsrep::thread_service::cond*>(cond),
            reinterpret_cast<wsrep::thread_service::mutex*>(mutex));
    }

    int cond_timedwait_cb(wsrep_cond_t* cond, wsrep_mutex_t* mutex,
                          const struct timespec* ts)
    {
        assert(thread_service_impl);
        return thread_service_impl->timedwait(
            reinterpret_cast<wsrep::thread_service::cond*>(cond),
            reinterpret_cast<wsrep::thread_service::mutex*>(mutex), ts);
    }

    int cond_signal_cb(wsrep_cond_t* cond)
    {
        assert(thread_service_impl);
        return thread_service_impl->signal(
            reinterpret_cast<wsrep::thread_service::cond*>(cond));
    }

    int cond_broadcast_cb(wsrep_cond_t* cond)
    {
        assert(thread_service_impl);
        return thread_service_impl->broadcast(
            reinterpret_cast<wsrep::thread_service::cond*>(cond));
    }

    static wsrep_thread_service_v1_t thread_service_callbacks
        = { thread_key_create_cb,
            thread_create_cb,
            thread_detach_cb,
            thread_equal_cb,
            thread_exit_cb,
            thread_join_cb,
            thread_self_cb,
            thread_setschedparam_cb,
            thread_getschedparam_cb,
            mutex_key_create_cb,
            mutex_init_cb,
            mutex_destroy_cb,
            mutex_lock_cb,
            mutex_trylock_cb,
            mutex_unlock_cb,
            cond_key_create_cb,
            cond_init_cb,
            cond_destroy_cb,
            cond_wait_cb,
            cond_timedwait_cb,
            cond_signal_cb,
            cond_broadcast_cb };
}

int wsrep::thread_service_v1_probe(void* dlh)
{
   typedef int (*init_fn)(wsrep_thread_service_v1_t*);
    union {
        init_fn dlfun;
        void* obj;
    } alias;
    // Clear previous errors
    (void)dlerror();
    alias.obj = dlsym(dlh, WSREP_THREAD_SERVICE_INIT_FUNC);
    if (alias.obj)
    {
        wsrep::log_info()
            << "Found support for thread service v1 from provider";
        return 0;
    }
    else
    {
        wsrep::log_info() << "Thread service v1 not found from provider: "
                          << dlerror();
        return ENOTSUP;
    }

}

int wsrep::thread_service_v1_init(void* dlh,
                                  wsrep::thread_service* thread_service)
{
    if (not (dlh && thread_service)) return EINVAL;

    typedef int (*init_fn)(wsrep_thread_service_v1_t*);
    union {
        init_fn dlfun;
        void* obj;
    } alias;
    alias.obj = dlsym(dlh, WSREP_THREAD_SERVICE_INIT_FUNC);
    if (alias.obj)
    {
        wsrep::log_info() << "Initializing process instrumentation";
        wsrep_thread_service_v1::thread_service_impl = thread_service;
        return (*alias.dlfun)(&wsrep_thread_service_v1::thread_service_callbacks);
    }
    else
    {
        wsrep::log_info()
            << "Provider does not support process instrumentation";
        return ENOTSUP;
    }
}
