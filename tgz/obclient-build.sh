#!/bin/bash
#
# Build obclient and create tar.gz package for macOS
# Mirrors the behavior of rpm/obclient-build.sh + obclient.spec for Linux RPM.
#
# Usage:
#   ./obclient-build.sh                              # dev build
#   ./obclient-build.sh <top_dir> <pkg> <ver> <rel>  # CI build
#

set -e

OS_KERNEL="$(uname -s)"
if [[ "$OS_KERNEL" != "Darwin" ]]; then
  echo "[ERROR] This script is for macOS only." >&2
  exit 1
fi

OS_ARCH="$(uname -m)"  # arm64 / x86_64

# ---------------------------------------------------------------------------
# Parse arguments (same convention as rpm/obclient-build.sh)
# ---------------------------------------------------------------------------
if [ $# -ne 4 ]; then
  TOP_DIR=$(cd "$(dirname "$0")/.."; pwd)
  PACKAGE="obclient"
  VERSION=$(cat "${TOP_DIR}/rpm/obclient-VER.txt")
  RELEASE="dev.darwin.${OS_ARCH}"
else
  TOP_DIR=$1
  PACKAGE=$2
  VERSION=$3
  RELEASE="$4.darwin.${OS_ARCH}"
fi

PREFIX=/u01/obclient
PKG_NAME="${PACKAGE}-${VERSION}-${RELEASE}"
STAGING_DIR="${TOP_DIR}/${PACKAGE}-staging.$$"

echo "============================================================"
echo "[BUILD] macOS tar.gz packaging"
echo "[BUILD] TOP_DIR  = ${TOP_DIR}"
echo "[BUILD] PACKAGE  = ${PACKAGE}"
echo "[BUILD] VERSION  = ${VERSION}"
echo "[BUILD] RELEASE  = ${RELEASE}"
echo "[BUILD] PKG_NAME = ${PKG_NAME}"
echo "============================================================"

# ---------------------------------------------------------------------------
# Step 1: Build
# ---------------------------------------------------------------------------
echo ""
echo "[BUILD] Step 1/4: Building obclient ..."
cd "${TOP_DIR}"
bash build.sh
echo "[BUILD] Build completed."

# ---------------------------------------------------------------------------
# Step 2: Install to staging directory
# ---------------------------------------------------------------------------
echo ""
echo "[BUILD] Step 2/4: Installing to staging directory ..."
mkdir -p "${STAGING_DIR}"
make DESTDIR="${STAGING_DIR}" install
echo "[BUILD] Install completed."

# ---------------------------------------------------------------------------
# Step 3: Clean up staging directory
#         (mirrors %install section in rpm/obclient.spec)
# ---------------------------------------------------------------------------
echo ""
echo "[BUILD] Step 3/4: Cleaning up staging directory ..."

# Remove .git artifacts
find "${STAGING_DIR}" -name '.git' -type d -print0 | xargs -0 rm -rf 2>/dev/null || true

# Keep only the bin directory (same as spec: grep -v "bin")
for dir in $(ls "${STAGING_DIR}${PREFIX}" | grep -v "bin"); do
  rm -rf "${STAGING_DIR}${PREFIX}/${dir}"
done

# Remove binaries not present in RPM; keep the same file set as RPM:
#   msql2mysql, mysql_config, mysql_find_rows, mysql_waitpid,
#   mysqlaccess, mysqldump, mysqlimport, mysqltest, mytop, obclient
rm -rf "${STAGING_DIR}${PREFIX}/bin/mariadb"*
rm -rf "${STAGING_DIR}${PREFIX}/bin/obclient_config_editor"

echo "[BUILD] Remaining files in package:"
ls -la "${STAGING_DIR}${PREFIX}/bin/"

# ---------------------------------------------------------------------------
# Step 4: Create tar.gz
#         Keep the same directory layout as RPM: u01/obclient/bin/...
#         so that `tar xzf <pkg>.tar.gz -C /` installs to the same path.
# ---------------------------------------------------------------------------
echo ""
echo "[BUILD] Step 4/4: Creating tar.gz package ..."

# Ensure output directory exists
mkdir -p "${TOP_DIR}/tgz"

# Create tar.gz preserving the u01/obclient/ prefix (same as RPM)
cd "${STAGING_DIR}"
tar -czf "${TOP_DIR}/tgz/${PKG_NAME}.tar.gz" .${PREFIX}

echo "[BUILD] Cleaning up staging directory ..."
rm -rf "${STAGING_DIR}"

echo ""
echo "============================================================"
echo "[BUILD] SUCCESS"
echo "[BUILD] Package: tgz/${PKG_NAME}.tar.gz"
echo ""
echo "[BUILD] Package contents (same layout as RPM):"
echo "  u01/obclient/"
echo "  u01/obclient/bin/"
echo "  u01/obclient/bin/obclient"
echo "  u01/obclient/bin/mysql_config"
echo "  ..."
echo ""
echo "[USAGE] Install to default path (same as RPM):"
echo "  sudo tar -xzf ${PKG_NAME}.tar.gz -C /"
echo ""
echo "[USAGE] Install to custom path:"
echo "  mkdir -p ~/obclient && tar -xzf ${PKG_NAME}.tar.gz -C ~/obclient --strip-components=2"
echo "  export PATH=~/obclient/bin:\$PATH"
echo "============================================================"
