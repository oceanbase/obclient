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

#include "db_storage_engine.hpp"
#include "db_client.hpp"

void db::storage_engine::transaction::start(db::client* cc)
{
    wsrep::unique_lock<wsrep::mutex> lock(se_.mutex_);
    if (se_.transactions_.insert(cc).second == false)
    {
        ::abort();
    }
    cc_ = cc;
}

void db::storage_engine::transaction::apply(
    const wsrep::transaction& transaction)
{
    assert(cc_);
    se_.bf_abort_some(transaction);
}

void db::storage_engine::transaction::commit(const wsrep::gtid& gtid)
{
    if (cc_)
    {
        wsrep::unique_lock<wsrep::mutex> lock(se_.mutex_);
        se_.transactions_.erase(cc_);
        se_.store_position(gtid);
    }
    cc_ = nullptr;
}


void db::storage_engine::transaction::rollback()
{
    if (cc_)
    {
        wsrep::unique_lock<wsrep::mutex> lock(se_.mutex_);
        se_.transactions_.erase(cc_);
    }
    cc_ = nullptr;
}

void db::storage_engine::bf_abort_some(const wsrep::transaction& txc)
{
    std::uniform_int_distribution<size_t> uniform_dist(0, alg_freq_);
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    if (alg_freq_ && uniform_dist(random_engine_) == 0)
    {
        if (transactions_.empty() == false)
        {
            for (auto victim : transactions_)
            {
                wsrep::client_state& cc(victim->client_state());
                if (cc.mode() == wsrep::client_state::m_local)
                {
                    if (victim->bf_abort(txc.seqno()))
                    {
                        ++bf_aborts_;
                    }
                    break;
                }
            }
        }
    }
}

void db::storage_engine::store_position(const wsrep::gtid& gtid)
{
    validate_position(gtid);
    position_ = gtid;
}

wsrep::gtid db::storage_engine::get_position() const
{
    return position_;
}

void db::storage_engine::store_view(const wsrep::view& view)
{
    view_ = view;
}

wsrep::view db::storage_engine::get_view() const
{
    return view_;
}

void db::storage_engine::validate_position(const wsrep::gtid& gtid) const
{
    using std::rel_ops::operator<=;
    if (position_.id() == gtid.id() && gtid.seqno() <= position_.seqno())
    {
        std::ostringstream os;
        os << "Invalid position submitted, position seqno "
           << position_.seqno()
           << " is greater than submitted seqno "
           << gtid.seqno();
        throw wsrep::runtime_error(os.str());
    }
}
