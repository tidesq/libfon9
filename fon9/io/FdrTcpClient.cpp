/// \file fon9/io/FdrTcpClient.cpp
/// \author fonwinz@gmail.com
#include "fon9/sys/Config.h"
#ifdef fon9_POSIX
#include "fon9/io/FdrTcpClient.hpp"
#include "fon9/RevPrint.hpp"

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
   ::shutdown(this->GetFD(), SHUT_WR);
   this->RemoveFdrEvent();
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

void FdrTcpClientImpl::OnFdrSocket_Error(StrView fnName, int eno) {
   this->RemoveFdrEvent();
   std::string errmsg;
   if (eno)
      errmsg = RevPrintTo<std::string>(fnName, ':', GetSysErrC(eno));
   else {
      fnName.AppendTo(errmsg);
      errmsg.append(":disconnected.");
   }
   this->Owner_->OnSocketError(this, errmsg);
}
bool FdrTcpClientImpl::CheckRead() {
   size_t   totrd = 0;
   if (fon9_LIKELY(this->RecvSize_ >= RecvBufferSize::Default && !this->IsClosing_)) {
      size_t expectSize = (this->RecvSize_ == RecvBufferSize::Default
                           ? 1024 * 4
                           : static_cast<size_t>(this->RecvSize_));
      if (expectSize < 64)
         expectSize = 64;

      for (;;) {
         struct iovec   bufv[2];
         bufv[1].iov_len = 0;

         size_t   bufCount = this->RecvBuffer_.GetRecvBlockVector(bufv, expectSize);
         ssize_t  bytesTransfered = readv(this->GetFD(), bufv, static_cast<int>(bufCount));
         if (bytesTransfered > 0) {
            DcQueueList&   rxbuf = this->RecvBuffer_.SetDataReceived(bytesTransfered);
            this->OnRecvBufferReady(*this->Owner_, rxbuf, &OwnerDevice::OpImpl_IsBufferAlive);
            if ((totrd += bytesTransfered) > 1024 * 256) // 避免一次占用太久, 所以先結束.
               return true;
            if (static_cast<size_t>(bytesTransfered) < (bufv[0].iov_len + bufv[1].iov_len))
               return true;
         }
         else if (bytesTransfered < 0)
            goto __READ_ERROR;
         else
            break;
      } // for (;;) readv();
   }
   else {
      char  buf[1024 * 256];
      for (;;) {
         ssize_t  rdsz = read(this->GetFD(), buf, sizeof(buf));
         if (rdsz < 0)
            goto __READ_ERROR;
         if (rdsz == 0)
            break;
         totrd += rdsz;
      }
   }
   if (totrd > 0)
      return true;
   //  read() == 0: disconnect.
   this->OnFdrSocket_Error("Recv", 0);
   return false;

__READ_ERROR:
   this->OnFdrSocket_Error("Recv", errno);
   return false;
}
void FdrTcpClientImpl::OnFdrEvent_Handling(FdrEventFlag evs) {
   if (fon9_UNLIKELY(this->IsClosing_))
      return;

   if (IsEnumContains(evs, FdrEventFlag::Readable)) {
      if (!this->CheckRead())
         return;
   }

   if (IsEnumContains(evs, FdrEventFlag::Writable)) {
      if (fon9_LIKELY(this->IsConnected_)) {
         ContinueSendAux aux;
         this->ContinueSend(*this->Owner_, aux);
      }
      else if (!IsEnumContains(evs, FdrEventFlag::Error)) {
         // 連線失敗用 FdrEventFlag::Error 方式通知.
         // 所以 !IsEnumContains(evs, FdrEventFlag::Error) 才需要處理連線成功事件 OnSocketConnected().
         this->IsConnected_ = true;
         this->DisableEvent();
         this->Owner_->OnSocketConnected(this, this->GetFD());
      }
   }

   if (IsEnumContains(evs, FdrEventFlag::Error)) {
      this->OnFdrSocket_Error("Event", Socket::LoadSocketErrno(this->GetFD()));
   }
   else if (IsEnumContains(evs, FdrEventFlag::OperationCanceled)) {
      this->OnFdrSocket_Error("Cancel", ECANCELED);
   }
}
void FdrTcpClientImpl::OnFdrEvent_StartSend() {
   ContinueSendAux aux;
   this->ContinueSend(*this->Owner_, aux);
}

//--------------------------------------------------------------------------//

} } // namespaces
#endif
