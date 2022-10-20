#!/bin/bash
#for taobao abs
# Usage: obclient-build.sh <oceanbasepath> <package> <version> <release>
# Usage: obclient-build.sh

SCRIPT_DIR=$(cd "$(dirname "$0")";pwd)
TOP_DIR=${1:-${SCRIPT_DIR}/../}
DEP_DIR=${TOP_DIR}/deps/3rd/
PACKAGE=${2:-$(basename $0 -build.sh)}
VERSION=${3:-`cat ${PACKAGE}-VER.txt`}
RELEASE=${4:-1}

echo "[BUILD] args: TOP_DIR=${TOP_DIR} PACKAGE=${PACKAGE} VERSION=${VERSION} RELEASE=${RELEASE}"

# dep_create
cd $DEP_DIR
bash dep_create.sh
if [[ $? -ne 0 ]]; then
    echo "dep create failed"
    exit 1
fi
cd $SCRIPT_DIR

TMP_DIR=${TOP_DIR}/${PACKAGE}-tmp.$$
BOOST_DIR=${TMP_DIR}/BOOST

echo "[BUILD] create tmp dirs...TMP_DIR=${TMP_DIR}"
mkdir -p ${TMP_DIR}
mkdir -p ${TMP_DIR}/BUILD
mkdir -p ${TMP_DIR}/RPMS
mkdir -p ${TMP_DIR}/SOURCES
mkdir -p ${TMP_DIR}/SRPMS
mkdir -p $BOOST_DIR

#cp ./boost_1_59_0.tar.gz $BOOST_DIR
SPEC_FILE=${PACKAGE}.spec

echo "[BUILD] make rpms...dep_dir=$DEP_DIR spec_file=${SPEC_FILE}"
rpmbuild --define "_topdir ${TMP_DIR}" --define "NAME ${PACKAGE}" --define "VERSION ${VERSION}" --define "RELEASE ${RELEASE}" -ba $SPEC_FILE || exit 2
echo "[BUILD] make rpms done."

cd ${TOP_DIR}
find ${TMP_DIR}/RPMS/ -name "*.rpm" -exec mv '{}' ./rpm/ \;
rm -rf ${TMP_DIR}
