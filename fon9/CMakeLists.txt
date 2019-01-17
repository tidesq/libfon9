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
 HostId.cpp

 BitvEncode.cpp
 BitvDecode.cpp

 CountDownLatch.cpp
 CyclicBarrier.cpp
 ThreadId.cpp
 Timer.cpp
 DefaultThreadPool.cpp
 SchTask.cpp

 Log.cpp
 ErrC.cpp
 Outcome.cpp
 Named.cpp
 Blob.c
 ByteVector.cpp
 Base64.cpp
 Random.cpp
 ConsoleIO.cpp
 CmdArgs.cpp
 ConfigLoader.cpp
 ConfigParser.cpp
 DllHandle.cpp
 PkCont.cpp

 File.cpp
 FilePath.cpp
 TimedFileName.cpp
 Appender.cpp
 FileAppender.cpp
 LogFile.cpp
 FdrNotify.cpp
 ConfigFileBinder.cpp
 FileRevRead.cpp

 InnFile.cpp
 InnSyncer.cpp
 InnSyncerFile.cpp
 InnDbf.cpp
 
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
 io/SocketClientConfig.cpp
 io/SocketClientDevice.cpp
 io/TcpClientBase.cpp
 io/TcpServerBase.cpp
 io/DgramBase.cpp

 io/FdrSocket.cpp
 io/FdrSocketClient.cpp
 io/FdrService.cpp
 io/FdrServiceEpoll.cpp
 io/FdrTcpClient.cpp
 io/FdrTcpServer.cpp
 io/FdrDgram.cpp
 io/FileIO.cpp

 web/HttpSession.cpp
 web/HttpParser.cpp
 web/HttpMessage.cpp
 web/UrlCodec.cpp
 web/HttpDate.cpp
 web/HttpHandler.cpp
 web/HttpHandlerStatic.cpp
 web/WebSocket.cpp
 web/WebSocketAuther.cpp
 web/WsSeedVisitor.cpp

 seed/SeedBase.cpp
 seed/Tab.cpp
 seed/Layout.cpp
 seed/Raw.cpp
 seed/Tree.cpp
 seed/TreeOp.cpp
 seed/PodOp.cpp
 seed/MaTree.cpp
 seed/NamedPark.cpp
 seed/Field.cpp
 seed/FieldMaker.cpp
 seed/FieldBytes.cpp
 seed/FieldChars.cpp
 seed/FieldDyBlob.cpp
 seed/FieldSchCfgStr.cpp
 seed/SeedSearcher.cpp
 seed/SeedAcl.cpp
 seed/SeedFairy.cpp
 seed/SeedVisitor.cpp
 seed/SysEnv.cpp
 seed/CloneTree.cpp
 seed/TabTreeOp.cpp
 seed/Plugins.cpp
 seed/PluginsMgr.cpp
 seed/ConfigGridView.cpp
 seed/SeedSubr.cpp

 crypto/Sha1.cpp
 crypto/Sha256.cpp
 auth/AuthMgr.cpp
 auth/RoleMgr.cpp
 auth/UserMgr.cpp
 auth/PolicyTable.cpp
 auth/PolicyTree.cpp
 auth/PolicyMaster.cpp 
 auth/PolicyAcl.cpp 
 auth/SaslClient.cpp
 auth/SaslScramServer.cpp
 auth/SaslScramSha256.cpp
 auth/SaslScramSha256Client.cpp
 auth/SaslScramSha256Server.cpp
 
 framework/Framework.cpp
 framework/SeedSession.cpp
 framework/IoManager.cpp
 framework/NamedIoManager.cpp
 framework/IoFactory.cpp
 framework/IoFactoryTcpClient.cpp
 framework/IoFactoryTcpServer.cpp
 framework/IoFactoryDgram.cpp
 framework/IoFactoryFileIO.cpp
 framework/SeedImporter.cpp

 fmkt/Symb.cpp
 fmkt/SymbTree.cpp
 fmkt/SymbDy.cpp
 fmkt/SymbIn.cpp
 fmkt/SymbRef.cpp
 fmkt/SymbBS.cpp
 fmkt/SymbDeal.cpp
 fmkt/Trading.cpp

 fix/FixBase.cpp
 fix/FixCompID.cpp
 fix/FixParser.cpp
 fix/FixBuilder.cpp
 fix/FixRecorder.cpp
 fix/FixRecorder_Searcher.cpp
 fix/FixFeeder.cpp
 fix/FixSender.cpp
 fix/FixReceiver.cpp
 fix/FixConfig.cpp
 fix/FixAdminMsg.cpp
 fix/FixBusinessReject.cpp
 fix/FixSession.cpp
 fix/IoFixSession.cpp
 fix/IoFixSender.cpp
)
add_library(fon9_s STATIC ${fon9src})
target_link_libraries(fon9_s pthread rt dl)

