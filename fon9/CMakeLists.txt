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

 File.cpp
 FilePath.cpp
 TimedFileName.cpp
 Appender.cpp
 FileAppender.cpp
 LogFile.cpp
 
 buffer/MemBlock.cpp
 buffer/BufferNode.cpp
 buffer/BufferList.cpp
 buffer/BufferNodeWaiter.cpp
 buffer/DcQueue.cpp
 buffer/DcQueueList.cpp
)
add_library(fon9_s STATIC ${fon9src})
target_link_libraries(fon9_s pthread rt)
add_library(fon9 SHARED ${fon9src})
target_link_libraries(fon9 pthread rt dl)

# unit tests
add_executable(Subr_UT Subr_UT.cpp)
target_link_libraries(Subr_UT pthread)

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

add_executable(ThreadController_UT ThreadController_UT.cpp)
target_link_libraries(ThreadController_UT fon9_s)

add_executable(Timer_UT Timer_UT.cpp)
target_link_libraries(Timer_UT fon9_s)

add_executable(Buffer_UT buffer/Buffer_UT.cpp)
target_link_libraries(Buffer_UT fon9_s)

add_executable(MemBlock_UT buffer/MemBlock_UT.cpp)
target_link_libraries(MemBlock_UT fon9_s)

add_executable(File_UT File_UT.cpp)
target_link_libraries(File_UT fon9_s)

add_executable(TimedFileName_UT TimedFileName_UT.cpp)
target_link_libraries(TimedFileName_UT fon9_s)

add_executable(LogFile_UT LogFile_UT.cpp)
target_link_libraries(LogFile_UT fon9_s)
