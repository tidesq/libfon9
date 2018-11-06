/// \file fon9/io/FdrTcpClient.cpp
/// \author fonwinz@gmail.com
#include "fon9/sys/Config.h"
#ifdef fon9_POSIX
#include "fon9/io/FdrTcpClient.hpp"

namespace fon9 { namespace io {

bool FdrTcpClientImpl::OpImpl_ConnectTo(const SocketAddress& addr, SocketResult& soRes) {
   this->State_ = State::Connecting;
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
   FdrEventProcessor(this, *this->Owner_, evs);
}
void FdrTcpClientImpl::OnFdrEvent_StartSend() {
   this->StartSend(*this->Owner_);
}

} } // namespaces
#endif
