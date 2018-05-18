/// \file fon9/io/TcpServerBase.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_TcpServerBase_hpp__
#define __fon9_io_TcpServerBase_hpp__
#include "fon9/io/Server.hpp"
#include "fon9/io/SocketServer.hpp"
#include "fon9/io/DeviceStartSend.hpp"

namespace fon9 { namespace io {

using AcceptedClientId = uint64_t;

class fon9_API TcpListenerBase;
using ListenerSP = intrusive_ptr<TcpListenerBase>;

/// \ingroup io
/// 透過 TcpServer 連入的 Device 基底.
/// - 遠端連入後建立的對應 Device.
/// - 一旦斷線(LinkBroken, LinkError, Close...):
///   - 觸發 Dispose 最終會刪除對應的 Device.
///   - 告知 Listener 移除 this.
class fon9_API AcceptedClientDeviceBase : public Device {
   fon9_NON_COPY_NON_MOVE(AcceptedClientDeviceBase);
   using base = Device;

   friend TcpListenerBase;
   AcceptedClientId  AcceptedClientId_{0};

protected:
   /// AcceptedClientDeviceBase 不可能執行到這兒, 所以這裡 do nothing.
   virtual void OpImpl_Reopen() override;

   /// 僅能在尚未 LinkReady 時執行, 一旦 LinkReady 就不會做任何事情!
   /// 在 Server 接受連線後, 會將 Device 設定好, 所以這裡的參數為 Server 提供的 deviceId.
   virtual void OpImpl_Open(std::string deviceId) override;

   /// 判斷斷線 => this->Owner_->RemoveAcceptedClient(*this);
   virtual void OpImpl_StateChanged(const StateChangedArgs& e) override;

   // virtual void OpImpl_Close(std::string cause) override {
   //    shutdown(this->Socket_.GetSocketHandle(), ...);
   //    OpThr_DisposeNoClose(*this, std::move(cause));
   // }

public:
   const ListenerSP  Owner_;
   AcceptedClientDeviceBase(ListenerSP owner, SessionSP ses, ManagerSP mgr)
      : base(std::move(ses), std::move(mgr), Style::AcceptedClient)
      , Owner_{std::move(owner)} {
   }
   AcceptedClientId GetAcceptedClientId() const {
      return this->AcceptedClientId_;
   }
};

/// \ingroup io
/// - 協助 AcceptedClientBase 完成:
///   - Device member function: `virtual bool IsSendBufferEmpty() const override;`
/// - 協助完成 `DeviceImpl_DeviceStartSend<>` 所需要的 `struct SendAux`;
template <class AcceptedClientBase>
class AcceptedClientWithSend : public AcceptedClientBase {
   fon9_NON_COPY_NON_MOVE(AcceptedClientWithSend);
public:
   using AcceptedClientBase::AcceptedClientBase;

   virtual bool IsSendBufferEmpty() const override {
      bool res;
      this->OpQueue_.InplaceOrWait(AQueueTaskKind::Send, DeviceAsyncOp{[&res](Device& dev) {
         res = static_cast<AcceptedClientBase*>(&dev)->GetSendBuffer().IsEmpty();
      }});
      return res;
   }

   template <class SendAuxBase>
   struct SendAux : public SendAuxBase {
      using SendAuxBase::SendAuxBase;
      SendAux() = delete;

      static SendBuffer& GetSendBuffer(AcceptedClientBase& dev) {
         return dev.GetSendBuffer();
      }
      void AsyncSend(AcceptedClientBase&, StartSendChecker& sc, ObjHolderPtr<BufferList>&& pbuf) {
         sc.AsyncSend(std::move(pbuf));
      }
   };
};

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// TcpListener 的基底, 負責管理 AcceptedClient.
class fon9_API TcpListenerBase : public intrusive_ref_counter<TcpListenerBase> {
   fon9_NON_COPY_NON_MOVE(TcpListenerBase);
   friend AcceptedClientDeviceBase;

   bool  IsDisposing_{false};

   // 因為不確定: std::vector<AcceptedClientDeviceBaseSP> 是否能最佳化.
   // - 例如: erase(iter); 只用 memmove(); 就足夠.
   // - 所以, 自行呼叫:
   //   - intrusive_ptr_add_ref(): 在 AddAcceptedClient(); 之後呼叫.
   //   - intrusive_ptr_release(): 在 Dispose(); 及 RemoveAcceptedClient(); 呼叫
   using AcceptedClientsImpl = std::vector<AcceptedClientDeviceBase*>;
   static AcceptedClientsImpl::iterator FindAcceptedClients(AcceptedClientsImpl& devs, AcceptedClientId id);

   using AcceptedClients = MustLock<AcceptedClientsImpl>;
   mutable AcceptedClients  AcceptedClients_;

   void RemoveAcceptedClient(AcceptedClientDeviceBase& devAccepted);
   DeviceSP GetAcceptedClient(StrView* acceptedClientId);

   virtual void OnListener_Dispose() = 0;

protected:
   void AddAcceptedClient(DeviceServer& server, AcceptedClientDeviceBase& devAccepted, StrView connId);
   void SetAcceptedClientsReserved(size_t capacity) {
      if (capacity) {
         AcceptedClients::Locker devs{this->AcceptedClients_};
         devs->reserve(capacity);
      }
   }

public:
   TcpListenerBase() {
   }
   virtual ~TcpListenerBase();

   void Dispose(std::string cause);
   bool IsDisposing() const {
      return this->IsDisposing_;
   }

   size_t GetConnectionCount() const {
      AcceptedClients::Locker devs{this->AcceptedClients_};
      return devs->size();
   }

   void OpImpl_CloseAcceptedClient(StrView acceptedClientId);
   void OpImpl_LingerCloseAcceptedClient(StrView acceptedClientId);
};
fon9_WARN_POP;

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
      size_t curClientCount = (this->Listener_ ? this->Listener_->GetConnectionCount() : 0);
      RevPrint(rbuf, "|ClientCount=", curClientCount);
      BufferAppendTo(rbuf.MoveOut(), info);
   }

   static void OpThr_CloseAcceptedClient(Device& dev, std::string acceptedClientId) {
      if (auto* listener = static_cast<TcpServerBase*>(&dev)->Listener_.get())
         listener->OpImpl_CloseAcceptedClient(&acceptedClientId);
   }
   static void OpThr_LingerCloseAcceptedClient(Device& dev, std::string acceptedClientId) {
      if (auto* listener = static_cast<TcpServerBase*>(&dev)->Listener_.get())
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
