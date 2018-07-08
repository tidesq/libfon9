#/bin/bash
set -e
set -x
g++ -O3 -DNDEBUG -std=c++11 main.cpp -I ~/devel/external/yas/include -I ~/devel/fon9 -L ~/devel/output/fon9/release/fon9 -lfon9_s -lpthread
./a.out 

