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
set -e

OUTPUT_DIR=${OUTPUT_DIR:-${BUILD_DIR}/${BUILD_TYPE}/fon9}

# unit tests: Tools / Utility
$OUTPUT_DIR/Subr_UT
$OUTPUT_DIR/Base64_UT
$OUTPUT_DIR/Endian_UT
$OUTPUT_DIR/Bitv_UT
$OUTPUT_DIR/ConfigLoader_UT

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
$OUTPUT_DIR/SchTask_UT

# unit tests: buffer
$OUTPUT_DIR/Buffer_UT
$OUTPUT_DIR/MemBlock_UT

# unit tests: file / inn
$OUTPUT_DIR/File_UT
$OUTPUT_DIR/TimedFileName_UT
$OUTPUT_DIR/InnFile_UT
$OUTPUT_DIR/InnDbf_UT
./t_FileRevRead.sh  t_FileRevRead.sh

rm -rf ./logs
rm -rf /tmp/*log
rm -rf /tmp/*txt
$OUTPUT_DIR/LogFile_UT

# unit tests: io
$OUTPUT_DIR/Socket_UT

# unit tests: seed
$OUTPUT_DIR/Seed_UT
$OUTPUT_DIR/Tree_UT

# unit tests: crypt / auth
$OUTPUT_DIR/Crypto_UT
$OUTPUT_DIR/AuthMgr_UT

# unit tests: fmkt / fix
$OUTPUT_DIR/Symb_UT
$OUTPUT_DIR/FixParser_UT
$OUTPUT_DIR/FixRecorder_UT
$OUTPUT_DIR/FixFeeder_UT
$OUTPUT_DIR/FixSender_UT
./t_FixReceiver.sh

{ set +x; } 2>/dev/null
echo '#####################################################'
echo '#                 All tests passed!                 #'
echo '#####################################################'
