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

PROVIDER=${PROVIDER:?"Provider library is required"}

function report()
{
    servers=$1
    clients=$2
    result_file=dbms-bench-$(basename $PROVIDER)-$servers-$clients
    trx_per_sec=$(grep -a "Transactions per second" "$result_file" | cut -d ':' -f 2)
    cpu=$(grep -a "Percent of CPU this job got" "$result_file" | cut -d ':' -f 2)
    vol_ctx_switches=$(grep -a "Voluntary context switches" "$result_file" | cut -d ':' -f 2)
    invol_ctx_switches=$(grep -a "Involuntary context switches" "$result_file" | cut -d ':' -f 2)
    echo "$clients $cpu $trx_per_sec $vol_ctx_switches $invol_ctx_switches"
}

for servers in 1 2 4 8 16
do
    echo "Servers: $servers"
    for clients in 1 2 4 8 16 32
    do
	report $servers $clients
    done
done

