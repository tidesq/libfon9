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
   SocketOptions  AcceptedSocketOptions_;
   DeviceOptions  AcceptedClientOptions_;

   /// listen() 的 backlog 參數.
   /// - SetDefaults() = 5
   int   ListenBacklog_;

   /// \ref IoServiceArgs::OnTagValue(StrView tag, StrView& value)
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

   class fon9_API Parser : public SocketConfig::Parser {
      fon9_NON_COPY_NON_MOVE(Parser);
      using BaseParser = SocketConfig::Parser;
      SocketServerConfig& Owner_;
   public:
      /// 若設定字串為 "port|...opts(tag=value)...", 則 this->AddrBind_ = port;
      Parser(SocketServerConfig& owner)
         : BaseParser{owner.ListenConfig_, owner.ListenConfig_.AddrBind_}
         , Owner_(owner) {
      }
      ~Parser();

      /// - "ListenBacklog=n"
      /// - "ClientOptions={configs}" 提供: AcceptedSocketOptions_, AcceptedClientOptions_
      /// - 其餘丟給 ListenConfig_.OnTagValue() 及 ServiceArgs_.OnTagValue();
      Result OnTagValue(StrView tag, StrView& value) override;

      /// 轉給 AcceptedSocketOptions_.OnTagValue(), AcceptedClientOptions_.OnTagValue();
      virtual Result OnTagValueClient(StrView tag, StrView& value);

      /// 預設 do nothing, 傳回 true.
      virtual bool OnErrorBreakClient(ErrorEventArgs& e);
   };

   bool CreateListenSocket(Socket& soListen, SocketResult& soRes) const;
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_SocketServer_hpp__
