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

#ifndef WSREP_XID_HPP
#define WSREP_XID_HPP

#include <iosfwd>
#include "buffer.hpp"
#include "exception.hpp"

namespace wsrep
{
    class xid
    {
    public:
        xid()
            : format_id_(-1)
            , gtrid_len_(0)
            , bqual_len_(0)
            , data_()
        { }

        xid(long format_id, long gtrid_len,
            long bqual_len, const char* data)
            : format_id_(format_id)
            , gtrid_len_(gtrid_len)
            , bqual_len_(bqual_len)
            , data_()
        {
            if (gtrid_len_ > 64 || bqual_len_ > 64)
            {
                throw wsrep::runtime_error("maximum wsrep::xid size exceeded");
            }
            const long len = gtrid_len_ + bqual_len_;
            if (len > 0)
            {
                data_.push_back(data, data + len);
            }
        }

        xid(const xid& xid)
            : format_id_(xid.format_id_)
            , gtrid_len_(xid.gtrid_len_)
            , bqual_len_(xid.bqual_len_)
            , data_(xid.data_)
        { }

        bool is_null() const
        {
            return format_id_ == -1;
        }

        void clear()
        {
            format_id_ = -1;
            gtrid_len_ = 0;
            bqual_len_ = 0;
            data_.clear();
        }

        xid& operator= (const xid& other)
        {
            format_id_ = other.format_id_;
            gtrid_len_ = other.gtrid_len_;
            bqual_len_ = other.bqual_len_;
            data_ = other.data_;
            return *this;
        }

        bool operator==(const xid& other) const
        {
            if (format_id_ != other.format_id_ ||
                gtrid_len_ != other.gtrid_len_ ||
                bqual_len_ != other.bqual_len_ ||
                data_.size() != other.data_.size())
            {
                return false;
            }
            return data_ == other.data_;
        }

        friend std::string to_string(const wsrep::xid& xid);
        friend std::ostream& operator<<(std::ostream& os, const wsrep::xid& xid);
    protected:
        long format_id_;
        long gtrid_len_;
        long bqual_len_;
        mutable_buffer data_;
    };

    std::string to_string(const wsrep::xid& xid);
    std::ostream& operator<<(std::ostream& os, const wsrep::xid& xid);
}

#endif // WSREP_XID_HPP
