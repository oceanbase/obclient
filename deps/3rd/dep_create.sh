#!/bin/bash

#clear env
unalias -a

PWD="$(cd $(dirname $0); pwd)"
PROJECT_NAME="obclient"

# Detect OS and architecture
OS_KERNEL="$(uname -s)"
if [[ "$OS_KERNEL" == "Darwin" ]]; then
    OS_ARCH="$(uname -m)"
    OS_TAG="darwin.${OS_ARCH}"
elif [[ -f /etc/redhat-release ]]; then
    OS_RELEASE="$(grep -Po '(?<=release )\d' /etc/redhat-release)" || exit 1
    OS_ARCH="$(uname -p)" || exit 1
    OS_TAG="el${OS_RELEASE}.${OS_ARCH}"
else
    echo "Unsupported OS: cannot detect platform" 1>&2
    exit 1
fi

DEP_FILE="$PROJECT_NAME.${OS_TAG}.deps"

echo -e "check dependencies profile for ${OS_TAG}... \c"

if [[ ! -f "${DEP_FILE}" ]]; then
    echo "NOT FOUND" 1>&2
    exit 2
else
    echo "FOUND"
fi

mkdir "${PWD}/pkg" >/dev/null 2>&1

echo -e "check repository address in profile... \c"
# Use portable grep+sed instead of grep -P for macOS compatibility
REPO="$(grep '^repo=' "${DEP_FILE}" 2>/dev/null | sed 's/^repo=//')"
if [[ -n "$REPO" ]]; then
    echo "$REPO"
else
    echo "NOT FOUND" 1>&2
    exit 3
fi

echo "download dependencies..."

if [[ "$OS_KERNEL" == "Darwin" ]]; then
    #
    # macOS: download and extract tar.gz packages using curl + tar
    #
    PKGS="$(grep '\.tar\.gz' "${DEP_FILE}" | grep -v '^#')"

    for pkg_entry in $PKGS
    do
      if [[ $pkg_entry == http:* || $pkg_entry == https:* ]]; then
        URL="$pkg_entry"
        pkg="${pkg_entry##*/}"
      else
        URL="$REPO/${pkg_entry}"
        pkg="$pkg_entry"
      fi
      if [[ -f "${PWD}/pkg/${pkg}" ]]; then
        echo "find package <${pkg}> in cache"
      else
        echo -e "download package <${pkg}>... \c"
        TEMP=$(mktemp "${PWD}/pkg/.${pkg}.XXXX")
        curl -sL "${URL}" -o "${TEMP}"
        if [[ $? -eq 0 ]]; then
          mv -f "${TEMP}" "${PWD}/pkg/${pkg}"
          echo "SUCCESS"
        else
          rm -rf "${TEMP}"
          echo "FAILED" 1>&2
          exit 4
        fi
      fi
      echo -e "unpack package <${pkg}>... \c"
      tar -xzf "${PWD}/pkg/${pkg}" -C "${PWD}"

      if [[ $? -eq 0 ]]; then
        echo "SUCCESS"
      else
        echo "FAILED" 1>&2
        exit 5
      fi
    done
else
    #
    # Linux: download and extract rpm packages using wget + rpm2cpio
    #
    RPMS="$(grep '\.rpm' "${DEP_FILE}" | grep -v '^#')"
    URL=""

    for pkg in $RPMS
    do
      if [[ $pkg == http:* || $pkg == https:* ]]; then
        URL="$pkg"
        pkg="${pkg##*/}"
      else
        URL="$REPO/${pkg}"
        pkg="$pkg"
      fi
      if [[ -f "${PWD}/pkg/${pkg}" ]]; then
        echo "find package <${pkg}> in cache"
      else
        echo -e "download package <${pkg}>... \c"
        TEMP=$(mktemp -p "/" -u ".${pkg}.XXXX")
        wget "${URL}" -q -O "${PWD}/pkg/${TEMP}"
        if [[ $? -eq 0 ]]; then
          mv -f "${PWD}/pkg/$TEMP" "${PWD}/pkg/${pkg}"
          echo "SUCCESS"
        else
          rm -rf "${PWD}/pkg/$TEMP"
          echo "FAILED" 1>&2
          exit 4
        fi
      fi
      echo -e "unpack package <${pkg}>... \c"
      rpm2cpio "${PWD}/pkg/${pkg}" | cpio -di -u --quiet

      if [[ $? -eq 0 ]]; then
        echo "SUCCESS"
      else
        echo "FAILED" 1>&2
        exit 5
      fi
    done
fi
