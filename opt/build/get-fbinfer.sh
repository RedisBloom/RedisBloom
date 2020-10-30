#!/usr/bin/env bash

set -e

[[ $VERBOSE == 1 ]] && set -x

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

ROOT=$HERE/../..
ROOT=$(realpath $ROOT)
OS=$(python3 $ROOT/opt/readies/bin/platform --os)

VERSION=${VERSION:-1.0.0}
TARGET_DIR=${TARGET_DIR:-"$ROOT/deps"}

mkdir -p ${TARGET_DIR}
FBINFER_ARCHIVE=infer-${OS}64-v$VERSION.tar.xz

if [ ! -f ${FBINFER_ARCHIVE} ]; then
    echo "Downloading fbinfer to ${TARGET_DIR}/${FBINFER_ARCHIVE}"
    wget --quiet --show-progress --progress=bar:force:noscroll --no-check-certificate \
        https://github.com/facebook/infer/releases/download/v$VERSION/${FBINFER_ARCHIVE}
fi

tar xvf ${FBINFER_ARCHIVE} -C ${TARGET_DIR}/

if [ ! -f ${TARGET_DIR}/infer ]; then
    echo "Linking fbinfer ${TARGET_DIR}/infer-${OS}64-v$VERSION/bin/infer to ${TARGET_DIR}/infer"
    ln -s "${TARGET_DIR}/infer-${OS}64-v$VERSION/bin/infer" ${TARGET_DIR}/infer
fi

rm -rf ${FBINFER_ARCHIVE}
