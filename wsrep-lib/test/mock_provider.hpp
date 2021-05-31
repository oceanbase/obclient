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

#ifndef WSREP_MOCK_PROVIDER_HPP
#define WSREP_MOCK_PROVIDER_HPP

#include "wsrep/provider.hpp"
#include "wsrep/logger.hpp"
#include "wsrep/buffer.hpp"
#include "wsrep/high_priority_service.hpp"

#include <cstring>
#include <map>
#include <iostream> // todo: proper logging

#include <boost/test/unit_test.hpp>

namespace wsrep
{
    class mock_provider : public wsrep::provider
    {
    public:
        typedef std::map<wsrep::transaction_id, wsrep::seqno> bf_abort_map;

        mock_provider(wsrep::server_state& server_state)
            : provider(server_state)
            , certify_result_()
            , commit_order_enter_result_()
            , commit_order_leave_result_()
            , release_result_()
            , replay_result_()
            , group_id_("1")
            , server_id_("1")
            , group_seqno_(0)
            , bf_abort_map_()
            , start_fragments_()
            , fragments_()
            , commit_fragments_()
            , rollback_fragments_()
            , toi_write_sets_()
            , toi_start_transaction_()
            , toi_commit_()
        { }

        enum wsrep::provider::status
        connect(const std::string&, const std::string&, const std::string&,
                bool) WSREP_OVERRIDE
        { return wsrep::provider::success; }
        int disconnect() WSREP_OVERRIDE { return 0; }
        int capabilities() const WSREP_OVERRIDE { return 0; }
        int desync() WSREP_OVERRIDE { return 0; }
        int resync() WSREP_OVERRIDE { return 0; }
        wsrep::seqno pause() WSREP_OVERRIDE { return wsrep::seqno(0); }
        int resume() WSREP_OVERRIDE { return 0; }
        enum wsrep::provider::status run_applier(wsrep::high_priority_service*)
            WSREP_OVERRIDE
        {
            return wsrep::provider::success;
        }
        // Provider implemenatation interface
        int start_transaction(wsrep::ws_handle&) WSREP_OVERRIDE { return 0; }
        enum wsrep::provider::status
        certify(wsrep::client_id client_id,
                wsrep::ws_handle& ws_handle,
                int flags,
                wsrep::ws_meta& ws_meta)
            WSREP_OVERRIDE
        {
            ws_handle = wsrep::ws_handle(ws_handle.transaction_id(), (void*)1);
            wsrep::log_debug() << "provider certify: "
                               << "client: " << client_id.get()
                               << " flags: " << std::hex << flags
                               << std::dec
                               << " certify_status: " << certify_result_;
            if (certify_result_)
            {
                return certify_result_;
            }

            ++fragments_;
            if (starts_transaction(flags))
            {
                ++start_fragments_;
            }
            if (commits_transaction(flags))
            {
                ++commit_fragments_;
            }
            if (rolls_back_transaction(flags))
            {
                ++rollback_fragments_;
            }

            wsrep::stid stid(server_id_,
                             ws_handle.transaction_id(),
                             client_id);
            bf_abort_map::iterator it(bf_abort_map_.find(
                                          ws_handle.transaction_id()));
            if (it == bf_abort_map_.end())
            {
                ++group_seqno_;
                wsrep::gtid gtid(group_id_, wsrep::seqno(group_seqno_));
                ws_meta = wsrep::ws_meta(gtid, stid,
                                         wsrep::seqno(group_seqno_ - 1),
                                         flags);
                return wsrep::provider::success;
            }
            else
            {
                enum wsrep::provider::status ret;
                if (it->second.is_undefined())
                {
                    ws_meta = wsrep::ws_meta(wsrep::gtid(), wsrep::stid(),
                                             wsrep::seqno::undefined(), 0);
                    ret = wsrep::provider::error_certification_failed;
                }
                else
                {
                    ++group_seqno_;
                    wsrep::gtid gtid(group_id_, wsrep::seqno(group_seqno_));
                    ws_meta = wsrep::ws_meta(gtid, stid,
                                             wsrep::seqno(group_seqno_ - 1),
                                             flags);
                    ret = wsrep::provider::error_bf_abort;
                }
                bf_abort_map_.erase(it);
                return ret;
            }
        }

