# set(fon9src
# )
# add_library(fon9_s STATIC ${fon9src})
# target_link_libraries(fon9_s pthread rt)
# add_library(fon9 SHARED ${fon9src})
# target_link_libraries(fon9 pthread rt dl)

# unit tests
add_executable(Subr_UT Subr_UT.cpp)
target_link_libraries(Subr_UT pthread)

add_executable(StrView_UT StrView_UT.cpp)
target_link_libraries(StrView_UT)
