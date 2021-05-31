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

#ifndef WSREP_MOCK_SERVER_STATE_HPP
#define WSREP_MOCK_SERVER_STATE_HPP

#include "wsrep/server_state.hpp"
#include "wsrep/server_service.hpp"
#include "mock_client_state.hpp"
#include "mock_high_priority_service.hpp"
#include "mock_storage_service.hpp"
#include "mock_provider.hpp"

#include "wsrep/compiler.hpp"

namespace wsrep
{
    class mock_server_service : public wsrep::server_service
    {
    public:
        mock_server_service(wsrep::server_state& server_state)
            : sync_point_enabled_()
            , sync_point_action_()
            , sst_before_init_()
            , server_state_(server_state)
            , last_client_id_(0)
            , last_transaction_id_(0)
            , logged_view_()
            , position_()
        { }

        wsrep::storage_service* storage_service(wsrep::client_service&)
            WSREP_OVERRIDE
        {
            return new wsrep::mock_storage_service(server_state_,
                                                   wsrep::client_id(++last_client_id_));
        }

        wsrep::storage_service* storage_service(wsrep::high_priority_service&)
            WSREP_OVERRIDE
        {
            return new wsrep::mock_storage_service(server_state_,
                                                   wsrep::client_id(++last_client_id_));
        }

        void release_storage_service(wsrep::storage_service* storage_service)
            WSREP_OVERRIDE
        {
            delete storage_service;
        }

        wsrep::high_priority_service* streaming_applier_service(
            wsrep::client_service&)
            WSREP_OVERRIDE
        {
            wsrep::mock_client* cs(new wsrep::mock_client(
                                       server_state_,
                                       wsrep::client_id(++last_client_id_),
                                       wsrep::client_state::m_high_priority));
            wsrep::mock_high_priority_service* ret(
                new wsrep::mock_high_priority_service(server_state_,
                                                      cs, false));
            cs->open(cs->id());
            cs->before_command();
            return ret;
        }

        wsrep::high_priority_service* streaming_applier_service(
            wsrep::high_priority_service&) WSREP_OVERRIDE
        {
            wsrep::mock_client* cs(new wsrep::mock_client(
                                       server_state_,
                                       wsrep::client_id(++last_client_id_),
                                       wsrep::client_state::m_high_priority));
            wsrep::mock_high_priority_service* ret(
                new wsrep::mock_high_priority_service(server_state_,
                                                      cs, false));
            cs->open(cs->id());
            cs->before_command();
            return ret;
        }

        void release_high_priority_service(
            wsrep::high_priority_service *high_priority_service)
            WSREP_OVERRIDE
        {
            mock_high_priority_service* mhps(
                static_cast<mock_high_priority_service*>(high_priority_service));
            wsrep::mock_client* cs(&static_cast<wsrep::mock_client&>(
                                       mhps->client_state()));
            cs->after_command_before_result();
            cs->after_command_after_result();
            cs->close();
            cs->cleanup();
            delete cs;
            delete mhps;
        }
        void bootstrap() WSREP_OVERRIDE { }
        void log_message(enum wsrep::log::level level, const char* message)
            WSREP_OVERRIDE
        {
            wsrep::log(level, server_state_.name().c_str()) << message;
        }
        void log_dummy_write_set(wsrep::client_state&,
                                 const wsrep::ws_meta&)
            WSREP_OVERRIDE
        {
        }
        void log_view(wsrep::high_priority_service*, const wsrep::view& view)
            WSREP_OVERRIDE
        {
            logged_view_ = view;
        }

        void recover_streaming_appliers(wsrep::client_service&)
            WSREP_OVERRIDE
        { }

        void recover_streaming_appliers(wsrep::high_priority_service&)
            WSREP_OVERRIDE
        { }

        wsrep::view get_view(wsrep::client_service&, const wsrep::id& own_id)
            WSREP_OVERRIDE
        {
            int const my_idx(logged_view_.member_index(own_id));
            wsrep::view my_view(
                logged_view_.state_id(),
                logged_view_.view_seqno(),
                logged_view_.status(),
                logged_view_.capabilities(),
                my_idx,
                logged_view_.protocol_version(),
                logged_view_.members()
            );
            return my_view;
        }

