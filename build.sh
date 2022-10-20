#!/bin/bash
function clean()
{
  rm -rf CMakeFiles CMakeCache.txt
}

function sw()
{
  local tmp=`pwd`;
  while true; do
    if [ x$tmp = 'x' ]; then
      break;
    fi;
    if [ -d $tmp/.dep_create ]; then
      DEP_DIR=$tmp/.dep_create;
      break;
    else
      if [ -d $tmp/rpm/.dep_create ]; then
        DEP_DIR=$tmp/rpm/.dep_create;
        break;
      fi;
    fi;
    tmp=${tmp%/*};
  done;
  echo "DEP_DIR = $DEP_DIR";
  [ -f $DEP_DIR/info ] && echo "base on `cat $DEP_DIR/info`"
}

function do_init()
{
  set -x
  cd rpm && dep_create obclient
  cd ..
}

clean
do_init

TOP_DIR=$(cd "$(dirname "$0")";pwd)
#DEP_DIR=${TOP_DIR}/rpm/.dep_create
OBCLIENT_VERSION=`cat rpm/obclient-VER.txt`

sw

cmake . \
-DCMAKE_INSTALL_PREFIX=/app/mariadb \
-DMYSQL_DATADIR=/data/mariadb \
-DSYSCONFDIR=/etc \
-DMYSQL_USER=mysql \
-DWITH_INNOBASE_STORAGE_ENGINE=0 \
-DWITH_ARCHIVE_STORAGE_ENGINE=0 \
-DWITH_BLACKHOLE_STORAGE_ENGINE=0 \
-DWITH_PARTITION_STORAGE_ENGINE=0 \
-DWITHOUT_MROONGA_STORAGE_ENGINE=1 \
-DWITH_DEBUG=0 \
-DWITH_READLINE=1 \
-DWITH_SSL=$DEP_DIR \
-DWITH_SSL_PATH=$DEP_DIR \
-DOPENSSL_ROOT_DIR=$DEP_DIR \
-DOPENSSL_INCLUDE_DIR=$DEP_DIR/include \
-DOPENSSL_SSL_LIBRARY=$DEP_DIR/lib/libssl.a \
-DOPENSSL_CRYPTO_LIBRARY=$DEP_DIR/lib/libcrypto.a \
-DCURSES_CURSES_H_PATH=$DEP_DIR/include \
-DCURSES_CURSES_LIBRARY=$DEP_DIR/lib/libcurses.a \
-DCURSES_FORM_LIBRARY=$DEP_DIR/lib/libform.a \
-DCURSES_HAVE_CURSES_H=$DEP_DIR/include/curses.h \
-DCURSES_INCLUDE_PATH=$DEP_DIR/include \
-DCURSES_LIBRARY=$DEP_DIR/lib/libcurses.a \
-DCURSES_NCURSES_LIBRARY=$DEP_DIR/lib/libncurses.a \
-DWITH_ZLIB=system \
-DWITH_LIBWRAP=0 \
-DENABLED_LOCAL_INFILE=1 \
-DMYSQL_UNIX_ADDR=/app/mariadb/mysql.sock \
-DDEFAULT_CHARSET=utf8 \
-DDEFAULT_COLLATION=utf8_general_ci \
-DWITHOUT_TOKUDB=1 \
-DWITHOUT_SERVER=1 \
-DOBCLIENT_VERSION=$OBCLIENT_VERSION 

make -j `cat /proc/cpuinfo | grep processor| wc -l`
