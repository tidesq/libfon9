/// \file fon9/io/win/IocpDgram.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/win/IocpDgram.hpp"

namespace fon9 { namespace io {

void IocpDgramImpl::OpImpl_Close() {
   this->State_ = State::Closing;
   CancelIoEx(reinterpret_cast<HANDLE>(this->Socket_.GetSocketHandle()), &this->SendOverlapped_);
   CancelIoEx(reinterpret_cast<HANDLE>(this->Socket_.GetSocketHandle()), &this->RecvOverlapped_);
}
bool IocpDgramImpl::OpImpl_ConnectTo(const SocketAddress& addr, SocketResult& soRes) {
   (void)soRes;
   this->State_ = State::Connecting;
   if (!addr.IsEmpty()) {
      assert(addr == this->Owner_->RemoteAddress_);
      this->SendTo_ = &this->Owner_->RemoteAddress_;
      //--- 若要同時收送, 則 bind()/connect() 會衝突?
      //--- 例如: RemoteIp:Port|Bind=LocalIp:Port
      //--- 如果 RemoteIp != LocalIp 則收不到訊息!
      // connect(remote address) for send: WSASend() 的對方位置.
      //if (::connect(this->Socket_.GetSocketHandle(), &addr.Addr_, addr.GetAddrLen()) != 0) {
      //   soRes = SocketResult{"connect"};
      //   return false;
      //}
   }
   this->State_ = State::Connected;
   this->Owner_->OnSocketConnected(this, this->Socket_.GetSocketHandle());
   return true;
}
void IocpDgramImpl::OnIocpSocket_Error(OVERLAPPED* lpOverlapped, DWORD eno) {
   if (lpOverlapped == &this->RecvOverlapped_) {
      switch (eno) {
      case ERROR_PORT_UNREACHABLE: // 1234:沒有任何服務正在遠端系統上的目的地網路端點上操作。
      case WSAECONNRESET: // 使用 WSASendTo() 若對方(如果對方為本機ip)沒有 listen udp port, 則在 WSARecv() 時會有此錯誤!
         if (this->RecvBuffer_.IsReceiving()) {
            this->RecvBuffer_.SetDataReceived(0);
            this->RecvBuffer_.SetContinueRecv();
         }
         this->ContinueRecv(RecvBufferSize::Default);
         return;
      }
   }
   this->Owner_->OnSocketError(this, this->GetErrorMessage(lpOverlapped, eno));
}
void IocpDgramImpl::OnIocpSocket_Received(DcQueueList& rxbuf) {
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
void IocpDgramImpl::OnIocpSocket_Writable(DWORD bytesTransfered) {
   if (fon9_LIKELY(this->State_ == State::Connected)) {
      OwnerDevice::ContinueSendAux aux{bytesTransfered};
      DeviceContinueSend(*this->Owner_, this->SendBuffer_, aux);
   }
   else if (fon9_LIKELY(this->State_ == State::Connecting)) {
      this->State_ = State::Connected;
      this->Owner_->OnSocketConnected(this, this->Socket_.GetSocketHandle());
   }
   else {
      // Closing 即使有剩餘資料, 也不用再送了!
      // 若有需要等候送完, 應使用 (Device::LingerClose + 系統的 linger) 機制.
   }
}

} } // namespaces
