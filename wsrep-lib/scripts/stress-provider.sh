#!/bin/bash -eu
#
# Copyright (C) 2019 Codership Oy <info@codership.com>
#
# This file is part of wsrep-lib.
#
# Wsrep-lib is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# Wsrep-lib is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with wsrep-lib.  If not, see <https://www.gnu.org/licenses/>.
#

set -x
WSREP_PROVIDER=${WSREP_PROVIDER:?"Provider library is required"}

transactions=$((1 << 20))
clients=16
servers=3
rows=100
alg_freq=100

./dbsim/dbsim \
	--servers=$servers \
	--clients=$clients \
	--transactions=$(($transactions/($servers*$clients))) \
	--rows=$rows \
	--alg-freq=$alg_freq \
	--wsrep-provider=$WSREP_PROVIDER

rm -r dbsim_1_data dbsim_2_data dbsim_3_data
