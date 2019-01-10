set(f9tws_src
 ExgMktFeeder.cpp
 ExgMktPlayer.cpp
 ExgTradingLineFix.cpp
 ExgTradingLineMgr.cpp
)
add_library(f9tws_s STATIC ${f9tws_src})
target_link_libraries(f9tws_s pthread rt dl)

add_library(f9tws SHARED ${f9tws_src})
target_link_libraries(f9tws pthread rt dl)

# unit tests
add_executable(f9twsExgMkt_UT ExgMkt_UT.cpp)
target_link_libraries(f9twsExgMkt_UT fon9_s f9tws_s)
