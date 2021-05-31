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

#ifndef WSREP_MOCK_HIGH_PRIORITY_SERVICE_HPP
#define WSREP_MOCK_HIGH_PRIORITY_SERVICE_HPP

#include "wsrep/high_priority_service.hpp"
#include "mock_client_state.hpp"

#include <memory>

namespace wsrep
{
    class mock_high_priority_service : public wsrep::high_priority_service
    {
    public:
        mock_high_priority_service(
            wsrep::server_state& server_state,
            wsrep::mock_client_state* client_state,
            bool replaying)
            : wsrep::high_priority_service(server_state)
            , do_2pc_()
            , fail_next_applying_()
            , fail_next_toi_()
            , client_state_(client_state)
            , replaying_(replaying)
            , nbo_cs_()
        { }

        ~mock_high_priority_service()
        { }
        int start_transaction(const wsrep::ws_handle&, const wsrep::ws_meta&)
            WSREP_OVERRIDE;

        int next_fragment(const wsrep::ws_meta&) WSREP_OVERRIDE;

        const wsrep::transaction& transaction() const WSREP_OVERRIDE
        { return client_state_->transaction(); }
        int adopt_transaction(const wsrep::transaction&) WSREP_OVERRIDE;
        int apply_write_set(const wsrep::ws_meta&,
                            const wsrep::const_buffer&,
                            wsrep::mutable_buffer&) WSREP_OVERRIDE;
        int append_fragment_and_commit(
            const wsrep::ws_handle&,
            const wsrep::ws_meta&,
            const wsrep::const_buffer&,
            const wsrep::xid&) WSREP_OVERRIDE
        { return 0; }
        int remove_fragments(const wsrep::ws_meta&) WSREP_OVERRIDE
        { return 0; }
        int commit(const wsrep::ws_handle&, const wsrep::ws_meta&)
            WSREP_OVERRIDE;
        int rollback(const wsrep::ws_handle&, const wsrep::ws_meta&) WSREP_OVERRIDE;
        int apply_toi(const wsrep::ws_meta&,
                      const wsrep::const_buffer&,
                      wsrep::mutable_buffer&) WSREP_OVERRIDE;
        int apply_nbo_begin(const wsrep::ws_meta&,
                            const wsrep::const_buffer&,
                            wsrep::mutable_buffer&) WSREP_OVERRIDE;
        void adopt_apply_error(wsrep::mutable_buffer& err) WSREP_OVERRIDE;
        void after_apply() WSREP_OVERRIDE;
        void store_globals() WSREP_OVERRIDE { }
        void reset_globals() WSREP_OVERRIDE { }
        void switch_execution_context(wsrep::high_priority_service&)
            WSREP_OVERRIDE { }
        int log_dummy_write_set(const wsrep::ws_handle&,
                                const wsrep::ws_meta&,
                                wsrep::mutable_buffer&) WSREP_OVERRIDE;
        bool is_replaying() const WSREP_OVERRIDE { return replaying_; }
        void debug_crash(const char*) WSREP_OVERRIDE { /* Not in unit tests*/}

        wsrep::client_state& client_state()
        {
            return *client_state_;
        }
        bool do_2pc_;
        bool fail_next_applying_;
        bool fail_next_toi_;

        wsrep::mock_client* nbo_cs() const { return nbo_cs_.get(); }

    private:
        mock_high_priority_service(const mock_high_priority_service&);
        mock_high_priority_service& operator=(const mock_high_priority_service&);
        wsrep::mock_client_state* client_state_;
        bool replaying_;

        /* Client state associated to NBO processing. This should be
         * stored elsewhere (like mock_server_state), but is kept
         * here for convenience. */
        std::unique_ptr<wsrep::mock_client> nbo_cs_;
    };
}

#endif // WSREP_MOCK_HIGH_PRIORITY_SERVICE_HPP
