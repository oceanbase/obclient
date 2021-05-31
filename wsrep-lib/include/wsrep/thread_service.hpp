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


/** @file thread_service.hpp
 *
 * Service interface for threads and synchronization primitives.
 * The purpose of this interface is to provider provider implementations
 * means to integrate with application thread implementation.
 *
 * Interface is designed to resemble POSIX threads, mutexes and
 * condition variables.
 */

#include <cstddef> // size_t
#include "compiler.hpp"

struct timespec;
struct sched_param;

namespace wsrep
{

    class thread_service
    {
    public:

        virtual ~thread_service() { }
        class thread_key { };
        class thread { };
        class mutex_key { };
        class mutex { };
        class cond_key { };
        class cond { };

        /**
         * Method will be called before library side thread
         * service initialization.
         */
        virtual int before_init() = 0;

        /**
         * Method will be called after library side thread service
         * has been initialized.
         */
        virtual int after_init() = 0;

        /* Thread */
        virtual const thread_key* create_thread_key(const char* name) WSREP_NOEXCEPT
            = 0;
        virtual int create_thread(const thread_key*, thread**,
                                  void* (*fn)(void*), void*) WSREP_NOEXCEPT
            = 0;
        virtual int detach(thread*) WSREP_NOEXCEPT = 0;
        virtual int equal(thread*, thread*) WSREP_NOEXCEPT = 0;
        WSREP_NORETURN virtual void exit(thread*, void* retval) WSREP_NOEXCEPT = 0;
        virtual int join(thread*, void** retval) WSREP_NOEXCEPT = 0;
        virtual thread* self() WSREP_NOEXCEPT = 0;
        virtual int setschedparam(thread*, int,
                                  const struct sched_param*) WSREP_NOEXCEPT
            = 0;
        virtual int getschedparam(thread*, int*, struct sched_param*) WSREP_NOEXCEPT
            = 0;

        /* Mutex */
        virtual const mutex_key* create_mutex_key(const char* name) WSREP_NOEXCEPT
            = 0;
        virtual mutex* init_mutex(const mutex_key*, void*, size_t) WSREP_NOEXCEPT = 0;
        virtual int destroy(mutex*) WSREP_NOEXCEPT = 0;
        virtual int lock(mutex*) WSREP_NOEXCEPT = 0;
        virtual int trylock(mutex*) WSREP_NOEXCEPT = 0;
        virtual int unlock(mutex*) WSREP_NOEXCEPT = 0;

        /* Condition variable */
        virtual const cond_key* create_cond_key(const char* name) WSREP_NOEXCEPT = 0;
        virtual cond* init_cond(const cond_key*, void*, size_t) WSREP_NOEXCEPT = 0;
        virtual int destroy(cond*) WSREP_NOEXCEPT = 0;
        virtual int wait(cond*, mutex*) WSREP_NOEXCEPT = 0;
        virtual int timedwait(cond*, mutex*, const struct timespec*) WSREP_NOEXCEPT
            = 0;
        virtual int signal(cond*) WSREP_NOEXCEPT = 0;
        virtual int broadcast(cond*) WSREP_NOEXCEPT = 0;
    };
} // namespace wsrep
