/// \file fon9/io/win/IocpTcpServer.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/win/IocpTcpServer.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace io {
fon9_WARN_DISABLE_PADDING;

class IocpTcpListener::AcceptedClient : public AcceptedClientDeviceBase, public IocpSocket {
   fon9_NON_COPY_NON_MOVE(AcceptedClient);
   using base = AcceptedClientDeviceBase;
   const IocpTcpListener::ListenerSP   Owner_;

   virtual unsigned IocpSocketAddRef() override {
      return intrusive_ptr_add_ref(this);
   }
   virtual unsigned IocpSocketReleaseRef() override {
      return intrusive_ptr_release(this);
   }
   virtual void OnIocpSocket_Error(OVERLAPPED* lpOverlapped, DWORD eno) override {
      this->AsyncDispose(this->GetErrorMessage(lpOverlapped, eno));
   }

   virtual void OnIocpSocket_Received(DcQueueList& rxbuf) override {
      this->OnRecvBufferReady(*this, rxbuf, nullptr);
   }
   virtual void OnIocpSocket_Writable(DWORD bytesTransfered) override {
      struct ContinueSendAux : public IocpContinueSendAux {
         using IocpContinueSendAux::IocpContinueSendAux;
         ContinueSendAux() = delete;
         static bool IsBufferAlive(Device&, IoBuffer*) {
            return true;
         }
      };
      ContinueSendAux aux{bytesTransfered};
      this->ContinueSend(*this, aux);
   }

   virtual void OpImpl_StartRecv(RecvBufferSize preallocSize) override {
      this->StartRecv(preallocSize);
   }
   virtual void OpImpl_StateChanged(const StateChangedArgs& e) override {
      base::OpImpl_StateChanged(e);
      if (e.Before_ <= State::LinkReady && State::LinkReady < e.After_)
         this->Owner_->RemoveAcceptedClient(*this);
   }
   virtual void OpImpl_Close(std::string cause) override {
      shutdown(this->Socket_.GetSocketHandle(), SD_SEND);
      OpThr_DisposeNoClose(*this, std::move(cause));
   }

public:
   AcceptedClientId  AcceptedClientId_{0};

   AcceptedClient(IocpTcpListener& owner, Socket soAccepted, SessionSP ses, ManagerSP mgr, SocketResult& soRes)
      : base(std::move(ses), std::move(mgr))
      , IocpSocket(owner.IocpService_, std::move(soAccepted), soRes)
      , Owner_{&owner} {
   }

   virtual bool IsSendBufferEmpty() const override {
      bool res;
      this->OpQueue_.WaitInvoke(AQueueTaskKind::Send, DeviceAsyncOp{[&res](Device& dev) {
         res = static_cast<AcceptedClient*>(&dev)->SendBuffer_.IsEmpty();
      }});
      return res;
   }

   template <class SendAuxBase>
   struct SendAux : public SendAuxBase {
      using SendAuxBase::SendAuxBase;
      SendAux() = delete;

      static IocpSocket* GetImpl(AcceptedClient& dev) {
         return &dev;
      }
      void AsyncSend(AcceptedClient&, SendChecker& sc, ObjHolderPtr<BufferList>&& pbuf) {
         sc.AsyncSend(std::move(pbuf));
      }
   };

   using Impl = DeviceImpl_IoBufferSend<AcceptedClient, IocpSocket>;
};

//--------------------------------------------------------------------------//

IocpTcpListener::IocpTcpListener(IocpServiceSP&& iosv, IocpTcpServerSP&& server, Socket&& soListen)
   : IocpHandler{std::move(iosv)}
   , Server_{std::move(server)}
   , ListenSocket_{std::move(soListen)} {
}

IocpTcpListener::ListenerSP IocpTcpListener::CreateListener(IocpTcpServerSP server, SocketResult& soRes) {
   const SocketServerConfig& cfg = server->Config_;
   Socket   soListen;
   if (!cfg.CreateListenSocket(soListen, soRes))
      return ListenerSP{};
   
   IocpServiceSP  iosv = server->IoServiceSP_;
   size_t         capAcceptedClients{0};
   if (!iosv) {
      char        addrbuf[SocketAddress::kMaxAddrPortStrSize];
      std::string thrName{"Listen:"};
      thrName.append(addrbuf, cfg.ListenConfig_.AddrBind_.ToAddrPortStr(addrbuf));

      IoServiceArgs  iosvArgs = cfg.ServiceArgs_;
      if (iosvArgs.Capacity_) // 如果有限制最大連線數量(雖然目前 IocpService 沒用到), 則容量必須稍大, 才能接收連線後再關閉.
         capAcceptedClients = (iosvArgs.Capacity_ += 4);
      iosv = IocpService::MakeService(iosvArgs, thrName, soRes);
      if (!iosv)
         return ListenerSP{};
   }
   ListenerSP retval{new IocpTcpListener{std::move(iosv), std::move(server), std::move(soListen)}};
   auto       resAttach = retval->IocpAttach(retval->ListenSocket_.GetSocketHandle());
   if (resAttach.IsError()) {
      soRes = SocketResult{"IocpAttach", resAttach.GetError()};
      return ListenerSP{};
   }
   if (capAcceptedClients) {
      AcceptedClients::Locker devs{retval->AcceptedClients_};
      devs->reserve(capAcceptedClients);
   }
   if (retval->SetupAccepter(soRes))
      return retval;
   return ListenerSP{};
}

