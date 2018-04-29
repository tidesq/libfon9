/// \file fon9/io/Server.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_Server_hpp__
#define __fon9_io_Server_hpp__
#include "fon9/io/Device.hpp"

namespace fon9 { namespace io {

/// \ingroup io
/// 提供 DeviceServer 接受 AcceptedClient 連入後, 建立一個 Session 給 AcceptedClient 使用.
class fon9_API SessionServer : public Session {
   fon9_NON_COPY_NON_MOVE(SessionServer);
public:
   using Session::Session;
   SessionServer() = default;
   /// 當 DeviceServer(例: TcpServer) **建立新 Device 之前** 的通知.
   /// 必須傳回一個 Session, 用來處理新進的連線.
   /// 可能在任意 thread 呼叫, 必須自行考慮 thread safe 的問題.
   virtual SessionSP OnDevice_Accepted(DeviceServer& dev) = 0;
};
using SessionServerSP = SessionSPT<SessionServer>;

/// \ingroup io
/// Server類型的通訊設備基底.
class fon9_API DeviceServer : public Device {
   fon9_NON_COPY_NON_MOVE(DeviceServer);
   using base = Device;
public:
   DeviceServer(SessionServerSP ses, ManagerSP mgr, Style style = Style::Server)
      : base(std::move(ses), std::move(mgr), style) {
   }
   /// Server 不支援, 傳回: SysErr::function_not_supported
   virtual SendResult SendASAP(const void* src, size_t size) override;
   virtual SendResult SendASAP(BufferList&& src) override;
   virtual SendResult SendBuffered(const void* src, size_t size) override;
   virtual SendResult SendBuffered(BufferList&& src) override;
   /// Server 不支援, 傳回 true;
   virtual bool IsSendBufferEmpty() const override;
   SessionSP OnDevice_Accepted() {
      return static_cast<SessionServer*>(this->Session_.get())->OnDevice_Accepted(*this);
   }
};

} } // namespaces
#endif//__fon9_io_Server_hpp__
