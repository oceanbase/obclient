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

/** @file transaction.hpp */
#ifndef WSREP_TRANSACTION_HPP
#define WSREP_TRANSACTION_HPP

#include "provider.hpp"
#include "server_state.hpp"
#include "transaction_id.hpp"
#include "streaming_context.hpp"
#include "lock.hpp"
#include "sr_key_set.hpp"
#include "buffer.hpp"
#include "client_service.hpp"
#include "xid.hpp"

#include <cassert>
#include <vector>

namespace wsrep
{
    class client_service;
    class client_state;
    class key;
    class const_buffer;

    class transaction
    {
    public:
        enum state
        {
            s_executing,
            s_preparing,
            s_prepared,
            s_certifying,
            s_committing,
            s_ordered_commit,
            s_committed,
            s_cert_failed,
            s_must_abort,
            s_aborting,
            s_aborted,
            s_must_replay,
            s_replaying
        };
        static const int n_states = s_replaying + 1;
        enum state state() const
        { return state_; }

        transaction(wsrep::client_state& client_state);
        ~transaction();
        // Accessors
        wsrep::transaction_id id() const
        { return id_; }

        const wsrep::id& server_id() const
        { return server_id_; }

        bool active() const
        { return (id_ != wsrep::transaction_id::undefined()); }


        void state(wsrep::unique_lock<wsrep::mutex>&, enum state);

        // Return true if the certification of the last
        // fragment succeeded
        bool certified() const { return certified_; }

        wsrep::seqno seqno() const
        {
            return ws_meta_.seqno();
        }
        // Return true if the last fragment was ordered by the
        // provider
        bool ordered() const
        { return (ws_meta_.seqno().is_undefined() == false); }

        /**
         * Return true if any fragments have been succesfully certified
         * for the transaction.
         */
        bool is_streaming() const
        {
            return (streaming_context_.fragments_certified() > 0);
        }

        /**
         * Return number of fragments certified for current statement.
         *
         * This counts fragments which have been succesfully certified
         * since the construction of object or last after_statement()
         * call.
         *
         * @return Number of fragments certified for current statement.
         */
        size_t fragments_certified_for_statement() const
        {
            return fragments_certified_for_statement_;
        }

        /**
         * Return true if transaction has not generated any changes.
         */
        bool is_empty() const
        {
            return sr_keys_.empty();
        }

        bool is_xa() const
        {
            return !xid_.is_null();
        }

        void assign_xid(const wsrep::xid& xid);

        const wsrep::xid& xid() const
        {
            return xid_;
        }

        int restore_to_prepared_state(const wsrep::xid& xid);

        int commit_or_rollback_by_xid(const wsrep::xid& xid, bool commit);

        void xa_detach();

        int xa_replay(wsrep::unique_lock<wsrep::mutex>&);

        bool pa_unsafe() const { return pa_unsafe_; }
        void pa_unsafe(bool pa_unsafe) { pa_unsafe_ = pa_unsafe; }

        bool implicit_deps() const { return implicit_deps_; }
        void implicit_deps(bool implicit) { implicit_deps_ = implicit; }

        int start_transaction(const wsrep::transaction_id& id);

        int start_transaction(const wsrep::ws_handle& ws_handle,
                              const wsrep::ws_meta& ws_meta);

        int next_fragment(const wsrep::ws_meta& ws_meta);

        void adopt(const transaction& transaction);
        void fragment_applied(wsrep::seqno seqno);

        int prepare_for_ordering(const wsrep::ws_handle& ws_handle,
                                 const wsrep::ws_meta& ws_meta,
                                 bool is_commit);

        int assign_read_view(const wsrep::gtid* gtid);

        int append_key(const wsrep::key&);

        int append_data(const wsrep::const_buffer&);

        int after_row();

        int before_prepare(wsrep::unique_lock<wsrep::mutex>&);

        int after_prepare(wsrep::unique_lock<wsrep::mutex>&);

        int before_commit();

        int ordered_commit();

        int after_commit();

        int before_rollback();

        int after_rollback();

        int before_statement();

        int after_statement();

        void after_applying();

