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

#ifndef WSREP_WSREP_PROVIDER_V26_HPP
#define WSREP_WSREP_PROVIDER_V26_HPP

#include "wsrep/provider.hpp"

struct wsrep_st;

namespace wsrep
{
    class thread_service;
    class wsrep_provider_v26 : public wsrep::provider
    {
    public:

        wsrep_provider_v26(wsrep::server_state&, const std::string&,
                           const std::string&,
                           const wsrep::provider::services& services);
        ~wsrep_provider_v26();
        enum wsrep::provider::status
        connect(const std::string&, const std::string&, const std::string&,
                    bool) WSREP_OVERRIDE;
        int disconnect() WSREP_OVERRIDE;
        int capabilities() const WSREP_OVERRIDE;

        int desync() WSREP_OVERRIDE;
        int resync() WSREP_OVERRIDE;
        wsrep::seqno pause() WSREP_OVERRIDE;
        int resume() WSREP_OVERRIDE;

        enum wsrep::provider::status
        run_applier(wsrep::high_priority_service*) WSREP_OVERRIDE;
        int start_transaction(wsrep::ws_handle&) WSREP_OVERRIDE { return 0; }
        enum wsrep::provider::status
        assign_read_view(wsrep::ws_handle&, const wsrep::gtid*) WSREP_OVERRIDE;
        int append_key(wsrep::ws_handle&, const wsrep::key&) WSREP_OVERRIDE;
        enum wsrep::provider::status
        append_data(wsrep::ws_handle&, const wsrep::const_buffer&)
            WSREP_OVERRIDE;
        enum wsrep::provider::status
        certify(wsrep::client_id, wsrep::ws_handle&,
                int,
                wsrep::ws_meta&) WSREP_OVERRIDE;
        enum wsrep::provider::status
        bf_abort(wsrep::seqno,
                 wsrep::transaction_id,
                 wsrep::seqno&) WSREP_OVERRIDE;
        enum wsrep::provider::status
        rollback(const wsrep::transaction_id) WSREP_OVERRIDE;
        enum wsrep::provider::status
        commit_order_enter(const wsrep::ws_handle&,
                           const wsrep::ws_meta&) WSREP_OVERRIDE;
        int commit_order_leave(const wsrep::ws_handle&,
                               const wsrep::ws_meta&,
                               const wsrep::mutable_buffer&) WSREP_OVERRIDE;
        int release(wsrep::ws_handle&) WSREP_OVERRIDE;
        enum wsrep::provider::status replay(const wsrep::ws_handle&,
                                            wsrep::high_priority_service*)
            WSREP_OVERRIDE;
        enum wsrep::provider::status enter_toi(wsrep::client_id,
                                               const wsrep::key_array&,
                                               const wsrep::const_buffer&,
                                               wsrep::ws_meta&,
                                               int)
            WSREP_OVERRIDE;
        enum wsrep::provider::status leave_toi(wsrep::client_id,
                                               const wsrep::mutable_buffer&)
            WSREP_OVERRIDE;
        std::pair<wsrep::gtid, enum wsrep::provider::status>
        causal_read(int) const WSREP_OVERRIDE;
        enum wsrep::provider::status wait_for_gtid(const wsrep::gtid&, int)
            const WSREP_OVERRIDE;
        wsrep::gtid last_committed_gtid() const WSREP_OVERRIDE;
        enum wsrep::provider::status sst_sent(const wsrep::gtid&, int)
            WSREP_OVERRIDE;
        enum wsrep::provider::status sst_received(const wsrep::gtid& gtid, int)
            WSREP_OVERRIDE;
        enum wsrep::provider::status enc_set_key(const wsrep::const_buffer& key)
            WSREP_OVERRIDE;
        std::vector<status_variable> status() const WSREP_OVERRIDE;
        void reset_status() WSREP_OVERRIDE;
        std::string options() const WSREP_OVERRIDE;
        enum wsrep::provider::status options(const std::string&) WSREP_OVERRIDE;
        std::string name() const WSREP_OVERRIDE;
        std::string version() const WSREP_OVERRIDE;
        std::string vendor() const WSREP_OVERRIDE;
        void* native() const WSREP_OVERRIDE;
    private:
        wsrep_provider_v26(const wsrep_provider_v26&);
        wsrep_provider_v26& operator=(const wsrep_provider_v26);
        struct wsrep_st* wsrep_;
    };
}


#endif // WSREP_WSREP_PROVIDER_V26_HPP
