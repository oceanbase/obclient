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

#ifndef WSREP_EXCEPTION_HPP
#define WSREP_EXCEPTION_HPP

#include <stdexcept>
#include <cstdlib>

namespace wsrep
{
    extern bool abort_on_exception;

    class runtime_error : public std::runtime_error
    {
    public:
        runtime_error(const char* msg)
            : std::runtime_error(msg)
        {
            if (abort_on_exception)
            {
                ::abort();
            }
        }

        runtime_error(const std::string& msg)
            : std::runtime_error(msg)
        {
            if (abort_on_exception)
            {
                ::abort();
            }
        }
    };

    class not_implemented_error : public std::exception
    {
    public:
        not_implemented_error()
            : std::exception()
        {
            ::abort();
        }
    };

    class fatal_error : public std::exception
    {
    };
}


#endif // WSREP_EXCEPTION_HPP
