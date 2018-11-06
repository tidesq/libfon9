/// \file fon9/io/win/IocpTcpClient.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/win/IocpTcpClient.hpp"

namespace fon9 { namespace io {

void IocpTcpClientImpl::OpImpl_Close() {
   this->State_ = State::Closing;
   // 不考慮繼續傳送 SendBuffer_, 若有需要等候送完, 應使用 (Device::LingerClose + 系統的 linger) 機制.
   if (::shutdown(this->Socket_.GetSocketHandle(), SD_SEND) != 0)
      // shutdown() 有 CancelIoEx() 的效果, 所以可以不用再呼叫 CancelIoEx().
      // 但如果 shutdown() 失敗, 有可能是 ConnectEx() 尚未完成, 所以在 shutdown() 失敗時呼叫 CancelIoEx().
      CancelIoEx(reinterpret_cast<HANDLE>(this->Socket_.GetSocketHandle()), &this->SendOverlapped_);

   // --------------------
   // 本地 shutdown(SD_SEND) 之後:
   // - 對方會 recv.bytes = 0, 然後對方資料送完後也會 shutdown(SD_SEND);
   //   => 本地端會得到 recv.bytes = 0
   //   => 然後 ReleaseRef() => delete this => 本地端 closesocket();
   // - 若對方沒有 shutdown() 或 close(), 大約 2 分鐘後 Windows 會透過 RecvOverlapped_ 觸發:
   //   "ERROR_SEM_TIMEOUT:121(0x79): The semaphore timeout period has expired." (信號等待逾時)
}
bool IocpTcpClientImpl::OpImpl_ConnectTo(const SocketAddress& addr, SocketResult& soRes) {
   // 使用 ConnectEx() 一定要先 bind 否則會: 10022: WSAEINVAL;
   // !AddrBind_.IsEmpty() 在 SocketClientDevice::OpImpl_ConnectToNext() 已經 bind 過了!
   // 所以這裡只有在 AddrBind_.IsEmpty() 時需要 bind.
   if (this->Owner_->Config_.AddrBind_.IsEmpty()) {
      if (!this->Socket_.Bind(this->Owner_->Config_.AddrBind_, soRes))
         return false;
   }
   this->State_ = State::Connecting;
   ZeroStruct(this->SendOverlapped_);
   intrusive_ptr_add_ref(this);
   if (FnConnectEx(this->Socket_.GetSocketHandle(), &addr.Addr_, addr.GetAddrLen(),
                   nullptr, 0, nullptr,//SendBuffer
                   &this->SendOverlapped_)) {
   }
   else {
      int eno = WSAGetLastError();
      if (eno != WSA_IO_PENDING) {
         soRes = SocketResult("ConnectEx", GetSocketErrC(eno));
         intrusive_ptr_release(this);
         return false;
      }
   }
   return true;
}

void IocpTcpClientImpl::OnIocpSocket_Error(OVERLAPPED* lpOverlapped, DWORD eno) {
   this->Owner_->OnSocketError(this, this->GetErrorMessage(lpOverlapped, eno));
}
void IocpTcpClientImpl::OnIocpSocket_Received(DcQueueList& rxbuf) {
   if (fon9_LIKELY(this->State_ >= State::Connecting)) {
      struct RecvAux : public IocpRecvAux {
         static bool IsRecvBufferAlive(Device& dev, RecvBuffer& rbuf) {
            return OwnerDevice::OpImpl_IsRecvBufferAlive(*static_cast<OwnerDevice*>(&dev), rbuf);
         }
      };
      RecvAux aux;
      DeviceRecvBufferReady(*this->Owner_, rxbuf, aux);
   }
   else
      rxbuf.MoveOut();
}
void IocpTcpClientImpl::OnIocpSocket_Writable(DWORD bytesTransfered) {
   if (fon9_LIKELY(this->State_ == State::Connected)) {
      OwnerDevice::ContinueSendAux aux{bytesTransfered};
      DeviceContinueSend(*this->Owner_, this->SendBuffer_, aux);
   }
   else if (fon9_LIKELY(this->State_ == State::Connecting)) {
      this->State_ = State::Connected;
      // 任何预先设定的套接字选项或属性，都不会自动拷贝到连接套接字。
      // 为了达到这一目的，在建立连接之后，应用程序必须在套接字上调用 SO_UPDATE_CONNECT_CONTEXT。
      // 否則像是 shutdown(); 之類的操作都沒有任何作用!!
      setsockopt(this->Socket_.GetSocketHandle(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
      this->Owner_->OnSocketConnected(this, this->Socket_.GetSocketHandle());
   }
   else {
      // Closing 即使有剩餘資料, 也不用再送了!
      // 若有需要等候送完, 應使用 (Device::LingerClose + 系統的 linger) 機制.
   }
}

} } // namespaces
