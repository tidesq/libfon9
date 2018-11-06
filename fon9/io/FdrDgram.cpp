/// \file fon9/io/FdrDgram.cpp
/// \author fonwinz@gmail.com
#include "fon9/sys/Config.h"
#ifdef fon9_POSIX
#include "fon9/io/FdrDgram.hpp"

namespace fon9 { namespace io {

bool FdrDgramImpl::OpImpl_ConnectTo(const SocketAddress& addr, SocketResult& soRes) {
   this->State_ = State::Connecting;
   if (!addr.IsEmpty()) {
      // connect(remote address) for send: writev() 的對方位置.
      if (::connect(this->GetFD(), &addr.Addr_, addr.GetAddrLen()) != 0) {
         soRes = SocketResult{"connect"};
         return false;
      }
   }
   this->State_ = State::Connected;
   this->Owner_->OpImpl_Connected(this->GetFD());
   return true;
}

void FdrDgramImpl::OnFdrEvent_Handling(FdrEventFlag evs) {
   FdrEventProcessor(this, *this->Owner_, evs);
}
void FdrDgramImpl::OnFdrEvent_StartSend() {
   this->StartSend(*this->Owner_);
}
void FdrDgramImpl::OnFdrSocket_Error(std::string errmsg) {
   this->Owner_->OnSocketError(this, std::move(errmsg));
}
void FdrDgramImpl::SocketError(StrView fnName, int eno) {
   switch (eno) {
   case ECONNREFUSED: /* 111:Connection refused */
      // 若對方(如果對方為本機ip)沒有 listen udp port, 則在 writev() 時會有此錯誤!
      return;
   }
   base::SocketError(fnName, eno);
}

} } // namespaces
#endif
