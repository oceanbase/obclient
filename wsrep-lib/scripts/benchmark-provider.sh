#!/bin/bash -eu
#
# Copyright (C) 2018 Codership Oy <info@codership.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
# USA.
#

set -x
PROVIDER=${PROVIDER:?"Provider library is required"}

total_transactions=$((1 << 17))

function run_benchmark()
{
    servers=$1
    clients=$2
    transactions=$(($total_transactions/($servers*$clients)))
    result_file=dbms-bench-$(basename $PROVIDER)-$servers-$clients
    echo "Running benchmark for servers: $servers clients $clients"
    rm -r *_data/ || :
    command time -v ./src/dbms_simulator --servers=$servers --clients=$clients --transactions=$transactions --wsrep-provider="$PROVIDER" --fast-exit=1 >& $result_file
}


for servers in 1 2 4 8 16
# for servers in 16
do
    for clients in 1 2 4 8 16 32
#     for clients in 4 8 16 32
    do
	run_benchmark $servers $clients
    done
done
