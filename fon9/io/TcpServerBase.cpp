/// \file fon9/io/TcpServerBase.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/TcpServerBase.hpp"

namespace fon9 { namespace io {

void AcceptedClientDeviceBase::OpImpl_Reopen() {
}
void AcceptedClientDeviceBase::OpImpl_Open(std::string deviceId) {
   if (this->OpImpl_GetState() < State::LinkReady) {
      OpThr_SetDeviceId(*this, std::move(deviceId));
      OpThr_SetLinkReady(*this, std::string{});
   }
}
void AcceptedClientDeviceBase::OpImpl_StateChanged(const StateChangedArgs& e) {
   base::OpImpl_StateChanged(e);
   if (e.Before_ <= State::LinkReady && State::LinkReady < e.After_)
      this->Owner_->RemoveAcceptedClient(*this);
}

//--------------------------------------------------------------------------//

TcpListenerBase::~TcpListenerBase() {
   // 解構時沒必要再呼叫 Dispose() 了, 因為此時已經 AcceptedClients_ 必定為空!
   // this->Dispose("Listener.Dtor");
}

void TcpListenerBase::Dispose(std::string cause) {
   if (this->IsDisposing_)
      return;
   this->IsDisposing_ = true;
   this->OnListener_Dispose();
   AcceptedClientsImpl devs;
   {
      AcceptedClients::Locker lkdevs{this->AcceptedClients_};
      devs.swap(*lkdevs);
   }
   for (auto* dev : devs) {
      dev->AsyncDispose(cause);
      intrusive_ptr_release(dev);
   }
}

TcpListenerBase::AcceptedClientsImpl::iterator
TcpListenerBase::FindAcceptedClients(AcceptedClientsImpl& devs, AcceptedClientId id) {
   return std::lower_bound(devs.begin(), devs.end(), id,
                           [](AcceptedClientDeviceBase* i, AcceptedClientId k) -> bool {
      return i->AcceptedClientId_ < k;
   });
}

void TcpListenerBase::AddAcceptedClient(DeviceServer& server, AcceptedClientDeviceBase& devAccepted, StrView connId) {
   {
      AcceptedClients::Locker devs{this->AcceptedClients_};
      devAccepted.AcceptedClientId_ = (devs->empty() ? 1 : (devs->back()->AcceptedClientId_ + 1));
      devs->push_back(&devAccepted);
   }
   intrusive_ptr_add_ref(&devAccepted);
   devAccepted.Initialize();
   if (server.Manager_)
      server.Manager_->OnDevice_Accepted(server, devAccepted);
   devAccepted.AsyncOpen(connId.ToString());
}
void TcpListenerBase::RemoveAcceptedClient(AcceptedClientDeviceBase& devAccepted) {
   if (this->IsDisposing_)
      return;
   AcceptedClients::Locker devs{this->AcceptedClients_};
   auto ifind = FindAcceptedClients(*devs, devAccepted.AcceptedClientId_);
   if (ifind != devs->end() && *ifind == &devAccepted) {
      devs->erase(ifind);
      intrusive_ptr_release(&devAccepted);
   }
}

DeviceSP TcpListenerBase::GetAcceptedClient(StrView* acceptedClientId) {
   const char*       endptr;
   AcceptedClientId  id = StrTo(*acceptedClientId, AcceptedClientId{}, &endptr);
   acceptedClientId->SetBegin(endptr);
   StrTrim(acceptedClientId);

   AcceptedClients::Locker devs{this->AcceptedClients_};
   auto ifind = FindAcceptedClients(*devs, id);
   return (ifind == devs->end() ? DeviceSP{} : DeviceSP{*ifind});
}
void TcpListenerBase::OpImpl_CloseAcceptedClient(StrView acceptedClientId) {
   if (DeviceSP dev = this->GetAcceptedClient(&acceptedClientId))
      dev->AsyncClose(acceptedClientId.ToString());
}
void TcpListenerBase::OpImpl_LingerCloseAcceptedClient(StrView acceptedClientId) {
   if (DeviceSP dev = this->GetAcceptedClient(&acceptedClientId))
      dev->AsyncLingerClose(acceptedClientId.ToString());
}
void TcpListenerBase::OnTcpServer_OnCommonTimer() {
}


} } // namespaces