        wsrep::gtid get_position(wsrep::client_service&) WSREP_OVERRIDE
        {
            return position_;
        }

        void set_position(wsrep::client_service&,
                          const wsrep::gtid& gtid) WSREP_OVERRIDE
        {
            position_ = gtid;
        }

        void log_state_change(enum wsrep::server_state::state,
                              enum wsrep::server_state::state)
            WSREP_OVERRIDE
        { }
        bool sst_before_init() const WSREP_OVERRIDE
        { return sst_before_init_; }
        std::string sst_request() WSREP_OVERRIDE { return ""; }
        int start_sst(const std::string&,
                      const wsrep::gtid&,
                      bool) WSREP_OVERRIDE { return 0; }
        void background_rollback(wsrep::client_state& client_state)
            WSREP_OVERRIDE
        {
            client_state.before_rollback();
            client_state.after_rollback();
        }

        int wait_committing_transactions(int) WSREP_OVERRIDE { return 0; }

        wsrep::transaction_id next_transaction_id()
        {
            return wsrep::transaction_id(++last_transaction_id_);
        }

        void debug_sync(const char* sync_point) WSREP_OVERRIDE
        {
            if (sync_point_enabled_ == sync_point)
            {
                switch (sync_point_action_)
                {
                case spa_none:
                    break;
                case spa_initialize:
                    server_state_.initialized();
                    break;
                case spa_initialize_error:
                    throw wsrep::runtime_error("Inject initialization error");
                    break;
                }
            }
        }

        std::string sync_point_enabled_;
        enum sync_point_action
        {
            spa_none,
            spa_initialize,
            spa_initialize_error

        } sync_point_action_;
        bool sst_before_init_;

        void logged_view(const wsrep::view& view)
        {
            logged_view_ = view;
        }
        void position(const wsrep::gtid& position)
        {
            position_ = position;
        }
    private:
        wsrep::server_state& server_state_;
        unsigned long long last_client_id_;
        unsigned long long last_transaction_id_;
        wsrep::view logged_view_;
        wsrep::gtid position_;
    };


    class mock_server_state : public wsrep::server_state
    {
    public:
        mock_server_state(const std::string& name,
                          enum wsrep::server_state::rollback_mode rollback_mode,
                          wsrep::server_service& server_service)
            : wsrep::server_state(mutex_, cond_, server_service, NULL,
                                  name, "", "", "./",
                                  wsrep::gtid::undefined(),
                                  1,
                                  rollback_mode)
            , mutex_()
            , cond_()
            , provider_(*this)
        { }

        wsrep::mock_provider& provider() const WSREP_OVERRIDE
        { return provider_; }

        // mock connected state for tests without overriding the connect()
        // method.
        int mock_connect(const std::string& own_id,
                         const std::string& cluster_name,
                         const std::string& cluster_address,
                         const std::string& state_donor,
                         bool bootstrap)
        {
            int const ret(server_state::connect(cluster_name,
                                                cluster_address,
                                                state_donor,
                                                bootstrap));
            if (0 == ret)
            {
                wsrep::id cluster_id("1");
                wsrep::gtid state_id(cluster_id, wsrep::seqno(0));
                std::vector<wsrep::view::member> members;
                members.push_back(wsrep::view::member(wsrep::id(own_id),
                                                      "name", ""));
                wsrep::view bootstrap_view(state_id,
                                           wsrep::seqno(1),
                                           wsrep::view::primary,
                                           0,
                                           0,
                                           1,
                                           members);
                server_state::on_connect(bootstrap_view);
            }
            else
            {
                assert(0);
            }

            return ret;
        }

        int mock_connect()
        {
             return mock_connect(name(), "cluster", "local", "0", false);
        }

    private:
        wsrep::default_mutex mutex_;
        wsrep::default_condition_variable cond_;
        mutable wsrep::mock_provider provider_;
    };
}

#endif // WSREP_MOCK_SERVER_STATE_HPP
