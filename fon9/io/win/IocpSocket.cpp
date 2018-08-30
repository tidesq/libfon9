/// \file fon9/io/win/IocpSocket.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/win/IocpSocket.hpp"

namespace fon9 { namespace io {

IocpSocket::IocpSocket(IocpServiceSP iosv, Socket&& so, SocketResult& soRes)
   : IocpHandler{std::move(iosv)}
   , Socket_{std::move(so)} {
   auto res = this->IocpAttach(this->Socket_.GetSocketHandle());
   if (res.IsError())
      soRes = SocketResult{"IocpAttach", res.GetError()};
}
IocpSocket::~IocpSocket() {
   this->SendBuffer_.ForceClearBuffer(this->Eno_ ? GetSysErrC(this->Eno_) : ErrC{std::errc::operation_canceled});
}
std::string IocpSocket::GetErrorMessage(OVERLAPPED* lpOverlapped, DWORD eno) const {
   std::string errmsg{this->GetOverlappedKind(lpOverlapped).ToString()};
   errmsg.push_back(':');
   if (eno == WSAEDISCON)
      errmsg.append("disconnected.");
   else
      RevPrintAppendTo(errmsg, GetSysErrC(eno));
   return errmsg;
}

void IocpSocket::OnIocp_Error(OVERLAPPED* lpOverlapped, DWORD eno) {
   if (this->Eno_ == 0)
      this->Eno_ = eno;
   this->OnIocpSocket_Error(lpOverlapped, eno);
   this->IocpSocketReleaseRef();
}
bool IocpSocket::DropRecv() {
   DWORD    bytesTransfered = 0;
   char     buf[1024 * 256];
   WSABUF   bufv{sizeof(buf), buf};
   DWORD    rxBytes, flags = 0;
   while (WSARecv(this->Socket_.GetSocketHandle(), &bufv, 1, &rxBytes, &flags, nullptr, nullptr) != SOCKET_ERROR && rxBytes != 0) {
      bytesTransfered += rxBytes;
   }
   int eno = WSAGetLastError();
   if (eno == WSAEWOULDBLOCK) {
      this->ContinueRecv(RecvBufferSize::NoRecvEvent);
      return true;
   }
   if (eno == 0 && bytesTransfered == 0) // 正常斷線, 沒有錯誤?!
      eno = WSAEDISCON;
   this->OnIocp_Error(&this->RecvOverlapped_, static_cast<DWORD>(eno));
   return false;
}
void IocpSocket::OnIocp_Done(OVERLAPPED* lpOverlapped, DWORD bytesTransfered) {
   if (fon9_LIKELY(lpOverlapped == &this->RecvOverlapped_)) {
      if (fon9_LIKELY(bytesTransfered > 0)) {
         DcQueueList& rxbuf = this->RecvBuffer_.SetDataReceived(bytesTransfered);
         this->OnIocpSocket_Received(rxbuf);
      }
      else if (!this->DropRecv()) // DropRecv() 返回失敗, 表示已經呼叫過 OnIocp_Error(); 所以不用再 ReleaseRef();
         return;                  // 因此直接 return;
   }
   else if (lpOverlapped == &this->SendOverlapped_) {
      this->OnIocpSocket_Writable(bytesTransfered);
   }
   this->IocpSocketReleaseRef();
}

//--------------------------------------------------------------------------//

void IocpSocket::StartRecv(RecvBufferSize expectSize) {
   WSABUF bufv[2];
   size_t bufCount;
   if (fon9_UNLIKELY(expectSize < RecvBufferSize::Default)) {
      // 不理會資料事件: Session預期不會有資料.
      // RecvBufferSize::NoRecvEvent, CloseRecv; //還是需要使用[接收事件]偵測斷線.
      bufv[0].len = 0;
      bufv[0].buf = nullptr;
      bufCount = 1;
   }
   else {
      if (expectSize == RecvBufferSize::Default) // 接收緩衝區預設大小.
         expectSize = static_cast<RecvBufferSize>(1024 * 4);
      const size_t allocSize = static_cast<size_t>(expectSize);
      bufCount = this->RecvBuffer_.GetRecvBlockVector(bufv, allocSize >= 64 ? allocSize : (allocSize * 2));
   }
   // 啟動 WSARecv();
   DWORD rxBytes, flags = 0;
   ZeroStruct(this->RecvOverlapped_);
   this->IocpSocketAddRef();
   if (WSARecv(this->Socket_.GetSocketHandle(), bufv, static_cast<DWORD>(bufCount), &rxBytes, &flags, &this->RecvOverlapped_, nullptr) == SOCKET_ERROR) {
      switch (int eno = WSAGetLastError()) {
      case WSA_IO_PENDING: // ERROR_IO_PENDING: 正常接收等候中.
         break;
      default: // 接收失敗, 不會產生 OnIocp_* 事件
         this->RecvBuffer_.Clear();
         this->OnIocp_Error(&this->RecvOverlapped_, static_cast<DWORD>(eno));
      }
   }
   else {
      // 已收到(或已從RCVBUF複製進來), 但仍會觸發 OnIocp_Done() 事件,
      // 所以一律在 OnIocp_Done() 事件裡面處理即可.
   }
}

//--------------------------------------------------------------------------//

Device::SendResult IocpSocket::SendAfterAddRef(DcQueueList& dcbuf) {
   WSABUF wbuf[64];
   DWORD  wcount = static_cast<DWORD>(dcbuf.PeekBlockVector(wbuf));
   if (fon9_UNLIKELY(wcount == 0)) {
      wcount = 1;
      wbuf[0].len = 0;
      wbuf[0].buf = nullptr;
   }

   DWORD  txBytes = 0, flags = 0;
   ZeroStruct(this->SendOverlapped_);
   if (WSASend(this->Socket_.GetSocketHandle(), wbuf, wcount, &txBytes, flags, &this->SendOverlapped_, nullptr) == SOCKET_ERROR) {
      switch (int eno = WSAGetLastError()) {
      case WSA_IO_PENDING: // ERROR_IO_PENDING: 正常傳送等候中.
         break;
      default: // 傳送失敗, 不會產生 OnIocp_* 事件
         this->OnIocp_Error(&this->SendOverlapped_, static_cast<DWORD>(eno));
         return GetSocketErrC(eno);
      }
   }
   else {
      // 已送出(已填入SNDBUF), 但仍會觸發 OnIocp_Done() 事件,
      // 所以一律在 OnIocp_Done() 事件裡面處理即可.
   }
   return Device::SendResult{0};
}

//--------------------------------------------------------------------------//

SendDirectResult IocpSocket::IocpRecvAux::SendDirect(RecvDirectArgs& e, BufferList&& txbuf) {
   IocpSocket&      so = ContainerOf(RecvBuffer::StaticCast(e.RecvBuffer_), &IocpSocket::RecvBuffer_);
   SendASAP_AuxBuf  aux{txbuf};
   return DeviceSendDirect(e, so.SendBuffer_, aux);
}

} } // namespaces
