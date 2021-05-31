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

#ifndef WSREP_STREAMING_CONTEXT_HPP
#define WSREP_STREAMING_CONTEXT_HPP

#include "compiler.hpp"
#include "logger.hpp"
#include "seqno.hpp"
#include "transaction_id.hpp"

#include <vector>

namespace wsrep
{
    class streaming_context
    {
    public:
        enum fragment_unit
        {
            bytes,
            row,
            statement
        };

        streaming_context()
            : fragments_certified_()
            , fragments_()
            , rollback_replicated_for_()
            , fragment_unit_()
            , fragment_size_()
            , unit_counter_()
            , log_position_()
        { }

        /**
         * Set streaming parameters.
         *
         * Calling this method has a side effect of resetting unit
         * counter.
         *
         * @param fragment_unit Desired fragment unit.
         * @param fragment_size Desired fragment size.
         */
        void params(enum fragment_unit fragment_unit, size_t fragment_size)
        {
            if (fragment_size)
            {
                WSREP_LOG_DEBUG(wsrep::log::debug_log_level(),
                                 wsrep::log::debug_level_streaming,
                                 "Enabling streaming: "
                                 << fragment_unit << " " << fragment_size);
            }
            else
            {
                WSREP_LOG_DEBUG(wsrep::log::debug_log_level(),
                                wsrep::log::debug_level_streaming,
                                "Disabling streaming");
            }
            fragment_unit_ = fragment_unit;
            fragment_size_ = fragment_size;
            reset_unit_counter();
        }

        void enable(enum fragment_unit fragment_unit, size_t fragment_size)
        {
            WSREP_LOG_DEBUG(wsrep::log::debug_log_level(),
                            wsrep::log::debug_level_streaming,
                            "Enabling streaming: "
                            << fragment_unit << " " << fragment_size);
            assert(fragment_size > 0);
            fragment_unit_ = fragment_unit;
            fragment_size_ = fragment_size;
        }

        enum fragment_unit fragment_unit() const { return fragment_unit_; }

        size_t fragment_size() const { return fragment_size_; }

        void disable()
        {
            WSREP_LOG_DEBUG(wsrep::log::debug_log_level(),
                            wsrep::log::debug_level_streaming,
                            "Disabling streaming");
            fragment_size_ = 0;
        }

        void certified()
        {
            ++fragments_certified_;
        }

        size_t fragments_certified() const
        {
            return fragments_certified_;
        }

        void stored(wsrep::seqno seqno)
        {
            check_fragment_seqno(seqno);
            fragments_.push_back(seqno);
        }

        size_t fragments_stored() const
        {
            return fragments_.size();
        }

        void applied(wsrep::seqno seqno)
        {
            check_fragment_seqno(seqno);
            ++fragments_certified_;
            fragments_.push_back(seqno);
        }

        void rolled_back(wsrep::transaction_id id)
        {
            assert(rollback_replicated_for_ == wsrep::transaction_id::undefined());
            rollback_replicated_for_ = id;
        }

        bool rolled_back() const
        {
            return (rollback_replicated_for_ !=
                    wsrep::transaction_id::undefined());
        }

        size_t unit_counter() const
        {
            return unit_counter_;
        }

        void set_unit_counter(size_t count)
        {
            unit_counter_ = count;
        }

        void increment_unit_counter(size_t inc)
        {
            unit_counter_ += inc;
        }

        void reset_unit_counter()
        {
            unit_counter_ = 0;
        }

        size_t log_position() const
        {
            return log_position_;
        }

        void set_log_position(size_t position)
        {
            log_position_ = position;
        }

        const std::vector<wsrep::seqno>& fragments() const
        {
            return fragments_;
        }

        bool fragment_size_exceeded() const
        {
            return unit_counter_ >= fragment_size_;
        }

        void cleanup()
        {
            fragments_certified_ = 0;
            fragments_.clear();
            rollback_replicated_for_ = wsrep::transaction_id::undefined();
            unit_counter_ = 0;
            log_position_ = 0;
        }
    private:

        void check_fragment_seqno(wsrep::seqno seqno WSREP_UNUSED)
        {
            assert(seqno.is_undefined() == false);
            assert(fragments_.empty() || fragments_.back() < seqno);
        }

        size_t fragments_certified_;
        std::vector<wsrep::seqno> fragments_;
        wsrep::transaction_id rollback_replicated_for_;
        enum fragment_unit fragment_unit_;
        size_t fragment_size_;
        size_t unit_counter_;
        size_t log_position_;
    };
}

#endif // WSREP_STREAMING_CONTEXT_HPP