//--------------------------------------------------------------------------//

bool IocpTcpListener::SetupAccepter(SocketResult& soRes) {
   if (this->ClientSocket_.IsSocketReady())
      return true;
   if (!this->ClientSocket_.CreateOverlappedSocket(this->Server_->Config_.ListenConfig_.GetAF(), SocketType::Stream, soRes))
      return false;
   intrusive_ptr_add_ref(this); // release at: OnIocp_Done(); OnIocp_Error();
   ZeroStruct(this->ListenOverlapped_);
   DWORD rxBytes;
   if (AcceptEx(this->ListenSocket_.GetSocketHandle(),
                this->ClientSocket_.GetSocketHandle(),
                this->AcceptBuffer_,
                0, //不必先收到 Client 送來的資料.
                sizeof(AcceptBuffer_) / 2,
                sizeof(AcceptBuffer_) / 2,
                &rxBytes,
                &this->ListenOverlapped_)) {
      // 經測試, 不會到這兒! (因為使用 Overlapped I/O)
      // 即使在呼叫 AcceptEx() 之前, 已有 client 連上.
      // 仍會是 ERROR_IO_PENDING.
      // 所以一律在 OnIocp_Done() 處理 AcceptClient.
   }
   else {
      int eno = WSAGetLastError();
      if (eno != ERROR_IO_PENDING) {
         intrusive_ptr_release(this);
         soRes = SocketResult("AcceptEx", GetSocketErrC(eno));
         return false;
      }
   }
   return true;
}

void IocpTcpListener::OnIocp_Error(OVERLAPPED* lpOverlapped, DWORD eno) {
   (void)lpOverlapped;
   if (!this->IsDisposing_) {
      this->Server_->OpQueue_.AddTask(DeviceAsyncTask{[=](Device& dev) {
         IocpTcpServer& server = *static_cast<IocpTcpServer*>(&dev);
         if (server.Listener_ == this)
            Device::OpThr_SetBrokenState(server, RevPrintTo<std::string>("IocpTcpListener.OnIocp_Error:|err=", GetSysErrC(eno)));
      }});
   }
   intrusive_ptr_release(this);
}

