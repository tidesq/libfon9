/// \file fon9/io/TcpClientBase.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_TcpClientBase_hpp__
#define __fon9_io_TcpClientBase_hpp__
#include "fon9/io/Socket.hpp"
#include "fon9/io/SocketClient.hpp"
#include "fon9/io/SocketAddressDN.hpp"
#include "fon9/io/DeviceStartSend.hpp"

namespace fon9 { namespace io {

class SendBuffer;
class RecvBuffer;

/// \ingroup io
/// TcpClient 基底, 衍生者: IocpTcpClient, FdrTcpClient;
/// - 查找設定裡面的 DN: Config_.DomainNames_
/// - 連線時依序嘗試 AddrList_ 裡面的位置 & port, 全都失敗才觸發 LinkError.
class fon9_API TcpClientBase : public Device {
   fon9_NON_COPY_NON_MOVE(TcpClientBase);
   TcpClientBase() = delete;
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
   void OpImpl_Connected(Socket::socket_t so);

   /// 實際執行 connect 的地方 ::connect() or ConnectEx() or ...
   virtual bool OpImpl_TcpConnect(Socket&& soCli, SocketResult& soRes) = 0;
   /// 清除「用來處理連線」的資源.
   virtual void OpImpl_TcpClearLinking();
   /// 清除緩衝區、Socket資源...
   virtual void OpImpl_TcpLinkBroken() = 0;

   virtual void OpImpl_Open(std::string cfgstr) override;
   virtual void OpImpl_Reopen() override;
   virtual void OpImpl_Close(std::string cause) override;
   virtual void OpImpl_StateChanged(const StateChangedArgs& e) override;

public:
   TcpClientBase(SessionSP ses, ManagerSP mgr)
      : base(std::move(ses), std::move(mgr), Style::Client) {
   }
   ~TcpClientBase();
};

/// \ingroup io
/// \tparam ClientImplT
/// \code
///   // OpImpl_TcpConnect() 時, 會重新建構新的 ClientImplT.
///   ClientImplT(OwnerDevice* owner, Socket&& so, SocketResult& soRes);
///
///   // OpImpl_TcpConnect() 時, 建構新的 ClientImplT 成功後, 會立即進行連線程序.
///   bool OpImpl_ConnectTo(const SocketAddress& addr, SocketResult& soRes);
///
///   // 當重新連線、斷線、連線失敗... 則會關閉先前的連線.
///   void OpImpl_Close();
///
///   // 執行 OpImpl_Close() 時, 設定此旗標.
///   bool IsClosing() const;
///
///   // 啟用「資料到達」事件.
///   void StartRecv(RecvBufferSize expectSize);
///
///   GetSendBuffer& GetSendBuffer();
/// \endcode
template <class IoServiceSP, class ClientImplT>
class TcpClientT : public TcpClientBase {
   fon9_NON_COPY_NON_MOVE(TcpClientT);
   using base = TcpClientBase;

protected:
   using ClientImpl = ClientImplT;
   friend ClientImpl;
   using ClientImplSP = intrusive_ptr<ClientImpl>;
   ClientImplSP   ImplSP_;

   void OpImpl_ResetImplSP() {
      if (ClientImplSP impl = std::move(this->ImplSP_))
         impl->OpImpl_Close();
   }
   virtual void OpImpl_TcpLinkBroken() override {
      this->OpImpl_ResetImplSP();
   }
   virtual void OpImpl_TcpClearLinking() override {
      base::OpImpl_TcpClearLinking();
      this->OpImpl_ResetImplSP();
   }
   bool OpImpl_TcpConnect(Socket&& soCli, SocketResult& soRes) {
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

   TcpClientT(IoServiceSP iosv, SessionSP ses, ManagerSP mgr)
      : base(std::move(ses), std::move(mgr))
      , IoService_{std::move(iosv)} {
   }

   static bool OpImpl_IsRecvBufferAlive(Device& dev, RecvBuffer& rbuf) {
      return &(static_cast<TcpClientT*>(&dev)->ImplSP_->GetRecvBuffer()) == &rbuf;
   }
   static bool OpImpl_IsSendBufferAlive(Device& dev, SendBuffer& sbuf) {
      return &(static_cast<TcpClientT*>(&dev)->ImplSP_->GetSendBuffer()) == &sbuf;
   }

   virtual bool IsSendBufferEmpty() const override {
      bool res;
      this->OpQueue_.InplaceOrWait(AQueueTaskKind::Send, DeviceAsyncOp{[&res](Device& dev) {
         if (ClientImpl* impl = static_cast<TcpClientT*>(&dev)->ImplSP_.get())
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
                && static_cast<TcpClientT*>(&dev)->ImplSP_.get() == impl) {
               static_cast<TcpClientT*>(&dev)->OpImpl_Connected(so);
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
         if (static_cast<TcpClientT*>(&dev)->ImplSP_ == impl)
            static_cast<TcpClientT*>(&dev)->OpImpl_ConnectToNext(&errmsg);
      }});
   }

   fon9_MSC_WARN_DISABLE(4623);//4623: '...': default constructor was implicitly defined as deleted
   struct ContinueSendAux : public ClientImpl::ContinueSendAux {
      using ClientImpl::ContinueSendAux::ContinueSendAux;
      static bool IsSendBufferAlive(Device& dev, SendBuffer& sbuf) {
         return TcpClientT::OpImpl_IsSendBufferAlive(*static_cast<TcpClientT*>(&dev), sbuf);
      }
   };

   template <class SendAuxBase>
   struct SendAux : public SendAuxBase {
      using SendAuxBase::SendAuxBase;

      static SendBuffer& GetSendBuffer(TcpClientT& dev) {
         return dev.ImplSP_->GetSendBuffer();
      }
      void AsyncSend(TcpClientT& dev, StartSendChecker& sc, ObjHolderPtr<BufferList>&& pbuf) {
         sc.AsyncSend(std::move(pbuf), dev.ImplSP_->GetSendBuffer(), &OpImpl_IsSendBufferAlive);
      }
   };
   fon9_MSC_WARN_POP;
};

} } // namespaces
#endif//__fon9_io_TcpClientBase_hpp__
