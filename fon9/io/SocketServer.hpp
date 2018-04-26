/// \file fon9/io/SocketServer.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_SocketServer_hpp__
#define __fon9_io_SocketServer_hpp__
#include "fon9/io/Socket.hpp"
#include "fon9/io/IoServiceArgs.hpp"

namespace fon9 { namespace io {

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// 一般用於 Tcp Server 的設定.
struct fon9_API SocketServerConfig {
   SocketConfig   ListenConfig_;

   /// 使用 "ClientOptions={configs}" 提供設定.
   SocketOptions  AcceptedClientOptions_;

   /// listen() 的 backlog 參數, 使用 "ListenBacklog=n" 設定.
   /// - SetDefaults() = 5
   int   ListenBacklog_;

   /// \ref IoServiceArgs::FromTagValue(StrView tag, StrView value)
   /// - ServiceArgs_.ThreadCount_ 提供服務的 threads 數量.
   ///   - SetDefaults() = std::thread::hardware_concurrency() / 2.
   ///   - 若 std::thread::hardware_concurrency()==0, 則設為 2.
   /// - ServiceArgs_.Capacity_ 允許最大連線數量(正在連線未斷線的數量).
   ///   - 活著的連線數量==ServiceArgs_.Capacity_, 就會暫停 accept(), 交給 OS 去拒絕新進的連線.
   ///   - SetDefaults() = 0 = 不限制(或由 Server 決定).
   IoServiceArgs  ServiceArgs_;

   /// ListenConfig: SO_REUSEADDR = Y; SO_REUSEPORT = Y;
   /// 其餘使用預設值.
   void SetDefaults();

   StrView ParseConfig(StrView cfgstr, FnOnTagValue fnUnknownField, FnOnTagValue fnClientOptionsUnknownField = FnOnTagValue{});

   SocketResult CreateListenSocket(Socket& soListen) const;
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_SocketServer_hpp__
