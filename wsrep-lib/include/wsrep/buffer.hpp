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

#ifndef WSREP_BUFFER_HPP
#define WSREP_BUFFER_HPP

#include <cstddef>
#include <vector>

namespace wsrep
{
    class const_buffer
    {
    public:
        const_buffer()
            : ptr_()
            , size_()
        { }

        const_buffer(const void* ptr, size_t size)
            : ptr_(ptr)
            , size_(size)
        { }

        const_buffer(const const_buffer& b)
            : ptr_(b.ptr())
            , size_(b.size())
        { }

        const void* ptr() const { return ptr_; }
        const char* data() const { return static_cast<const char*>(ptr_); }
        size_t size() const { return size_; }

        const_buffer& operator=(const const_buffer& b)
        {
            ptr_  = b.ptr();
            size_ = b.size();
            return *this;
        }
    private:
        const void* ptr_;
        size_t size_;
    };


    class mutable_buffer
    {
    public:
        mutable_buffer()
            : buffer_()
        { }

        mutable_buffer(const mutable_buffer& b)
            : buffer_(b.buffer_)
        { }

        void resize(size_t s) { buffer_.resize(s); }

        void clear()
        {
            // using swap to ensure deallocation
            std::vector<char>().swap(buffer_);
        }

        void push_back(const char* begin, const char* end)
        {
            buffer_.insert(buffer_.end(), begin, end);
        }

        template <class C> void push_back(const C& c)
        {
            std::copy(c.begin(), c.end(), std::back_inserter(buffer_));
        }

        size_t size() const { return buffer_.size(); }

        /**
         * Return pointer to underlying data array. The returned pointer
         * may or may not be null in case of empty buffer, it is up to
         * user to check the size of the array before dereferencing the
         * pointer.
         *
         * @return Pointer to underlying data array.
         */
        char* data() { return buffer_.data(); }

        /**
         * Return const pointer to underlying data array. The returned pointer
         * may or may not be null in case of empty buffer, it is up to
         * user to check the size of the array before dereferencing the
         * pointer.
         *
         * @return Const pointer to underlying data array.
         */
        const char* data() const { return buffer_.data(); }

        mutable_buffer& operator= (const mutable_buffer& other)
        {
            buffer_ = other.buffer_;
            return *this;
        }

        bool operator==(const mutable_buffer& other) const
        {
          return buffer_ == other.buffer_;
        }
    private:
        std::vector<char> buffer_;
    };
}

#endif // WSREP_BUFFER_HPP
