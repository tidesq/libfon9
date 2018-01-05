cmake_minimum_required(VERSION 2.6)
project(fon9)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

set(CXX_FLAGS
 -g
 -Wall
 -Wextra
 -Wconversion
 -Wold-style-cast
 -Woverloaded-virtual
 -Wpointer-arith
 -Wshadow
 -Wwrite-strings
 -fexec-charset=UTF-8
 -std=c++11
 )
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")
include_directories(.)

add_subdirectory(fon9)
