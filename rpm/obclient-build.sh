#!/bin/bash
#for taobao abs
# Usage: obclient-build.sh <oceanbasepath> <package> <version> <release>
# Usage: obclient-build.sh

REDHAT=`cat /etc/redhat-release|cut -d " " -f 7|cut -d "." -f 1`
if [ $# -ne 4 ]
then
  TOP_DIR=`pwd`/../
  PACKAGE=$(basename $0 -build.sh)
  VERSION=`cat ${PACKAGE}-VER.txt`
  RELEASE="dev.el${REDHAT}"
else
  TOP_DIR=$1
  PACKAGE=$2
  VERSION=$3
  RELEASE="$4.el${REDHAT}"
  export AONE_BUILD_NUMBER=${4}
fi

echo "[BUILD] args: TOP_DIR=${TOP_DIR} PACKAGE=${PACKAGE} VERSION=${VERSION} RELEASE=${RELEASE}"

TMP_DIR=${TOP_DIR}/${PACKAGE}-tmp.$$
BOOST_DIR=${TMP_DIR}/BOOST

echo "[BUILD] create tmp dirs...TMP_DIR=${TMP_DIR}"
mkdir -p ${TMP_DIR}
mkdir -p ${TMP_DIR}/BUILD
mkdir -p ${TMP_DIR}/RPMS
mkdir -p ${TMP_DIR}/SOURCES
mkdir -p ${TMP_DIR}/SRPMS
mkdir -p $BOOST_DIR

SPEC_FILE=${PACKAGE}.spec

echo "[BUILD] make rpms...dep_dir=$DEP_DIR spec_file=${SPEC_FILE}"
rpmbuild --define "_topdir ${TMP_DIR}" --define "NAME ${PACKAGE}" --define "VERSION ${VERSION}" --define "RELEASE ${RELEASE}" -ba $SPEC_FILE || exit 2
echo "[BUILD] make rpms done."

cd ${TOP_DIR}
find ${TMP_DIR}/RPMS/ -name "*.rpm" -exec mv '{}' ./rpm/ \;
rm -rf ${TMP_DIR}
