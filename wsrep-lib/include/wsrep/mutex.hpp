/*
 * Copyright (C) 2018 Codership Oy <info@codership.com>
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

#ifndef WSREP_MUTEX_HPP
#define WSREP_MUTEX_HPP

#include "compiler.hpp"
#include "exception.hpp"


#include <pthread.h>

namespace wsrep
{
    /**
     * Mutex interface.
     */
    class mutex
    {
    public:
        mutex() { }
        virtual ~mutex() { }
        virtual void lock() = 0;
        virtual void unlock() = 0;
        /* Return native handle */
        virtual void* native() = 0;
    private:
        mutex(const mutex& other);
        mutex& operator=(const mutex& other);
    };

    // Default pthread implementation
    class default_mutex : public wsrep::mutex
    {
    public:
        default_mutex()
            : wsrep::mutex(),
              mutex_()
        {
            if (pthread_mutex_init(&mutex_, 0))
            {
                throw wsrep::runtime_error("mutex init failed");
            }
        }
        ~default_mutex()
        {
            if (pthread_mutex_destroy(&mutex_)) ::abort();
        }

        void lock() WSREP_OVERRIDE
        {
            if (pthread_mutex_lock(&mutex_))
            {
                throw wsrep::runtime_error("mutex lock failed");
            }
        }

        void unlock() WSREP_OVERRIDE
        {
            if (pthread_mutex_unlock(&mutex_))
            {
                throw wsrep::runtime_error("mutex unlock failed");
            }
        }

        void* native() WSREP_OVERRIDE
        {
            return &mutex_;
        }

    private:
        pthread_mutex_t mutex_;
    };
}

#endif // WSREP_MUTEX_HPP
