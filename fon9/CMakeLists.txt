set(fon9src
 DecBase.cpp
 StrTools.cpp
 StrTo.cpp
 ToStr.cpp
 FmtDef.cpp
 ToStrFmt.cpp
 TimeInterval.cpp
 TimeStamp.cpp
 RevFormat.cpp

 CountDownLatch.cpp
 CyclicBarrier.cpp
 ThreadId.cpp
 Timer.cpp
 DefaultThreadPool.cpp
 
 Log.cpp
 ErrC.cpp
 Outcome.cpp
 Blob.c
 ByteVector.cpp
 Base64.cpp

 File.cpp
 FilePath.cpp
 TimedFileName.cpp
 Appender.cpp
 FileAppender.cpp
 LogFile.cpp
 FdrNotify.cpp
 InnFile.cpp
 
 buffer/MemBlock.cpp
 buffer/BufferNode.cpp
 buffer/BufferList.cpp
 buffer/BufferNodeWaiter.cpp
 buffer/DcQueue.cpp
 buffer/DcQueueList.cpp
 
 io/IoBase.cpp
 io/Session.cpp
 io/Manager.cpp
 io/Device.cpp
 io/Server.cpp
 io/SendBuffer.cpp
 io/RecvBuffer.cpp
 io/IoServiceArgs.cpp

 io/SocketAddress.cpp
 io/SocketAddressDN.cpp
 io/SocketConfig.cpp
 io/Socket.cpp
 io/SocketServer.cpp
 io/SocketClient.cpp
 io/TcpClientBase.cpp
 io/TcpServerBase.cpp

 io/FdrSocket.cpp
 io/FdrService.cpp
 io/FdrServiceEpoll.cpp
 io/FdrTcpClient.cpp
 io/FdrTcpServer.cpp

 seed/SeedBase.cpp
 seed/Tab.cpp
 seed/Layout.cpp
 seed/Named.cpp
 seed/Raw.cpp
 seed/Tree.cpp
 seed/TreeOp.cpp
 seed/PodOp.cpp

 seed/Field.cpp
 seed/FieldMaker.cpp
 seed/FieldBytes.cpp
 seed/FieldChars.cpp
 seed/FieldDyBlob.cpp
)
add_library(fon9_s STATIC ${fon9src})
target_link_libraries(fon9_s pthread rt)

add_library(fon9 SHARED ${fon9src})
target_link_libraries(fon9 pthread rt dl)

add_executable(Fon9Co seed/Fon9Co.cpp)
target_link_libraries(Fon9Co pthread fon9)

# unit tests: Tools / Utility
add_executable(Subr_UT Subr_UT.cpp)
target_link_libraries(Subr_UT pthread)

add_executable(Base64_UT Base64_UT.cpp)
target_link_libraries(Base64_UT fon9_s)

add_executable(Endian_UT Endian_UT.cpp)
target_link_libraries(Endian_UT fon9_s)

# unit tests: Container / Algorithm
add_executable(Trie_UT Trie_UT.cpp)
target_link_libraries(Trie_UT fon9_s)

# unit tests: AlNum
add_executable(StrView_UT StrView_UT.cpp)
target_link_libraries(StrView_UT)

add_executable(StrTo_UT StrTo_UT.cpp)
target_link_libraries(StrTo_UT fon9_s)

add_executable(ToStr_UT ToStr_UT.cpp)
target_link_libraries(ToStr_UT fon9_s)

add_executable(TimeStamp_UT TimeStamp_UT.cpp)
target_link_libraries(TimeStamp_UT fon9_s)

add_executable(RevPrint_UT RevPrint_UT.cpp)
target_link_libraries(RevPrint_UT fon9_s)

add_executable(CharVector_UT CharVector_UT.cpp)
target_link_libraries(CharVector_UT fon9_s)

# unit tests: Thread Tools
add_executable(ThreadController_UT ThreadController_UT.cpp)
target_link_libraries(ThreadController_UT fon9_s)

add_executable(Timer_UT Timer_UT.cpp)
target_link_libraries(Timer_UT fon9_s)

add_executable(AQueue_UT AQueue_UT.cpp)
target_link_libraries(AQueue_UT fon9_s)

# unit tests: buffer
add_executable(Buffer_UT buffer/Buffer_UT.cpp)
target_link_libraries(Buffer_UT fon9_s)

add_executable(MemBlock_UT buffer/MemBlock_UT.cpp)
target_link_libraries(MemBlock_UT fon9_s)

# unit tests: file
add_executable(File_UT File_UT.cpp)
target_link_libraries(File_UT fon9_s)

add_executable(TimedFileName_UT TimedFileName_UT.cpp)
target_link_libraries(TimedFileName_UT fon9_s)

add_executable(LogFile_UT LogFile_UT.cpp)
target_link_libraries(LogFile_UT fon9_s)

add_executable(InnFile_UT InnFile_UT.cpp)
target_link_libraries(InnFile_UT fon9_s)

# unit tests: io
add_executable(Socket_UT io/Socket_UT.cpp)
target_link_libraries(Socket_UT fon9_s)

add_executable(IoDev_UT io/IoDev_UT.cpp)
target_link_libraries(IoDev_UT fon9_s)

# unit tests: seed
add_executable(Seed_UT seed/Seed_UT.cpp)
target_link_libraries(Seed_UT fon9_s)

add_executable(Tree_UT seed/Tree_UT.cpp)
target_link_libraries(Tree_UT fon9_s)
