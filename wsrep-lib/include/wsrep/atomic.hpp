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

#ifndef WSREP_ATOMIC_HPP
#define WSREP_ATOMIC_HPP

#if defined(__GNUG__) && (__GNUC__ == 4 && __GNUC_MINOR__ == 4)
#include <cstdatomic>
#else
#include <atomic>
#endif // defined(__GNUG__) && (__GNUC__ == 4 && __GNUC_MINOR__ == 4)

#endif // WSREP_ATOMIC_HPP
