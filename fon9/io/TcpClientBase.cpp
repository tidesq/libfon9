/// \file fon9/io/TcpClientBase.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/TcpClientBase.hpp"

namespace fon9 { namespace io {

bool TcpClientBase::CreateSocket(Socket& so, const SocketAddress& addr, SocketResult& soRes) {
   return so.CreateDeviceSocket(addr.GetAF(), SocketType::Stream, soRes);
}
void TcpClientBase::OpImpl_Connected(Socket::socket_t so) {
   auto errc = Socket::LoadSocketErrC(so);
   if (errc) {
      OpThr_SetBrokenState(*this, RevPrintTo<std::string>("err=", errc));
      return;
   }
   SocketAddress  addrLocal;
   socklen_t      addrLen = sizeof(addrLocal);
   getsockname(so, &addrLocal.Addr_, &addrLen);
   char     uidbuf[kMaxTcpConnectionUID];
   StrView  uidstr = MakeTcpConnectionUID(uidbuf, &this->RemoteAddress_, &addrLocal);
   if (this->RemoteAddress_ == addrLocal) {
      std::string errmsg{"err=self-connect|id="};
      uidstr.AppendTo(errmsg);
      OpThr_SetBrokenState(*this, std::move(errmsg));
   }
   else {
      this->OpImpl_SetConnected(uidstr.ToString());
   }
}

} } // namespaces