add_library(fon9 SHARED ${fon9src})
target_link_libraries(fon9 pthread rt dl)

add_executable(Fon9Co framework/Fon9Co.cpp)
target_link_libraries(Fon9Co pthread fon9)

# unit tests: Tools / Utility
add_executable(Subr_UT Subr_UT.cpp)
target_link_libraries(Subr_UT pthread)

add_executable(Base64_UT Base64_UT.cpp)
target_link_libraries(Base64_UT fon9_s)

add_executable(Endian_UT Endian_UT.cpp)
target_link_libraries(Endian_UT fon9_s)

add_executable(ConfigLoader_UT ConfigLoader_UT.cpp)
target_link_libraries(ConfigLoader_UT fon9_s)

add_executable(PkCont_UT PkCont_UT.cpp)
target_link_libraries(PkCont_UT fon9_s)

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

add_executable(PackBcd_UT PackBcd_UT.cpp)
target_link_libraries(PackBcd_UT fon9_s)

# unit tests: Bitv/Serialize/Deserialize
add_executable(Bitv_UT Bitv_UT.cpp)
target_link_libraries(Bitv_UT fon9_s)

# unit tests: Thread Tools
add_executable(ThreadController_UT ThreadController_UT.cpp)
target_link_libraries(ThreadController_UT fon9_s)

add_executable(Timer_UT Timer_UT.cpp)
target_link_libraries(Timer_UT fon9_s)

add_executable(AQueue_UT AQueue_UT.cpp)
target_link_libraries(AQueue_UT fon9_s)

add_executable(SchTask_UT SchTask_UT.cpp)
target_link_libraries(SchTask_UT fon9_s)

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

add_executable(InnDbf_UT InnDbf_UT.cpp)
target_link_libraries(InnDbf_UT fon9_s)

add_executable(FileRevRead_UT FileRevRead_UT.cpp)
target_link_libraries(FileRevRead_UT fon9_s)

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

# unit tests: crypto / auth
add_executable(Crypto_UT crypto/Crypto_UT.cpp)
target_link_libraries(Crypto_UT fon9_s)

add_executable(AuthMgr_UT auth/AuthMgr_UT.cpp)
target_link_libraries(AuthMgr_UT fon9_s)

# unit tests: fmkt
add_executable(Symb_UT fmkt/Symb_UT.cpp)
target_link_libraries(Symb_UT fon9_s)

# unit tests: fix
add_executable(FixParser_UT fix/FixParser_UT.cpp)
target_link_libraries(FixParser_UT fon9_s)

add_executable(FixRecorder_UT fix/FixRecorder_UT.cpp)
target_link_libraries(FixRecorder_UT fon9_s)

add_executable(FixFeeder_UT fix/FixFeeder_UT.cpp)
target_link_libraries(FixFeeder_UT fon9_s)

add_executable(FixSender_UT fix/FixSender_UT.cpp)
target_link_libraries(FixSender_UT fon9_s)

add_executable(FixReceiver_UT fix/FixReceiver_UT.cpp)
target_link_libraries(FixReceiver_UT fon9_s)

add_executable(IoFixSession_UT fix/IoFixSession_UT.cpp)
target_link_libraries(IoFixSession_UT fon9_s)
