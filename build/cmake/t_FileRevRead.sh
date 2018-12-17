#!/bin/sh
#
# fon9/build/cmake/t_FileRevRead.sh
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
set -e

OUTPUT_DIR=${OUTPUT_DIR:-${BUILD_DIR}/${BUILD_TYPE}/fon9}

$OUTPUT_DIR/FileRevRead_UT $1 outf
tac $1 > outx
diff outf outx

if [ $? -eq 0 ]
then
   echo "[OK   ] FileRevRead_UT."
else
   echo "[ERROR] FileRevRead_UT."
fi
