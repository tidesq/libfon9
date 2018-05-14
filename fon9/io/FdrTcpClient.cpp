/// \file fon9/io/FdrTcpClient.cpp
/// \author fonwinz@gmail.com
#include "fon9/sys/Config.h"
#ifdef fon9_POSIX
#include "fon9/io/FdrTcpClient.hpp"

namespace fon9 { namespace io {

void FdrTcpClientImpl::OnFdrEvent_AddRef() {
   intrusive_ptr_add_ref(static_cast<baseCounter*>(this));
}
void FdrTcpClientImpl::OnFdrEvent_ReleaseRef() {
   intrusive_ptr_release(static_cast<baseCounter*>(this));
}

void FdrTcpClientImpl::OpImpl_Close() {
   this->IsClosing_ = true;
   this->EnabledEvents_.store(0, std::memory_order_relaxed);
   this->RemoveFdrEvent();
   ::shutdown(this->GetFD(), SHUT_WR);
}
bool FdrTcpClientImpl::OpImpl_ConnectTo(const SocketAddress& addr, SocketResult& soRes) {
   if (::connect(this->GetFD(), &addr.Addr_, addr.GetAddrLen()) == 0) {
      this->Owner_->OpImpl_Connected(this->GetFD());
   }
   else {
      int eno = errno;
      if (eno != EINPROGRESS) {
         soRes = SocketResult{"connect", GetSocketErrC(eno)};
         return false;
      }
      this->EnableEventBit(FdrEventFlag::Writable);
   }
   return true;
}

void FdrTcpClientImpl::OnFdrSocket_Error(std::string errmsg) {
   this->Owner_->OnSocketError(this, std::move(errmsg));
}
void FdrTcpClientImpl::OnFdrEvent_Handling(FdrEventFlag evs) {
   if (fon9_UNLIKELY(this->IsClosing_))
      return;

   if (IsEnumContains(evs, FdrEventFlag::Readable)) {
      if (!this->CheckRead(*this->Owner_, &OwnerDevice::OpImpl_IsRecvBufferAlive))
         return;
   }

   if (IsEnumContains(evs, FdrEventFlag::Writable)) {
      if (fon9_LIKELY(this->IsConnected_)) {
         OwnerDevice::ContinueSendAux aux;
         DeviceContinueSend(*this->Owner_, this->SendBuffer_, aux);
      }
      else if (!IsEnumContains(evs, FdrEventFlag::Error)) {
         // 連線失敗用 FdrEventFlag::Error 方式通知.
         // 所以 !IsEnumContains(evs, FdrEventFlag::Error) 才需要處理連線成功事件 OnSocketConnected().
         this->IsConnected_ = true;
         this->DisableEvent();
         this->Owner_->OnSocketConnected(this, this->GetFD());
      }
   }

   this->CheckSocketErrorOrCanceled(evs);
}
void FdrTcpClientImpl::OnFdrEvent_StartSend() {
   OwnerDevice::ContinueSendAux aux;
   DeviceContinueSend(*this->Owner_, this->SendBuffer_, aux);
}

} } // namespaces
#endif
