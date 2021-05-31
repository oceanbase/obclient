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

#ifndef WSREP_CONDITION_VARIABLE_HPP
#define WSREP_CONDITION_VARIABLE_HPP

#include "compiler.hpp"
#include "lock.hpp"

#include <cstdlib>

namespace wsrep
{
    class condition_variable
    {
    public:
        condition_variable() { }
        virtual ~condition_variable() { }
        virtual void notify_one() = 0;
        virtual void notify_all() = 0;
        virtual void wait(wsrep::unique_lock<wsrep::mutex>& lock) = 0;
    private:
        condition_variable(const condition_variable&);
        condition_variable& operator=(const condition_variable&);
    };

    // Default pthreads based condition variable implementation
    class default_condition_variable : public condition_variable
    {
    public:
        default_condition_variable()
            : cond_()
        {
            if (pthread_cond_init(&cond_, 0))
            {
                throw wsrep::runtime_error("Failed to initialized condvar");
            }
        }

        ~default_condition_variable()
        {
            if (pthread_cond_destroy(&cond_))
            {
                ::abort();
            }
        }
        void notify_one() WSREP_OVERRIDE
        {
            (void)pthread_cond_signal(&cond_);
        }

        void notify_all() WSREP_OVERRIDE
        {
            (void)pthread_cond_broadcast(&cond_);
        }

        void wait(wsrep::unique_lock<wsrep::mutex>& lock) WSREP_OVERRIDE
        {
            if (pthread_cond_wait(
                    &cond_,
                    reinterpret_cast<pthread_mutex_t*>(lock.mutex()->native())))
            {
                throw wsrep::runtime_error("Cond wait failed");
            }
        }

    private:
        pthread_cond_t cond_;
    };

}

#endif // WSREP_CONDITION_VARIABLE_HPP
