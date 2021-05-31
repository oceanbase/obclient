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

/** @file chrono.hpp
 *
 * Type definitions to work around GCC 4.4 incompatibilities with
 * C++11 chrono.
 */

#ifndef WSREP_CHRONO_HPP
#define WSREP_CHRONO_HPP

#include <chrono>

namespace wsrep
{
    /* wsrep::clock - clock type compatible with std::chrono::steady_clock. */
#if defined(__GNUG__) && (__GNUC__ == 4 && __GNUC_MINOR__ == 4)
    typedef std::chrono::monotonic_clock clock;
#else
    using clock = std::chrono::steady_clock;
#endif // defined(__GNUG__) && (__GNUC__ == 4 && __GNUC_MINOR__ == 4)

}

#endif // WSREP_CHRONO_HPP