        enum wsrep::provider::status
        assign_read_view(wsrep::ws_handle&, const wsrep::gtid*)
            WSREP_OVERRIDE
        { return wsrep::provider::success; }
        int append_key(wsrep::ws_handle&, const wsrep::key&)
            WSREP_OVERRIDE
        { return 0; }
        enum wsrep::provider::status
        append_data(wsrep::ws_handle&, const wsrep::const_buffer&)
            WSREP_OVERRIDE
        { return wsrep::provider::success; }
        enum wsrep::provider::status rollback(const wsrep::transaction_id)
        WSREP_OVERRIDE
        {
            ++fragments_;
            ++rollback_fragments_;
            return wsrep::provider::success;
        }
        enum wsrep::provider::status
        commit_order_enter(const wsrep::ws_handle& ws_handle,
                           const wsrep::ws_meta& ws_meta)
            WSREP_OVERRIDE
        {
            BOOST_REQUIRE(ws_handle.opaque());
            BOOST_REQUIRE(ws_meta.seqno().is_undefined() == false);
            return commit_order_enter_result_;
        }

        int commit_order_leave(const wsrep::ws_handle& ws_handle,
                               const wsrep::ws_meta& ws_meta,
                               const wsrep::mutable_buffer& err)
            WSREP_OVERRIDE
        {
            BOOST_REQUIRE(ws_handle.opaque());
            BOOST_REQUIRE(ws_meta.seqno().is_undefined() == false);
            return err.size() > 0 ?
                   wsrep::provider::error_fatal :
                   commit_order_leave_result_;
        }

        int release(wsrep::ws_handle& )
            WSREP_OVERRIDE
        {
            // BOOST_REQUIRE(ws_handle.opaque());
            return release_result_;
        }

        enum wsrep::provider::status replay(
            const wsrep::ws_handle& ws_handle,
            wsrep::high_priority_service* hps)
            WSREP_OVERRIDE
        {
            wsrep::mock_high_priority_service& high_priority_service(
                *static_cast<wsrep::mock_high_priority_service*>(hps));
            wsrep::mock_client_state& cc(
                static_cast<wsrep::mock_client_state&>(
                    high_priority_service.client_state()));
            wsrep::high_priority_context high_priority_context(cc);
            const wsrep::transaction& tc(cc.transaction());
            wsrep::ws_meta ws_meta;
            if (replay_result_ == wsrep::provider::success)
            {
                // If the ws_meta was not assigned yet, the certify
                // returned early due to BF abort.
                if (tc.ws_meta().seqno().is_undefined())
                {
                    ++group_seqno_;
                    ws_meta = wsrep::ws_meta(
                        wsrep::gtid(group_id_, wsrep::seqno(group_seqno_)),
                        wsrep::stid(server_id_, tc.id(), cc.id()),
                        wsrep::seqno(group_seqno_ - 1),
                        wsrep::provider::flag::start_transaction |
                        wsrep::provider::flag::commit);
                }
                else
                {
                    ws_meta = tc.ws_meta();
                }
            }
            else
            {
                return replay_result_;
            }

            if (high_priority_service.apply(ws_handle, ws_meta,
                                            wsrep::const_buffer()))
            {
                return wsrep::provider::error_fatal;
            }
            return wsrep::provider::success;
        }

        enum wsrep::provider::status enter_toi(wsrep::client_id client_id,
                                               const wsrep::key_array&,
                                               const wsrep::const_buffer&,
                                               wsrep::ws_meta& toi_meta,
                                               int flags)
            WSREP_OVERRIDE
        {
            ++group_seqno_;
            wsrep::gtid gtid(group_id_, wsrep::seqno(group_seqno_));
            wsrep::stid stid(server_id_,
                             wsrep::transaction_id::undefined(),
                             client_id);
            toi_meta = wsrep::ws_meta(gtid, stid,
                                      wsrep::seqno(group_seqno_ - 1),
                                      flags);
            ++toi_write_sets_;
            if (flags & wsrep::provider::flag::start_transaction)
                ++toi_start_transaction_;
            if (flags & wsrep::provider::flag::commit)
                ++toi_commit_;
            return certify_result_;
        }

