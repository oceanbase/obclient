#!/bin/bash

# Detect platform
OS_KERNEL="$(uname -s)"

function clean()
{
  rm -rf CMakeFiles CMakeCache.txt
}
clean

TOP_DIR=$(cd "$(dirname "$0")";pwd)
DEP_DIR=${TOP_DIR}/deps/3rd/usr/local/oceanbase/deps/devel

# Download and extract third-party dependencies
echo "=== Downloading dependencies ==="
(cd "${TOP_DIR}/deps/3rd" && bash dep_create.sh)
if [[ $? -ne 0 ]]; then
  echo "Failed to download dependencies" 1>&2
  exit 1
fi

# Common cmake options (shared between platforms)
CMAKE_COMMON_OPTS=(
  -DCMAKE_INSTALL_PREFIX=/u01/obclient
  -DMYSQL_DATADIR=/data/mariadb
  -DSYSCONFDIR=/etc
  -DMYSQL_USER=mysql
  -DWITH_UNIT_TESTS=0
  -DWITHOUT_SERVER=1
  -DWITH_INNOBASE_STORAGE_ENGINE=0
  -DWITH_ARCHIVE_STORAGE_ENGINE=0
  -DWITH_BLACKHOLE_STORAGE_ENGINE=0
  -DWITH_PARTITION_STORAGE_ENGINE=0
  -DWITHOUT_MROONGA_STORAGE_ENGINE=1
  -DWITH_DEBUG=0
  -DWITH_READLINE=1
  -DWITH_SSL=$DEP_DIR
  -DWITH_SSL_PATH=$DEP_DIR
  -DOPENSSL_ROOT_DIR=$DEP_DIR
  -DOPENSSL_INCLUDE_DIR=$DEP_DIR/include
  -DOPENSSL_SSL_LIBRARY=$DEP_DIR/lib/libssl.a
  -DOPENSSL_CRYPTO_LIBRARY=$DEP_DIR/lib/libcrypto.a
  -DWITH_ZLIB=system
  -DWITH_LIBWRAP=0
  -DENABLED_LOCAL_INFILE=1
  -DMYSQL_UNIX_ADDR=/app/mariadb/mysql.sock
  -DDEFAULT_CHARSET=utf8
  -DDEFAULT_COLLATION=utf8_general_ci
  -DWITHOUT_TOKUDB=1
  -DWITHOUT_SERVER=1
  -DOBCLIENT_VERSION=$OBCLIENT_VERSION
)

if [[ "$OS_KERNEL" == "Darwin" ]]; then
  # macOS: use system ncurses, no CURSES_* overrides needed
  # cmake FindCurses will locate system ncurses automatically
  CMAKE_PLATFORM_OPTS=()
else
  # Linux: use bundled ncurses from deps
  CMAKE_PLATFORM_OPTS=(
    -DCURSES_CURSES_H_PATH=$DEP_DIR/include
    -DCURSES_CURSES_LIBRARY=$DEP_DIR/lib/libcurses.a
    -DCURSES_HAVE_CURSES_H=$DEP_DIR/include/curses.h
    -DCURSES_INCLUDE_PATH=$DEP_DIR/include
    -DCURSES_LIBRARY=$DEP_DIR/lib/libcurses.a
    -DCURSES_NCURSES_LIBRARY=$DEP_DIR/lib/libncurses.a
    -DCURSES_FORM_LIBRARY=$DEP_DIR/lib/libform.a
  )
fi

cmake . "${CMAKE_COMMON_OPTS[@]}" "${CMAKE_PLATFORM_OPTS[@]}"

# Parallel build: detect CPU count per platform
if [[ "$OS_KERNEL" == "Darwin" ]]; then
  make -j $(sysctl -n hw.ncpu)
else
  make -j $(cat /proc/cpuinfo | grep processor | wc -l)
fi
