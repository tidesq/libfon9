/// \file fon9/io/TcpServerBase.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_TcpServerBase_hpp__
#define __fon9_io_TcpServerBase_hpp__
#include "fon9/io/Server.hpp"
#include "fon9/io/SocketServer.hpp"

namespace fon9 { namespace io {

/// \ingroup io
/// 透過 TcpServer 連入的 Device 基底.
/// - 遠端連入後建立的對應 Device.
/// - 一旦斷線(LinkBroken, LinkError, Close...) 就會觸發 Dispose 最終會刪除對應的 Device.
class fon9_API AcceptedClientDeviceBase : public Device {
   fon9_NON_COPY_NON_MOVE(AcceptedClientDeviceBase);
   using base = Device;

protected:
   /// AcceptedClientDeviceBase 不可能執行到這兒, 所以這裡 do nothing.
   virtual void OpImpl_Reopen() override;

   /// 僅能在尚未 LinkReady 時執行, 一旦 LinkReady 就不會做任何事情!
   /// 在 Server 接受連線後, 會將 Device 設定好, 所以這裡的參數為 Server 提供的 deviceId.
   virtual void OpImpl_Open(std::string deviceId) override;

   // virtual void OpImpl_Close(std::string cause) override {
   //    shutdown(this->Socket_.GetSocketHandle(), ...);
   //    OpThr_DisposeNoClose(*this, std::move(cause));
   // }

public:
   AcceptedClientDeviceBase(SessionSP ses, ManagerSP mgr)
      : base(std::move(ses), std::move(mgr), Style::AcceptedClient) {
   }
};

/// \ingroup io
/// TcpServer 使用基底.
/// - Windows: 衍生出 IocpTcpServer.
/// - POSIX: 衍生出 FdrTcpServer.
template <class Listener, class IoServiceSP>
class TcpServerBase : public DeviceServer {
   fon9_NON_COPY_NON_MOVE(TcpServerBase);
   using base = DeviceServer;

   template <class DeviceT>
   friend bool OpThr_ParseDeviceConfig(DeviceT& dev, StrView cfgstr, FnOnTagValue fnUnknownField);
   SocketServerConfig   Config_;

   friend Listener;//為了讓 Listener 可以取得 Config_; 及 Device 相關的保護操作.
   using ListenerSP = intrusive_ptr<Listener>;
   ListenerSP  Listener_;

   void OpImpl_DisposeListener(std::string cause) {
      if (ListenerSP listener = std::move(this->Listener_))
         listener->Dispose(std::move(cause));
   }
   void OpImpl_CreateListener() {
      SocketResult soRes;
      this->Listener_ = Listener::CreateListener(this, soRes);
      if (this->Listener_) // 建立 Listener 成功時, 會設定 DeviceId, 所以這裡設定 State 時, 沒有額外訊息!
         this->OpImpl_SetState(State::Listening, StrView{});
      else {
         std::string errmsg{RevPrintTo<std::string>("TcpServer.Open|", soRes)};
         this->OpImpl_SetState(State::LinkError, &errmsg);
      }
   }
   virtual void OpImpl_Reopen() override {
      this->OpImpl_DisposeListener("Reopen");
      this->OpImpl_CreateListener();
   }
   virtual void OpImpl_Open(std::string cfgstr) override {
      this->OpImpl_DisposeListener("Open");
      if (!OpThr_ParseDeviceConfig(*this, &cfgstr, FnOnTagValue{}))
         return;
      // "|L=ip:port" or "|L=[ip6]:port"
      char    uidbuf[kMaxTcpConnectionUID];
      StrView uidstr = MakeTcpConnectionUID(uidbuf, nullptr, &this->Config_.ListenConfig_.AddrBind_);
      OpThr_SetDeviceId(*this, uidstr.ToString());
      this->OpImpl_CreateListener();
   }
   virtual void OpImpl_Close(std::string cause) override {
      this->OpImpl_SetState(State::Closing, &cause);
      this->OpImpl_DisposeListener(cause);
      this->OpImpl_SetState(State::Closed, &cause);
   }
   /// 加上 "|ClientCount=n/m".
   /// - n = 現在連線的 client 數量.
   /// - m = 最大連線數量.
   virtual void OpImpl_AppendDeviceInfo(std::string& info) override {
      RevBufferList   rbuf{128 + sizeof(fon9::NumOutBuf)};
      if (this->Config_.ServiceArgs_.Capacity_ > 0)
         RevPrint(rbuf, '/', this->Config_.ServiceArgs_.Capacity_);
      size_t curClientCount = (this->Listener_ ? this->Listener_->GetCurrentConnectionCount() : 0);
      RevPrint(rbuf, "|ClientCount=", curClientCount);
      BufferAppendTo(rbuf.MoveOut(), info);
   }

   static void OpThr_CloseAcceptedClient(Device& dev, std::string acceptedClientId) {
      if (Listener* listener = static_cast<TcpServerBase*>(&dev)->Listener_.get())
         listener->OpImpl_CloseAcceptedClient(&acceptedClientId);
   }
   static void OpThr_LingerCloseAcceptedClient(Device& dev, std::string acceptedClientId) {
      if (Listener* listener = static_cast<TcpServerBase*>(&dev)->Listener_.get())
         listener->OpImpl_LingerCloseAcceptedClient(&acceptedClientId);
   }
   /// aclose  acceptedClientId cause
   /// alclose acceptedClientId cause
   virtual std::string OnDevice_Command(StrView cmd, StrView param) override {
      if (cmd == "aclose")
         this->OpQueue_.AddTask(DeviceAsyncOp(&OpThr_CloseAcceptedClient, param.ToString()));
      else if (cmd == "alclose")
         this->OpQueue_.AddTask(DeviceAsyncOp(&OpThr_LingerCloseAcceptedClient, param.ToString()));
      else
         return base::OnDevice_Command(cmd, param);
      return std::string{};
   }

public:
   const IoServiceSP IoServiceSP_;

   /// iosv 可為 nullptr, 此時建立 Listener 時, 會自動根據 Config_.ServiceArgs_ 建立 iosv.
   TcpServerBase(IoServiceSP iosv, SessionServerSP ses, ManagerSP mgr)
      : base(std::move(ses), std::move(mgr))
      , IoServiceSP_{std::move(iosv)} {
   }
};

} } // namespaces
#endif//__fon9_io_TcpServerBase_hpp__
