/// \file fon9/io/FdrTcpClient.cpp
/// \author fonwinz@gmail.com
#include "fon9/sys/Config.h"
#ifdef fon9_POSIX
#include "fon9/io/FdrTcpClient.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 { namespace io {

#ifdef fon9_WINDOWS
int write(SOCKET s, const void* src, size_t size) {
   return ::send(s, static_cast<const char*>(src), static_cast<int>(size), 0);
}
#endif

//--------------------------------------------------------------------------//

bool FdrTcpClient::IsSendBufferEmpty() const {
   return true;
}
FdrTcpClient::SendResult FdrTcpClient::SendASAP(const void* src, size_t size) {
   // SendBuffer::ALocker alocker{this->OpQueue_, this->SendBuffer_, src, size};
   // if (alocker.LockResult_ == SendBuffer::LockResult::AllowASAP) {
   //    auto wsz = write(Socket_.GetSocketHandle(), src, size);
   // }
   return SendResult{0};
}
FdrTcpClient::SendResult FdrTcpClient::SendASAP(BufferList&& src) {
   return SendResult{0};
}
FdrTcpClient::SendResult FdrTcpClient::SendBuffered(const void* src, size_t size) {
   return SendResult{0};
}
FdrTcpClient::SendResult FdrTcpClient::SendBuffered(BufferList&& src) {
   return SendResult{0};
}

//--------------------------------------------------------------------------//

bool FdrTcpClient::OpImpl_TcpConnect(Socket&& soCli, SocketResult& soRes) {
   if (::connect(soCli.GetSocketHandle(), &this->RemoteAddress_.Addr_, this->RemoteAddress_.GetAddrLen()) == 0)
      this->OpImpl_Connected(soCli);
   else {
      int eno;
      #ifdef fon9_WINDOWS
      if ((eno = WSAGetLastError()) != WSAEWOULDBLOCK)
      #else
      if ((eno = errno) != EINPROGRESS)
      #endif
      {
         soRes = SocketResult("connect", GetSocketErrC(eno));
         return false;
      }
      if (0); // this->IoService_.SetDetectWriteable(*this); // for detect connect.
   }
   this->Socket_ = std::move(soCli);
   return true;
}

} } // namespaces
#endif