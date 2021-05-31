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

#ifndef WSREP_ENCRYPTION_SERVICE_HPP
#define WSREP_ENCRYPTION_SERVICE_HPP

#include "buffer.hpp"

namespace wsrep
{
    /**
     * Encryption service.
     */
    class encryption_service
    {
    public:

        virtual ~encryption_service() { }

        /**
         * Encryption/decryption callback. Can be NULL for no encryption.
         *
         * @param ctx       Encryption context
         * @param key       Key used in encryption/decryption
         * @param iv        IV vector
         * @param input     Input data buffer
         * @param output    An output buffer, must be at least the size of the input
         *                  data plus unwritten bytes from the previous call(s).
         * @param encrypt   Flag used to either encrypt or decrypt data
         * @param last      true if this is the last buffer to encrypt/decrypt
         *
         * @return          Number of bytes written to output or a negative error code.
         */
        virtual int do_crypt(void**                ctx,
                             wsrep::const_buffer&  key,
                             const char            (*iv)[32],
                             wsrep::const_buffer&  input,
                             void*                 output,
                             bool                  encrypt,
                             bool                  last) = 0;

        /**
         * Is encryption enabled on server.
         * 
         * @return True if encryption is enabled. False otherwise
         */
        virtual bool encryption_enabled() = 0;
    };
}

#endif // WSREP_ENCRYPTION_SERVICE_HPP
