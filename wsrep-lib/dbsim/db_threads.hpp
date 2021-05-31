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

#ifndef WSREP_DB_THREADS_HPP
#define WSREP_DB_THREADS_HPP

#include "wsrep/thread_service.hpp"
#include <string>

namespace db
{
    class ti : public wsrep::thread_service
    {
    public:
        ti() { }
        int before_init() override;
        int after_init() override;

        /* Thread */
        const wsrep::thread_service::thread_key*
        create_thread_key(const char* name) WSREP_NOEXCEPT override;
        int create_thread(const wsrep::thread_service::thread_key* key,
                          wsrep::thread_service::thread**,
                          void* (*fn)(void*), void*) WSREP_NOEXCEPT override;
        int detach(wsrep::thread_service::thread*) WSREP_NOEXCEPT override;
        int equal(wsrep::thread_service::thread*,
                  wsrep::thread_service::thread*) WSREP_NOEXCEPT override;
        void exit(wsrep::thread_service::thread*, void*) WSREP_NOEXCEPT override;
        int join(wsrep::thread_service::thread*, void**) WSREP_NOEXCEPT override;
        wsrep::thread_service::thread* self() WSREP_NOEXCEPT override;
        int setschedparam(wsrep::thread_service::thread*, int,
                          const struct sched_param*) WSREP_NOEXCEPT override;
        int getschedparam(wsrep::thread_service::thread*, int*,
                          struct sched_param*) WSREP_NOEXCEPT override;

        /* Mutex */
        const wsrep::thread_service::mutex_key*
        create_mutex_key(const char* name) WSREP_NOEXCEPT override;
        wsrep::thread_service::mutex*
        init_mutex(const wsrep::thread_service::mutex_key* key, void* memblock,
                   size_t memblock_size) WSREP_NOEXCEPT override;
        int destroy(wsrep::thread_service::mutex* mutex) WSREP_NOEXCEPT override;
        int lock(wsrep::thread_service::mutex* mutex) WSREP_NOEXCEPT override;
        int trylock(wsrep::thread_service::mutex* mutex) WSREP_NOEXCEPT override;
        int unlock(wsrep::thread_service::mutex* mutex) WSREP_NOEXCEPT override;
        /* Cond */
        const wsrep::thread_service::cond_key*
        create_cond_key(const char* name) WSREP_NOEXCEPT override;
        wsrep::thread_service::cond*
        init_cond(const wsrep::thread_service::cond_key* key, void* memblock,
                  size_t memblock_size) WSREP_NOEXCEPT override;
        int destroy(wsrep::thread_service::cond* cond) WSREP_NOEXCEPT override;
        int wait(wsrep::thread_service::cond* cond,
                 wsrep::thread_service::mutex* mutex) WSREP_NOEXCEPT override;
        int timedwait(wsrep::thread_service::cond* cond,
                      wsrep::thread_service::mutex* mutex,
                      const struct timespec* ts) WSREP_NOEXCEPT override;
        int signal(wsrep::thread_service::cond* cond) WSREP_NOEXCEPT override;
        int broadcast(wsrep::thread_service::cond* cond) WSREP_NOEXCEPT override;

        static void level(int level);
        static void cond_checks(bool cond_checks);
        static std::string stats();
    };


}

#endif // WSREP_DB_THREADS_HPP
