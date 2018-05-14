/// \file fon9/io/FdrTcpServer.cpp
/// \author fonwinz@gmail.com
#include "fon9/sys/Config.h"
#ifdef fon9_POSIX
#include "fon9/io/FdrTcpServer.hpp"

namespace fon9 { namespace io {
fon9_WARN_DISABLE_PADDING;

class FdrTcpListener::AcceptedClient : public AcceptedClientDeviceBase, public FdrSocket {
   fon9_NON_COPY_NON_MOVE(AcceptedClient);
   using base = AcceptedClientDeviceBase;

   virtual void OnFdrEvent_AddRef() override {
      intrusive_ptr_add_ref(static_cast<Device*>(this));
   }
   virtual void OnFdrEvent_ReleaseRef() override {
      intrusive_ptr_release(static_cast<Device*>(this));
   }

   struct ContinueSendAux : public FdrSocket::ContinueSendAux {
      static bool IsSendBufferAlive(Device&, SendBuffer&) {
         return true;
      }
   };
   virtual void OnFdrEvent_StartSend() override {
      ContinueSendAux aux;
      DeviceContinueSend(*this, this->SendBuffer_, aux);
   }
   virtual void OnFdrEvent_Handling(FdrEventFlag evs) override {
      if (IsEnumContains(evs, FdrEventFlag::Readable)) {
         if (!this->CheckRead(*this, nullptr))
            return;
      }
      if (IsEnumContains(evs, FdrEventFlag::Writable)) {
         ContinueSendAux aux;
         DeviceContinueSend(*this, this->SendBuffer_, aux);
      }
      this->CheckSocketErrorOrCanceled(evs);
   }

   virtual void OnFdrSocket_Error(std::string errmsg) override {
      this->AsyncDispose(errmsg);
   }

   virtual void OpImpl_StartRecv(RecvBufferSize preallocSize) override {
      this->StartRecv(preallocSize);
   }
   virtual void OpImpl_Close(std::string cause) override {
      ::shutdown(this->GetFD(), SHUT_WR);
      OpThr_DisposeNoClose(*this, std::move(cause));
   }

public:
   AcceptedClient(FdrTcpListener& owner, Socket soAccepted, SessionSP ses, ManagerSP mgr)
      : base(&owner, std::move(ses), std::move(mgr))
      , FdrSocket(*owner.Server_->IoServiceSP_, std::move(soAccepted)) {
   }

   using Impl = DeviceImpl_DeviceStartSend<AcceptedClientWithSend<AcceptedClient>, FdrSocket>;
};

//--------------------------------------------------------------------------//

FdrTcpListener::FdrTcpListener(FdrTcpServerSP&& server, Socket&& soListen)
   : FdrEventHandler{*server->IoServiceSP_, soListen.MoveOut()}
   , Server_{std::move(server)} {
}

ListenerSP FdrTcpListener::CreateListener(FdrTcpServerSP server, SocketResult& soRes) {
   const SocketServerConfig& cfg = server->Config_;
   Socket   soListen;
   if (!cfg.CreateListenSocket(soListen, soRes))
      return ListenerSP{};
   
   size_t capAcceptedClients = cfg.ServiceArgs_.Capacity_;
   if (capAcceptedClients > 0) // 如果有限制最大連線數量, 則容量必須稍大, 才能接收連線後再關閉.
      capAcceptedClients += 4; // TODO: 檢查 getrlimit(RLIMIT_NOFILE) 允許的最大 fd.

   FdrTcpListener*   fdrListener;
   ListenerSP retval{fdrListener = new FdrTcpListener{std::move(server), std::move(soListen)}};
   fdrListener->SetAcceptedClientsReserved(capAcceptedClients);
   fdrListener->UpdateFdrEvent();
   return retval;
}

//--------------------------------------------------------------------------//

void FdrTcpListener::OnFdrEvent_StartSend() {
}
void FdrTcpListener::OnFdrEvent_AddRef() {
   intrusive_ptr_add_ref(static_cast<baseCounter*>(this));
}
void FdrTcpListener::OnFdrEvent_ReleaseRef() {
   intrusive_ptr_release(static_cast<baseCounter*>(this));
}
FdrEventFlag FdrTcpListener::GetRequiredFdrEventFlag() const {
   return FdrEventFlag::Readable;
}
void FdrTcpListener::OnFdrEvent_Handling(FdrEventFlag evs) {
   if (this->IsDisposing())
      return;

   FdrTcpServer&  server = *this->Server_;
   if (IsEnumContains(evs, FdrEventFlag::Error)) {
      server.OpQueue_.AddTask(DeviceAsyncTask{[this](Device& dev) {
         if (static_cast<FdrTcpServer*>(&dev)->Listener_ == this)
            FdrTcpServer::OpThr_SetBrokenState(dev, RevPrintTo<std::string>("FdrTcpListener.OnFdrEvent|err=", Socket::LoadSocketErrC(this->GetFD())));
      }});
      return;
   }

   SocketAddress  addrRemote;
   socklen_t      addrLen = sizeof(addrRemote);
   ZeroStruct(addrRemote);
   Socket   soAccepted(::accept(this->GetFD(), &addrRemote.Addr_, &addrLen));
   if (fon9_UNLIKELY(!soAccepted.IsSocketReady())) {
      fon9_LOG_FATAL("FdrTcpListener.accepted|err=", GetSocketErrC());
      return;
   }
   if (fon9_UNLIKELY(!soAccepted.SetNonBlock())) {
      fon9_LOG_FATAL("FdrTcpListener.SetNonBlock|soAccepted=", soAccepted.GetSocketHandle(), "|err=", GetSocketErrC());
      return;
   }
   SocketAddress  addrLocal;
   char           bufConnUID[kMaxTcpConnectionUID];
   addrLen = sizeof(addrLocal);
   StrView  strConnUID = MakeTcpConnectionUID(bufConnUID, &addrRemote,
      (getsockname(soAccepted.GetSocketHandle(), &addrLocal.Addr_, &addrLen) == 0 ? &addrLocal : nullptr));

   AcceptedClient*            devAccepted{nullptr};
   SocketResult               soRes;
   const SocketServerConfig&  cfg = server.Config_;
   if (cfg.ServiceArgs_.Capacity_ > 0 && this->GetConnectionCount() >= cfg.ServiceArgs_.Capacity_)
      // 如果超過了 MaxConnections, 要等一個 AcceptedClient 斷線後, 才允許接受新的連線.
      soRes = SocketResult{"OverMaxConnections", std::errc::too_many_files_open};
   else if (soAccepted.SetSocketOptions(cfg.AcceptedClientOptions_, soRes)) {
      if (SessionSP sesAccepted = server.OnDevice_Accepted()) {
         DeviceSP dev{devAccepted = new AcceptedClient::Impl(*this, std::move(soAccepted), std::move(sesAccepted), server.Manager_)};
         this->AddAcceptedClient(server, *devAccepted, strConnUID);
      }
      else
         soRes = SocketResult{"CreateSession", std::errc::operation_not_supported};
   }
   if (soRes.IsError() || devAccepted == nullptr)
      fon9_LOG_ERROR("TcpServer.Accepted"
                     "|dev=", ToHex{devAccepted},
                     "|err=", soRes, strConnUID);
   else
      fon9_LOG_INFO("TcpServer.Accepted"
                    "|dev=", ToHex{devAccepted},
                    "|aId=", devAccepted->GetAcceptedClientId(),
                    strConnUID);
}

void FdrTcpListener::OnListener_Dispose() {
   this->RemoveFdrEvent();
}
fon9_WARN_POP;

} } // namespaces
#endif
