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

/** @file uuid.hpp
 *
 * Helper methods to parse and print UUIDs, intended to use
 * internally in wsrep-lib.
 *
 * The implementation is copied from wsrep-API v26.
 */

#ifndef WSREP_UUID_HPP
#define WSREP_UUID_HPP

#include "wsrep/compiler.hpp"

#include <cstddef>

/**
 * Length of UUID string representation, not including terminating '\0'.
 */
#define WSREP_LIB_UUID_STR_LEN 36

namespace wsrep
{
    /**
     * UUID type.
     */
    typedef union uuid_
    {
        unsigned char data[16];
        size_t alignment;
    } uuid_t;

    static const wsrep::uuid_t uuid_initializer = {{0, }};

    /**
     * Read UUID from string.
     *
     * @param str String to read from
     * @param str_len Length of string
     * @param[out] UUID to read to
     *
     * @return Number of bytes read or negative error code in case
     *         of error.
     */
    int uuid_scan(const char* str, size_t str_len, wsrep::uuid_t* uuid);

    /**
     * Write UUID to string. The caller must allocate at least
     * WSREP_LIB_UUID_STR_LEN + 1 space for the output str parameter.
     *
     * @param uuid UUID to print
     * @param str[out] Output buffer
     * @param str_len Size of output buffer
     *
     * @return Number of chars printerd, negative error code in case of
     *         error.
     */
    int uuid_print(const wsrep::uuid_t* uuid, char* str, size_t str_len);
}

#endif // WSREP_UUID_HPP
