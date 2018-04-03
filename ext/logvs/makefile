INC=-I../../../external/mini-async-log/include -I../../../external/spdlog/include -I../..
LIB=-L../../../external/mini-async-log -lmini_async_log -L../../../output/fon9/release/fon9 -lfon9_s

all:
	g++ -D NDEBUG -g -O3 -std=c++11 -pthread logvs.cpp NanoLog.cpp $(INC) $(LIB) -o logvs