        bool bf_abort(wsrep::unique_lock<wsrep::mutex>& lock,
                      wsrep::seqno bf_seqno);
        bool total_order_bf_abort(wsrep::unique_lock<wsrep::mutex>&,
                                  wsrep::seqno bf_seqno);

        void clone_for_replay(const wsrep::transaction& other);

        bool bf_aborted() const
        {
            return (bf_abort_client_state_ != 0);
        }

        bool bf_aborted_in_total_order() const
        {
            return bf_aborted_in_total_order_;
        }

        int flags() const
        {
            return flags_;
        }

        // wsrep::mutex& mutex();
        wsrep::ws_handle& ws_handle() { return ws_handle_; }
        const wsrep::ws_handle& ws_handle() const { return ws_handle_; }
        const wsrep::ws_meta& ws_meta() const { return ws_meta_; }
        const wsrep::streaming_context& streaming_context() const
        { return streaming_context_; }
        wsrep::streaming_context& streaming_context()
        { return streaming_context_; }
        void adopt_apply_error(wsrep::mutable_buffer& buf)
        {
            apply_error_buf_ = std::move(buf);
        }
        const wsrep::mutable_buffer& apply_error() const
        { return apply_error_buf_; }
    private:
        transaction(const transaction&);
        transaction operator=(const transaction&);

        wsrep::provider& provider();
        void flags(int flags) { flags_ = flags; }
        // Return true if the transaction must abort, is aborting,
        // or has been aborted, or has been interrupted by DBMS
        // as indicated by client_service::interrupted() call.
        // The call will adjust transaction state and set client_state
        // error status accordingly.
        bool abort_or_interrupt(wsrep::unique_lock<wsrep::mutex>&);
        int streaming_step(wsrep::unique_lock<wsrep::mutex>&, bool force = false);
        int certify_fragment(wsrep::unique_lock<wsrep::mutex>&);
        int certify_commit(wsrep::unique_lock<wsrep::mutex>&);
        int append_sr_keys_for_commit();
        int release_commit_order(wsrep::unique_lock<wsrep::mutex>&);
        void streaming_rollback(wsrep::unique_lock<wsrep::mutex>&);
        int replay(wsrep::unique_lock<wsrep::mutex>&);
        void clear_fragments();
        void cleanup();
        void debug_log_state(const char*) const;
        void debug_log_key_append(const wsrep::key& key) const;

        wsrep::server_service& server_service_;
        wsrep::client_service& client_service_;
        wsrep::client_state& client_state_;
        wsrep::id server_id_;
        wsrep::transaction_id id_;
        enum state state_;
        std::vector<enum state> state_hist_;
        enum state bf_abort_state_;
        enum wsrep::provider::status bf_abort_provider_status_;
        int bf_abort_client_state_;
        bool bf_aborted_in_total_order_;
        wsrep::ws_handle ws_handle_;
        wsrep::ws_meta ws_meta_;
        int flags_;
        bool pa_unsafe_;
        bool implicit_deps_;
        bool certified_;
        size_t fragments_certified_for_statement_;
        wsrep::streaming_context streaming_context_;
        wsrep::sr_key_set sr_keys_;
        wsrep::mutable_buffer apply_error_buf_;
        wsrep::xid xid_;
    };

    static inline const char* to_c_string(enum wsrep::transaction::state state)
    {
        switch (state)
        {
        case wsrep::transaction::s_executing: return "executing";
        case wsrep::transaction::s_preparing: return "preparing";
        case wsrep::transaction::s_prepared: return "prepared";
        case wsrep::transaction::s_certifying: return "certifying";
        case wsrep::transaction::s_committing: return "committing";
        case wsrep::transaction::s_ordered_commit: return "ordered_commit";
        case wsrep::transaction::s_committed: return "committed";
        case wsrep::transaction::s_cert_failed: return "cert_failed";
        case wsrep::transaction::s_must_abort: return "must_abort";
        case wsrep::transaction::s_aborting: return "aborting";
        case wsrep::transaction::s_aborted: return "aborted";
        case wsrep::transaction::s_must_replay: return "must_replay";
        case wsrep::transaction::s_replaying: return "replaying";
        }
        return "unknown";
    }
    static inline std::string to_string(enum wsrep::transaction::state state)
    {
        return to_c_string(state);
    }

}

#endif // WSREP_TRANSACTION_HPP
