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

} } // namespaces
