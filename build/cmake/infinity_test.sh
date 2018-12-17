#!/bin/sh
#
# fon9/build/cmake/infinity_test.sh
#

ulimit -c unlimited

set -x
set -e

while true
do
  $*
done
