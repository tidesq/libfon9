#!/bin/sh
#
# fon9/build/cmake/test_infinity.sh
#

ulimit -c unlimited

set -x
set -e

while true
do
  $1
done