        enum wsrep::provider::status leave_toi(wsrep::client_id,
                                               const wsrep::mutable_buffer&)
            WSREP_OVERRIDE
        { return wsrep::provider::success; }

        std::pair<wsrep::gtid, enum wsrep::provider::status>
        causal_read(int) const WSREP_OVERRIDE
        {
            return std::make_pair(wsrep::gtid::undefined(),
                                  wsrep::provider::error_not_implemented);
        }
        enum wsrep::provider::status wait_for_gtid(const wsrep::gtid&,
            int) const WSREP_OVERRIDE
        { return wsrep::provider::success; }
        wsrep::gtid last_committed_gtid() const WSREP_OVERRIDE
        { return wsrep::gtid(); }
        enum wsrep::provider::status sst_sent(const wsrep::gtid&, int)
            WSREP_OVERRIDE
        { return wsrep::provider::success; }
        enum wsrep::provider::status sst_received(const wsrep::gtid&, int)
            WSREP_OVERRIDE
        { return wsrep::provider::success; }

        enum wsrep::provider::status enc_set_key(const wsrep::const_buffer&)
            WSREP_OVERRIDE
        { return wsrep::provider::success; }

        std::vector<status_variable> status() const WSREP_OVERRIDE
        {
            return std::vector<status_variable>();
        }
        void reset_status() WSREP_OVERRIDE { }
        std::string options() const WSREP_OVERRIDE { return ""; }
        enum wsrep::provider::status options(const std::string&)
            WSREP_OVERRIDE
        { return wsrep::provider::success; }
        std::string name() const WSREP_OVERRIDE { return "mock"; }
        std::string version() const WSREP_OVERRIDE { return "0.0"; }
        std::string vendor() const WSREP_OVERRIDE { return "mock"; }
        void* native() const WSREP_OVERRIDE { return 0; }

        //
        // Methods to modify mock state
        //
        /** Inject BF abort event into the provider.
         *
         * @param bf_seqno Aborter sequence number
         * @param trx_id Trx id to be aborted
         * @param[out] victim_seqno
         */
        enum wsrep::provider::status
        bf_abort(wsrep::seqno bf_seqno,
                 wsrep::transaction_id trx_id,
                 wsrep::seqno& victim_seqno)
            WSREP_OVERRIDE
        {
            bf_abort_map_.insert(std::make_pair(trx_id, bf_seqno));
            if (bf_seqno.is_undefined() == false)
            {
                group_seqno_ = bf_seqno.get();
            }
            victim_seqno = wsrep::seqno::undefined();
            return wsrep::provider::success;
        }

        // Parameters to control return value from the call
        enum wsrep::provider::status certify_result_;
        enum wsrep::provider::status commit_order_enter_result_;
        enum wsrep::provider::status commit_order_leave_result_;
        enum wsrep::provider::status release_result_;
        enum wsrep::provider::status replay_result_;

        size_t start_fragments() const { return start_fragments_; }
        size_t fragments() const { return fragments_; }
        size_t commit_fragments() const { return commit_fragments_; }
        size_t rollback_fragments() const { return rollback_fragments_; }
        size_t toi_write_sets() const { return toi_write_sets_; }
        size_t toi_start_transaction() const { return toi_start_transaction_; }
        size_t toi_commit() const { return toi_commit_; }
    private:
        wsrep::id group_id_;
        wsrep::id server_id_;
        long long group_seqno_;
        bf_abort_map bf_abort_map_;
        size_t start_fragments_;
        size_t fragments_;
        size_t commit_fragments_;
        size_t rollback_fragments_;
        size_t toi_write_sets_;
        size_t toi_start_transaction_;
        size_t toi_commit_;
    };
}


#endif // WSREP_MOCK_PROVIDER_HPP
