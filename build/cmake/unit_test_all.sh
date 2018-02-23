#!/bin/sh
#
# fon9/build/cmake/unit_test_all.sh
#

fon9path=`pwd`"$0"
fon9path="${fon9path%/*}"
fon9path="${fon9path%/*}"
fon9path="${fon9path%/*}"
develpath="${fon9path%/*}"
BUILD_DIR=${BUILD_DIR:-${develpath}/output/fon9}
BUILD_TYPE=${BUILD_TYPE:-release}

ulimit -c unlimited

set -x

OUTPUT_DIR=${OUTPUT_DIR:-${BUILD_DIR}/${BUILD_TYPE}/fon9}

$OUTPUT_DIR/Subr_UT
$OUTPUT_DIR/StrView_UT
$OUTPUT_DIR/StrTo_UT
$OUTPUT_DIR/ToStr_UT
