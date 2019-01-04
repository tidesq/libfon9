/// \file fon9/io/TcpServerBase.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_TcpServerBase_hpp__
#define __fon9_io_TcpServerBase_hpp__
#include "fon9/io/Server.hpp"
#include "fon9/io/SocketServer.hpp"
#include "fon9/io/DeviceStartSend.hpp"
#include "fon9/io/DeviceParseConfig.hpp"

namespace fon9 { namespace io {

/// \ingroup io
/// - 協助 DeviceAcceptedClientBase 完成:
///   - Device member function: `virtual bool IsSendBufferEmpty() const override;`
/// - 協助完成 `DeviceImpl_DeviceStartSend<>` 所需要的 `struct SendAuxImpl`;
template <class DeviceAcceptedClientBase>
class DeviceAcceptedClientWithSend : public DeviceAcceptedClientBase {
   fon9_NON_COPY_NON_MOVE(DeviceAcceptedClientWithSend);
public:
   using DeviceAcceptedClientBase::DeviceAcceptedClientBase;

   virtual bool IsSendBufferEmpty() const override {
      bool res;
      this->OpQueue_.InplaceOrWait(AQueueTaskKind::Send, DeviceAsyncOp{[&res](Device& dev) {
         res = static_cast<DeviceAcceptedClientBase*>(&dev)->GetSendBuffer().IsEmpty();
      }});
      return res;
   }

   template <class SendAuxBase>
   struct SendAuxImpl : public SendAuxBase {
      using SendAuxBase::SendAuxBase;
      SendAuxImpl() = delete;

      static SendBuffer& GetSendBufferAtLocked(DeviceAcceptedClientBase& dev) {
         return dev.GetSendBuffer();
      }
      void AsyncSend(DeviceAcceptedClientBase&, StartSendChecker& sc, ObjHolderPtr<BufferList>&& pbuf) {
         sc.AsyncSend(std::move(pbuf));
      }
   };
};

/// \ingroup io
/// TcpServer 使用基底.
/// - Windows: 衍生出 IocpTcpServer.
/// - POSIX: 衍生出 FdrTcpServer.
template <class Listener, class IoServiceSP>
class TcpServerBase : public DeviceServer {
   fon9_NON_COPY_NON_MOVE(TcpServerBase);
   using base = DeviceServer;
   friend Listener;//為了讓 Listener 可以取得 Config_; 及 Device 相關的保護操作.

   template <class DeviceT>
   friend bool OpThr_DeviceParseConfig(DeviceT& dev, StrView cfgstr);
   SocketServerConfig   Config_;
   DeviceListenerSP     Listener_;

   void OpImpl_DisposeListener(std::string cause) {
      if (DeviceListenerSP listener = std::move(this->Listener_))
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
      if (!OpThr_DeviceParseConfig(*this, &cfgstr))
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
      size_t curClientCount = (this->Listener_ ? this->Listener_->GetConnectionCount() : 0);
      RevPrint(rbuf, "|ClientCount=", curClientCount);
      BufferAppendTo(rbuf.MoveOut(), info);
   }

   static void OpThr_CloseAcceptedClient(Device& dev, std::string acceptedClientSeqAndOthers) {
      if (auto* listener = static_cast<TcpServerBase*>(&dev)->Listener_.get())
         listener->OpImpl_CloseAcceptedClient(&acceptedClientSeqAndOthers);
   }
   static void OpThr_LingerCloseAcceptedClient(Device& dev, std::string acceptedClientSeqAndOthers) {
      if (auto* listener = static_cast<TcpServerBase*>(&dev)->Listener_.get())
         listener->OpImpl_LingerCloseAcceptedClient(&acceptedClientSeqAndOthers);
   }
   /// aclose  acceptedClientSeq cause
   /// alclose acceptedClientSeq cause
   virtual std::string OnDevice_Command(StrView cmd, StrView param) override {
      if (cmd == "aclose")
         this->OpQueue_.AddTask(DeviceAsyncOp(&OpThr_CloseAcceptedClient, param.ToString()));
      else if (cmd == "alclose")
         this->OpQueue_.AddTask(DeviceAsyncOp(&OpThr_LingerCloseAcceptedClient, param.ToString()));
      else
         return base::OnDevice_Command(cmd, param);
      return std::string{};
   }

   virtual void OnCommonTimer(TimeStamp) override {
      if (this->Listener_)
         this->OpQueue_.AddTask(DeviceAsyncOp{[](Device& dev) {
            if (static_cast<TcpServerBase*>(&dev)->Listener_)
               static_cast<Listener*>(static_cast<TcpServerBase*>(&dev)->Listener_.get())->OnTcpServer_OnCommonTimer();
         }});
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
