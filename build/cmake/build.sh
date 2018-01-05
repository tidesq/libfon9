#!/bin/sh
#
# fon9/build/cmake/build.sh
#

fon9path=`pwd`"$0"
fon9path="${fon9path%/*}"
fon9path="${fon9path%/*}"
fon9path="${fon9path%/*}"
develpath="${fon9path%/*}"

set -x

SOURCE_DIR=${fon9path}
BUILD_DIR=${BUILD_DIR:-${develpath}/output}
BUILD_TYPE=${BUILD_TYPE:-release}
INSTALL_DIR=${INSTALL_DIR:-${develpath}/${BUILD_TYPE}}

mkdir -p $BUILD_DIR/$BUILD_TYPE \
  && cd $BUILD_DIR/$BUILD_TYPE \
  && cmake \
           -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
           -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR \
           $SOURCE_DIR \
  && make $*