void IocpTcpListener::OnIocp_Done(OVERLAPPED* lpOverlapped, DWORD bytesTransfered) {
   (void)bytesTransfered; (void)lpOverlapped;//assert(lpOverlapped == &this->ListenOverlapped_);
   if (this->IsDisposing_) {
      intrusive_ptr_release(this);
      return;
   }

   AcceptedClient* devAccepted{nullptr};
   char           bufConnUID[kMaxTcpConnectionUID];
   StrView        strConnUID;
   DWORD          rxBytes, flags;
   SocketResult   soRes;
   SOCKET         soAccepted = this->ClientSocket_.GetSocketHandle();
   if (!WSAGetOverlappedResult(this->ListenSocket_.GetSocketHandle(),
                               &this->ListenOverlapped_,
                               &rxBytes,
                               FALSE, //no wait.
                               &flags))
      soRes = SocketResult{"ListenResult"};
   else {
      int addrLocalLen, addrRemoteLen;
      LPSOCKADDR addrLocal, addrRemote;
      GetAcceptExSockaddrs(this->AcceptBuffer_,
                           rxBytes,
                           sizeof(AcceptBuffer_) / 2,
                           sizeof(AcceptBuffer_) / 2,
                           &addrLocal,
                           &addrLocalLen,
                           &addrRemote,
                           &addrRemoteLen);
      strConnUID = MakeTcpConnectionUID(bufConnUID, reinterpret_cast<SocketAddress*>(addrRemote), reinterpret_cast<SocketAddress*>(addrLocal));

      IocpTcpServer&             server = *this->Server_;
      const SocketServerConfig&  cfg = server.Config_;
      if (cfg.ServiceArgs_.Capacity_ > 0 && this->GetCurrentConnectionCount() >= cfg.ServiceArgs_.Capacity_)
         // 如果超過了 MaxConnections, 要等一個 AcceptedClient 斷線後, 才允許接受新的連線.
         soRes = SocketResult{"OverMaxConnections", std::errc::too_many_files_open};
      else if (this->ClientSocket_.SetSocketOptions(cfg.AcceptedClientOptions_, soRes)) {
         SOCKET soListen = this->ListenSocket_.GetSocketHandle();
         if (setsockopt(soAccepted, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<const char *>(&soListen), sizeof(soListen)) != 0)
            soRes = SocketResult{"SO_UPDATE_ACCEPT_CONTEXT"};
         else if (SessionSP sesAccepted = server.OnDevice_Accepted()) {
            DeviceSP dev{devAccepted = new AcceptedClient::Impl(*this, std::move(this->ClientSocket_), std::move(sesAccepted), server.Manager_, soRes)};
            if (soRes.IsSuccess()) {
               {
                  AcceptedClients::Locker devs{this->AcceptedClients_};
                  devAccepted->AcceptedClientId_ = (devs->empty() ? 1 : (devs->back()->AcceptedClientId_ + 1));
                  devs->push_back(devAccepted);
               }
               intrusive_ptr_add_ref(devAccepted);
               devAccepted->Initialize();
               if (server.Manager_)
                  server.Manager_->OnDevice_Accepted(server, *dev);
               devAccepted->AsyncOpen(strConnUID.ToString());
            }
         }
         else
            soRes = SocketResult{"CreateSession", std::errc::operation_not_supported};
      }
   }
   if (soRes.IsError() || devAccepted == nullptr)
      fon9_LOG_ERROR("TcpServer.Accepted"
                     "|dev=", ToHex{devAccepted},
                     "|soAccepted=", soAccepted,
                     "|err=", soRes, strConnUID);
   else
      fon9_LOG_INFO("TcpServer.Accepted"
                    "|dev=", ToHex{devAccepted},
                    "|aId=", devAccepted->AcceptedClientId_,
                    "|soAccepted=", soAccepted, strConnUID);

   this->ClientSocket_.Close();
   this->ResetupAccepter();
   intrusive_ptr_release(this);
}

void IocpTcpListener::ResetupAccepter() {
   SocketResult soRes;
   if (!this->SetupAccepter(soRes)) {
      fon9_LOG_ERROR("IocpTcpListener.ResetupAccepter|err=", soRes);
      if (0);// todo: retry.
   }
}

void IocpTcpListener::Dispose(std::string cause) {
   if (this->IsDisposing_)
      return;
   this->IsDisposing_ = true;
   this->ListenSocket_.Close();
   AcceptedClientsImpl devs;
   {
      AcceptedClients::Locker lkdevs{this->AcceptedClients_};
      devs.swap(*lkdevs);
   }
   for (AcceptedClient* dev : devs) {
      dev->AsyncDispose(cause);
      intrusive_ptr_release(dev);
   }
}

//--------------------------------------------------------------------------//

IocpTcpListener::AcceptedClientsImpl::iterator
IocpTcpListener::FindAcceptedClients(AcceptedClientsImpl& devs, AcceptedClientId id) {
   return std::lower_bound(devs.begin(), devs.end(), id,
                           [](AcceptedClient* i, AcceptedClientId k) -> bool {
      return i->AcceptedClientId_ < k;
   });
}

void IocpTcpListener::RemoveAcceptedClient(AcceptedClient& dev) {
   if (this->IsDisposing_)
      return;
   AcceptedClients::Locker devs{this->AcceptedClients_};
   auto ifind = FindAcceptedClients(*devs, dev.AcceptedClientId_);
   if (ifind != devs->end() && *ifind == &dev) {
      devs->erase(ifind);
      intrusive_ptr_release(&dev);
   }
}

DeviceSP IocpTcpListener::GetAcceptedClient(StrView* acceptedClientId) {
   const char*       endptr;
   AcceptedClientId  id = StrTo(*acceptedClientId, AcceptedClientId{}, &endptr);
   acceptedClientId->SetBegin(endptr);
   StrTrim(acceptedClientId);

   AcceptedClients::Locker devs{this->AcceptedClients_};
   auto ifind = FindAcceptedClients(*devs, id);
   return (ifind == devs->end() ? DeviceSP{} : DeviceSP{*ifind});
}
void IocpTcpListener::OpImpl_CloseAcceptedClient(StrView acceptedClientId) {
   if (DeviceSP dev = this->GetAcceptedClient(&acceptedClientId))
      dev->AsyncClose(acceptedClientId.ToString());
}
void IocpTcpListener::OpImpl_LingerCloseAcceptedClient(StrView acceptedClientId) {
   if (DeviceSP dev = this->GetAcceptedClient(&acceptedClientId))
      dev->AsyncLingerClose(acceptedClientId.ToString());
}

fon9_WARN_POP;
} } // namespaces
