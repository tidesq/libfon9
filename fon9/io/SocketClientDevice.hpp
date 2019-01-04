/// \file fon9/io/SocketClientDevice.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_SocketClientDevice_hpp__
#define __fon9_io_SocketClientDevice_hpp__
#include "fon9/io/SocketClientConfig.hpp"
#include "fon9/io/Socket.hpp"
#include "fon9/io/SocketAddressDN.hpp"
#include "fon9/io/DeviceStartSend.hpp"

namespace fon9 { namespace io {

/// \ingroup io
/// - 查找設定裡面的 DN: Config_.DomainNames_
/// - 連線時依序嘗試 AddrList_ 裡面的位置 & port, 全都失敗才觸發 LinkError.
/// - 取得 ip:port 之後, 呼叫 OpImpl_SocketConnect() 進行連線.
/// - 通常衍生者會擁有一個 ImplSP_ 去處理與 Socket 的溝通(建立、關閉、收送...).
class fon9_API SocketClientDevice : public Device {
   fon9_NON_COPY_NON_MOVE(SocketClientDevice);
   SocketClientDevice() = delete;
   using base = Device;

protected:
   template <class DeviceT>
   friend bool OpThr_DeviceParseConfig(DeviceT& dev, StrView cfgstr);
   SocketClientConfig   Config_;

   size_t               NextAddrIndex_;
   SocketAddressList    AddrList_;
   DnQueryReqId         DnReqId_{};
   SocketAddress        RemoteAddress_;

   virtual void OnCommonTimer(TimeStamp now) override;
   void StartConnectTimer() {
      this->CommonTimerRunAfter(TimeInterval_Second(this->Config_.TimeoutSecs_));
   }

   void OpImpl_ConnectToNext(StrView lastError);
   void OpImpl_ReopenImpl();
   void OpImpl_OnDnQueryDone(DnQueryReqId id, const DomainNameParseResult& res);
   virtual void OpImpl_OnAddrListEmpty();

   /// OpThr_SetDeviceId(*this, deviceId);
   /// OpThr_SetLinkReady(*this, std::string{});
   void OpImpl_SetConnected(std::string deviceId);

   /// 建立 Socket, 例如:
   /// - 建立 TcpClient: `return so.CreateDeviceSocket(addr.GetAF(), SocketType::Stream, soRes);`
   /// - 建立 UDP:       `return so.CreateDeviceSocket(addr.GetAF(), SocketType::Dgram, soRes);`
   virtual bool CreateSocket(Socket& so, const SocketAddress& addr, SocketResult& soRes) = 0;
   /// 實際執行 connect 的地方 ::connect() or ConnectEx() or ...
   virtual bool OpImpl_SocketConnect(Socket&& soCli, SocketResult& soRes) = 0;
   /// 清除「用來處理連線」的資源.
   virtual void OpImpl_SocketClearLinking();
   /// 清除緩衝區、Socket資源...
   virtual void OpImpl_SocketLinkBroken() = 0;

   /// 解析 cfgstr 填入 this->Config_;
   /// 若在解析過程有遇到不認識的 tag, 則會透過 this->OpImpl_SetProperty() 處理.
   /// 所以衍生者可以覆寫 `ConfigParser::Result OpImpl_SetProperty(StrView tag, StrView& value);` 來處理自訂選項.
   virtual void OpImpl_Open(std::string cfgstr) override;
   virtual void OpImpl_Reopen() override;
   virtual void OpImpl_Close(std::string cause) override;
   virtual void OpImpl_StateChanged(const StateChangedArgs& e) override;

public:
   SocketClientDevice(SessionSP ses, ManagerSP mgr, Style style = Style::Client)
      : base(std::move(ses), std::move(mgr), style) {
   }
   ~SocketClientDevice();
};

class SendBuffer;
class RecvBuffer;

/// \ingroup io
/// \tparam ClientImplT
///   \code
///   class ClientImplT {
///      // OpImpl_SocketConnect() 時, 會重新建構新的 ClientImplT.
///      ClientImplT(OwnerDevice* owner, Socket&& so, SocketResult& soRes);
///
///      // OpImpl_SocketConnect() 時, 建構新的 ClientImplT 成功後, 會立即進行連線程序.
///      bool OpImpl_ConnectTo(const SocketAddress& addr, SocketResult& soRes);
///
///      // 當重新連線、斷線、連線失敗... 則會關閉先前的連線.
///      void OpImpl_Close();
///
///      // 執行 OpImpl_Close() 時, 設定此旗標.
///      bool IsClosing() const;
///
///      // 啟用「資料到達」事件.
///      void StartRecv(RecvBufferSize expectSize);
///
///      GetSendBuffer& GetSendBuffer();
///   };
///   \endcode
/// \tparam DeviceBase 可參考 TcpClientBase.hpp 的 class TcpClientBase;
///   \code
///   class TcpClientBase {
///      void OpImpl_Connected(Socket::socket_t so);
///   };
///   \endcode
template <class IoServiceSP, class ClientImplT, class DeviceBase>
class SocketClientDeviceT : public DeviceBase {
   fon9_NON_COPY_NON_MOVE(SocketClientDeviceT);
   using base = DeviceBase;

protected:
   using ClientImpl = ClientImplT;
   friend ClientImpl;
   using ClientImplSP = intrusive_ptr<ClientImpl>;
   ClientImplSP   ImplSP_;

