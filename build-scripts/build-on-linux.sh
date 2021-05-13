#!/bin/bash

# $1 - Version
# $2 - Build version
# $3 - source Dir. If not set then relative to the script dir

set -e

source $(dirname $0)/set-ver-prms.sh "$1" "$2"

if [[ -n "$3" ]]; then
	SRC_DIR=$3
else
	SRC_DIR=$(readlink -f $(dirname $0)/..)
fi

PARALLEL_PRMS="-j$(nproc)"

mkdir -p build
pushd build

rm -rf *
export LANG=C

echo "cmake -G \"Unix Makefiles\" $GO_PRMS $SRC_DIR"
cmake -G "Unix Makefiles" $CMAKE_VERSION_PRMS $SRC_DIR
make -k $PARALLEL_PRMS VERBOSE=1 package

# generate source rpm
cpack -G RPM --config ./CPackSourceConfig.cmake

popd

