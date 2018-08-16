#!/bin/sh
#
# fon9/build/cmake/Fon9Co.sh
#

ulimit -c unlimited

set -x
set -e

~/devel/output/fon9/release/fon9/Fon9Co --workdir ~/devel/output/fon9