   void OpImpl_ResetImplSP() {
      if (ClientImplSP impl = std::move(this->ImplSP_))
         impl->OpImpl_Close();
   }
   virtual void OpImpl_SocketLinkBroken() override {
      this->OpImpl_ResetImplSP();
   }
   virtual void OpImpl_SocketClearLinking() override {
      base::OpImpl_SocketClearLinking();
      this->OpImpl_ResetImplSP();
   }
   bool OpImpl_SocketConnect(Socket&& soCli, SocketResult& soRes) {
      this->OpImpl_ResetImplSP();
      this->ImplSP_.reset(new ClientImpl{this, std::move(soCli), soRes});
      if (soRes.IsError())
         return false;
      return this->ImplSP_->OpImpl_ConnectTo(this->RemoteAddress_, soRes);
   }
   void OpImpl_StartRecv(RecvBufferSize preallocSize) {
      if (ClientImpl* impl = this->ImplSP_.get())
         impl->StartRecv(preallocSize);
   }

public:
   const IoServiceSP  IoService_;

   SocketClientDeviceT(IoServiceSP iosv, SessionSP ses, ManagerSP mgr)
      : base(std::move(ses), std::move(mgr))
      , IoService_{std::move(iosv)} {
   }

   static bool OpImpl_IsRecvBufferAlive(Device& dev, RecvBuffer& rbuf) {
      if (auto impl = static_cast<SocketClientDeviceT*>(&dev)->ImplSP_.get())
         return &(impl->GetRecvBuffer()) == &rbuf;
      return false;
   }
   static SendBuffer* OpImpl_GetSendBuffer(Device& dev, void* impl) {
      if (static_cast<ClientImpl*>(impl) == static_cast<SocketClientDeviceT*>(&dev)->ImplSP_.get())
         return &(static_cast<ClientImpl*>(impl)->GetSendBuffer());
      return nullptr;
   }

   virtual bool IsSendBufferEmpty() const override {
      bool res;
      this->OpQueue_.InplaceOrWait(AQueueTaskKind::Send, DeviceAsyncOp{[&res](Device& dev) {
         if (ClientImpl* impl = static_cast<SocketClientDeviceT*>(&dev)->ImplSP_.get())
            res = impl->GetSendBuffer().IsEmpty();
         else
            res = true;
      }});
      return res;
   }

   void OnSocketConnected(ClientImpl* impl, Socket::socket_t so) {
      if (this->OpImpl_GetState() < State::LinkReady)
         this->OpQueue_.AddTask(DeviceAsyncOp{[impl, so](Device& dev) {
         if (dev.OpImpl_GetState() == State::Linking
             && static_cast<SocketClientDeviceT*>(&dev)->ImplSP_.get() == impl) {
            static_cast<SocketClientDeviceT*>(&dev)->OpImpl_Connected(so);
         }
      }});
   }

   void OnSocketError(ClientImpl* impl, std::string errmsg) {
      DeviceOpQueue::ALockerForAsyncTask alocker{this->OpQueue_, AQueueTaskKind::Get};
      if (alocker.IsAllowInplace_) { // 檢查 Owner_ 是否仍在使用 this; 如果沒有在用 this, 則不用通知 Owner_;
         if (this->ImplSP_.get() != impl)
            return;
      }
      alocker.AddAsyncTask(DeviceAsyncOp{[impl, errmsg](Device& dev) {
         if (static_cast<SocketClientDeviceT*>(&dev)->ImplSP_ == impl)
            static_cast<SocketClientDeviceT*>(&dev)->OpImpl_ConnectToNext(&errmsg);
      }});
   }

   fon9_MSC_WARN_DISABLE(4623);//4623: '...': default constructor was implicitly defined as deleted
   struct ContinueSendAux : public ClientImpl::ContinueSendAux {
      using ClientImpl::ContinueSendAux::ContinueSendAux;
      static bool IsSendBufferAlive(Device& dev, SendBuffer& sbuf) {
         if (auto impl = static_cast<SocketClientDeviceT*>(&dev)->ImplSP_.get())
            return &(impl->GetSendBuffer()) == &sbuf;
         return false;
      }
   };

   template <class SendAuxBase>
   struct SendAuxImpl : public SendAuxBase {
      using SendAuxBase::SendAuxBase;

      static SendBuffer& GetSendBufferAtLocked(SocketClientDeviceT& dev) {
         return dev.ImplSP_->GetSendBuffer();
      }
      void AsyncSend(SocketClientDeviceT& dev, StartSendChecker& sc, ObjHolderPtr<BufferList>&& pbuf) {
         sc.AsyncSend(std::move(pbuf), dev.ImplSP_.get(), &OpImpl_GetSendBuffer);
      }
   };
   fon9_MSC_WARN_POP;
};

} } // namespaces
#endif//__fon9_io_SocketClientDevice_hpp__
