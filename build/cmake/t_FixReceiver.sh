#!/bin/sh
#
# fon9/build/cmake/t_FixReceiver.sh
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

rm -f fix.log
$OUTPUT_DIR/FixReceiver_UT ./fix.log ../../fon9/fix/FixReceiver_UT_Case.txt

# 移除紀錄的 timestamp.
sed -E 's/(^. )[0-9]*\.[0-9]* /\1/' -i fix.log

# 移除送出訊息的 52=SendingTime, 及 10=CheckSum: 因為每次測試這2個欄位都不會相同.
sed -E 's/(^S .*52=)[0-9]*-[0-9]*:[0-9]*:[0-9]*\.[0-9]*(.*)10=[0-9]*.$/\1\2/' -i fix.log

diff fix.log ../../fon9/fix/FixReceiver_UT_Result.txt
if [ $? -eq 0 ]
then
   echo "[OK   ] FixReceiver_UT."
else
   echo "[ERROR] FixReceiver_UT."
fi
