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

#include "wsrep/logger.hpp"

#include <iostream>

std::ostream& wsrep::log::os_ = std::cout;
static wsrep::default_mutex log_mutex_;
wsrep::mutex& wsrep::log::mutex_ = log_mutex_;
wsrep::log::logger_fn_type wsrep::log::logger_fn_ = 0;
std::atomic_int wsrep::log::debug_log_level_(0);

void wsrep::log::logger_fn(wsrep::log::logger_fn_type logger_fn)
{
    logger_fn_ = logger_fn;
}

void wsrep::log::debug_log_level(int debug_log_level)
{
    debug_log_level_.store(debug_log_level, std::memory_order_relaxed);
}

int wsrep::log::debug_log_level()
{
    return debug_log_level_.load(std::memory_order_relaxed);
}
