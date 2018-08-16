/// \file fon9/io/Server.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/Server.hpp"

namespace fon9 { namespace io {

bool DeviceServer::IsSendBufferEmpty() const {
   return true;
}
Device::SendResult DeviceServer::SendASAP(const void* src, size_t size) {
   (void)src; (void)size;
   return SendResult{std::errc::function_not_supported};
}
Device::SendResult DeviceServer::SendASAP(BufferList&& src) {
   BufferListConsumeErr(std::move(src), std::errc::function_not_supported);
   return SendResult{std::errc::function_not_supported};
}
Device::SendResult DeviceServer::SendBuffered(const void* src, size_t size) {
   (void)src; (void)size;
   return SendResult{std::errc::function_not_supported};
}
Device::SendResult DeviceServer::SendBuffered(BufferList&& src) {
   BufferListConsumeErr(std::move(src), std::errc::function_not_supported);
   return SendResult{std::errc::function_not_supported};
}

//--------------------------------------------------------------------------//

void DeviceAcceptedClient::OpImpl_Reopen() {
}
void DeviceAcceptedClient::OpImpl_Open(std::string deviceId) {
   if (this->OpImpl_GetState() < State::LinkReady) {
      OpThr_SetDeviceId(*this, std::move(deviceId));
      OpThr_SetLinkReady(*this, std::string{});
   }
}
void DeviceAcceptedClient::OpImpl_StateChanged(const StateChangedArgs& e) {
   base::OpImpl_StateChanged(e);
   if (e.Before_ <= State::LinkReady && State::LinkReady < e.After_)
      this->Owner_->RemoveAcceptedClient(*this);
}

//--------------------------------------------------------------------------//

DeviceListener::~DeviceListener() {
   // 解構時沒必要再呼叫 Dispose() 了, 因為此時已經 AcceptedClients_ 必定為空!
   // this->Dispose("Listener.Dtor");
}

void DeviceListener::Dispose(std::string cause) {
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

DeviceListener::AcceptedClientsImpl::iterator
DeviceListener::FindAcceptedClients(AcceptedClientsImpl& devs, DeviceAcceptedClientSeq seq) {
   auto iend = devs.end();
   auto ifind = std::lower_bound(devs.begin(), iend, seq,
                           [](DeviceAcceptedClient* i, DeviceAcceptedClientSeq k) -> bool {
      return i->AcceptedClientSeq_ < k;
   });
   if (ifind != iend && (*ifind)->AcceptedClientSeq_ == seq)
      return ifind;
   return iend;
}

void DeviceListener::AddAcceptedClient(DeviceServer& server, DeviceAcceptedClient& devAccepted, StrView connId) {
   {
      AcceptedClients::Locker devs{this->AcceptedClients_};
      devAccepted.AcceptedClientSeq_ = (devs->empty() ? 1 : (devs->back()->AcceptedClientSeq_ + 1));
      devs->push_back(&devAccepted);
   }
   intrusive_ptr_add_ref(&devAccepted);
   devAccepted.Initialize();
   if (server.Manager_)
      server.Manager_->OnDevice_Accepted(server, devAccepted);
   devAccepted.AsyncOpen(connId.ToString());
}
void DeviceListener::RemoveAcceptedClient(DeviceAcceptedClient& devAccepted) {
   if (this->IsDisposing_)
      return;
   AcceptedClients::Locker devs{this->AcceptedClients_};
   auto ifind = FindAcceptedClients(*devs, devAccepted.AcceptedClientSeq_);
   if (ifind != devs->end() && *ifind == &devAccepted) {
      devs->erase(ifind);
      intrusive_ptr_release(&devAccepted);
   }
}

DeviceSP DeviceListener::GetAcceptedClient(StrView* acceptedClientSeqAndOthers) {
   const char*             endptr;
   DeviceAcceptedClientSeq seq = StrTo(*acceptedClientSeqAndOthers, DeviceAcceptedClientSeq{}, &endptr);
   acceptedClientSeqAndOthers->SetBegin(endptr);
   StrTrim(acceptedClientSeqAndOthers);

   AcceptedClients::Locker devs{this->AcceptedClients_};
   auto ifind = FindAcceptedClients(*devs, seq);
   return (ifind == devs->end() ? DeviceSP{} : DeviceSP{*ifind});
}
void DeviceListener::OpImpl_CloseAcceptedClient(StrView acceptedClientSeqAndOthers) {
   if (DeviceSP dev = this->GetAcceptedClient(&acceptedClientSeqAndOthers))
      dev->AsyncClose(acceptedClientSeqAndOthers.ToString());
}
void DeviceListener::OpImpl_LingerCloseAcceptedClient(StrView acceptedClientSeqAndOthers) {
   if (DeviceSP dev = this->GetAcceptedClient(&acceptedClientSeqAndOthers))
      dev->AsyncLingerClose(acceptedClientSeqAndOthers.ToString());
}

} } // namespaces
