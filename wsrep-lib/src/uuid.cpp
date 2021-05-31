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

#include "uuid.hpp"

#include <cstring>
#include <cerrno>
#include <cstdio>
#include <cctype>

int wsrep::uuid_scan (const char* str, size_t str_len, wsrep::uuid_t* uuid)
{
    unsigned int uuid_len  = 0;
    unsigned int uuid_offt = 0;

    while (uuid_len + 1 < str_len) {
        /* We are skipping potential '-' after uuid_offt == 4, 6, 8, 10
         * which means
         *     (uuid_offt >> 1) == 2, 3, 4, 5,
         * which in turn means
         *     (uuid_offt >> 1) - 2 <= 3
         * since it is always >= 0, because uuid_offt is unsigned */
        if (((uuid_offt >> 1) - 2) <= 3 && str[uuid_len] == '-') {
            // skip dashes after 4th, 6th, 8th and 10th positions
            uuid_len += 1;
            continue;
        }

        if (isxdigit(str[uuid_len]) && isxdigit(str[uuid_len + 1])) {
            // got hex digit, scan another byte to uuid, increment uuid_offt
            sscanf (str + uuid_len, "%2hhx", uuid->data + uuid_offt);
            uuid_len  += 2;
            uuid_offt += 1;
            if (sizeof (uuid->data) == uuid_offt)
                return static_cast<int>(uuid_len);
        }
        else {
            break;
        }
    }

    *uuid = wsrep::uuid_initializer;
    return -EINVAL;
}

int wsrep::uuid_print (const wsrep::uuid_t* uuid, char* str, size_t str_len)
{
    if (str_len > WSREP_LIB_UUID_STR_LEN) {
        const unsigned char* u = uuid->data;
        return snprintf(str, str_len, "%02x%02x%02x%02x-%02x%02x-%02x%02x-"
                        "%02x%02x-%02x%02x%02x%02x%02x%02x",
                        u[ 0], u[ 1], u[ 2], u[ 3], u[ 4], u[ 5], u[ 6], u[ 7],
                        u[ 8], u[ 9], u[10], u[11], u[12], u[13], u[14], u[15]);
    }
    else {
        return -EMSGSIZE;
    }
}
