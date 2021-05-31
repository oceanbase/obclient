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

#ifndef WSREP_KEY_HPP
#define WSREP_KEY_HPP

#include "exception.hpp"
#include "buffer.hpp"

#include <iosfwd>

namespace wsrep
{
    /** @class key
     *
     * Certification key type.
     */
    class key
    {
    public:
        enum type
        {
            shared,
            reference,
            update,
            exclusive
        };

        key(enum type type)
            : type_(type)
            , key_parts_()
            , key_parts_len_()
        { }

        /**
         * Append key part to key.
         *
         * @param ptr Pointer to key part data. The caller is supposed to take
         *            care that the pointer remains valid over the lifetime
         *            if the key object.
         * @param len Length of the key part data.
         */
        void append_key_part(const void* ptr, size_t len)
        {
            if (key_parts_len_ == 3)
            {
                throw wsrep::runtime_error("key parts exceed maximum of 3");
            }
            key_parts_[key_parts_len_] = wsrep::const_buffer(ptr, len);
            ++key_parts_len_;
        }

        enum type type() const
        {
            return type_;
        }

        size_t size() const
        {
            return key_parts_len_;
        }

        const wsrep::const_buffer* key_parts() const
        {
            return key_parts_;
        }
    private:

        enum type type_;
        wsrep::const_buffer key_parts_[3];
        size_t key_parts_len_;
    };

    typedef std::vector<wsrep::key> key_array;

    std::ostream& operator<<(std::ostream&, enum wsrep::key::type);
    std::ostream& operator<<(std::ostream&, const wsrep::key&);
}

#endif // WSREP_KEY_HPP
