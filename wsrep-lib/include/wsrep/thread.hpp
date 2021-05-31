/*
 * Copyright (C) 2018-2019 Codership Oy <info@codership.com>
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

#include <pthread.h>
#include <iosfwd>

namespace wsrep
{
    class thread
    {
    public:
        class id
        {
        public:
            id() : thread_() { }
            explicit id(pthread_t thread) : thread_(thread) { }
        private:
            friend bool operator==(thread::id left, thread::id right)
            {
                return (pthread_equal(left.thread_, right.thread_));
            }
            friend std::ostream& operator<<(std::ostream&, const id&);
            pthread_t thread_;
        };

        thread()
            : id_(pthread_self())
        { }
    private:
        id id_;
    };

    namespace this_thread
    {
        static inline thread::id get_id() { return thread::id(pthread_self()); }
    }

    std::ostream& operator<<(std::ostream&, const thread::id&);
};
