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

# unit tests: Tools / Utility
$OUTPUT_DIR/Subr_UT
$OUTPUT_DIR/Base64_UT
$OUTPUT_DIR/Endian_UT

# unit tests: Container / Algorithm
$OUTPUT_DIR/Trie_UT

# unit tests: AlNum
$OUTPUT_DIR/StrView_UT
$OUTPUT_DIR/StrTo_UT
$OUTPUT_DIR/ToStr_UT
$OUTPUT_DIR/TimeStamp_UT
$OUTPUT_DIR/RevPrint_UT
$OUTPUT_DIR/CharVector_UT

# unit tests: Thread Tools
$OUTPUT_DIR/ThreadController_UT
$OUTPUT_DIR/Timer_UT
$OUTPUT_DIR/AQueue_UT

# unit tests: buffer
$OUTPUT_DIR/Buffer_UT
$OUTPUT_DIR/MemBlock_UT

# unit tests: file
$OUTPUT_DIR/File_UT
$OUTPUT_DIR/TimedFileName_UT

rm -rf ./logs
rm -rf /tmp/*log
rm -rf /tmp/*txt
$OUTPUT_DIR/LogFile_UT

# unit tests: io
$OUTPUT_DIR/Socket_UT

# unit tests: seed
$OUTPUT_DIR/Seed_UT
$OUTPUT_DIR/Tree_UT
