/// \file fon9/io/win/IocpSocket.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/win/IocpSocket.hpp"
#include "fon9/Log.hpp"

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
   fon9_LOG_TRACE("IocpSocket.OnIocp_Error|err=", this->GetOverlappedKind(lpOverlapped), ':', GetSysErrC(eno));
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
      this->StartRecv(RecvBufferSize::NoRecvEvent);
      return true;
   }
   if (eno == 0 && bytesTransfered == 0) // 正常斷線, 沒有錯誤?!
      eno = WSAEDISCON;
   this->OnIocp_Error(&this->RecvOverlapped_, static_cast<DWORD>(eno));
   return false;
}
void IocpSocket::OnIocp_Done(OVERLAPPED* lpOverlapped, DWORD bytesTransfered) {
   //fon9_LOG_TRACE("IocpSocket.OnIocp_Done|", this->GetOverlappedKind(lpOverlapped),
   //               ".byteTransfered=", bytesTransfered);
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
void IocpSocket::ContinueSend(Device* const dev, DWORD bytesTransfered, FnOpImpl_IsSocketAlive fnIsSocketAlive) {
   DcQueueList* qu;
   if (dev) {
      DeviceOpQueue::ALockerForInvoke alocker{dev->OpQueue_, AQueueTaskKind::Send};
      if (alocker.IsAllowInvoke_) {
         if (fnIsSocketAlive != nullptr && !fnIsSocketAlive(*dev, this))
            // 取消 alocker 的保護, 取出 qu, 然後送出.
            goto __GET_AND_CONTINUE_SEND;
         if (fon9_LIKELY(IsAllowContinueSend(dev->OpImpl_GetState()))) {
            // 在 alocker 的保護下, 取出 qu, 然後送出(送出時不用 alocker 保護).
            if ((qu = this->SendBuffer_.ContinueSend(bytesTransfered)) != nullptr)
               goto __CONTINUE_SEND;
            dev->AsyncCheckSendEmpty(alocker);
            return;
         }
      }
      fon9_WARN_DISABLE_PADDING;
      alocker.AddAsyncTask(DeviceAsyncOp{[this, bytesTransfered, fnIsSocketAlive](Device& dev) {
         fon9_LOG_TRACE("Async.ContinueSend|bytesTransfered=", bytesTransfered);
         if (DcQueueList* qu = this->SendBuffer_.ContinueSend(bytesTransfered)) {
            this->IocpSocketAddRef();
            this->SendAfterAddRef(*qu);
         }
         else if (fnIsSocketAlive == nullptr || fnIsSocketAlive(dev, this))
            Device::OpThr_CheckSendEmpty(dev, std::string{});
      }});
      fon9_WARN_POP;
      return;
   }
__GET_AND_CONTINUE_SEND:
   if ((qu = this->SendBuffer_.ContinueSend(bytesTransfered)) == nullptr)
      return;
__CONTINUE_SEND:
   this->IocpSocketAddRef();
   this->SendAfterAddRef(*qu);
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
         this->IocpSocketReleaseRef();
         this->RecvBuffer_.Clear();
         this->OnIocp_Error(&this->RecvOverlapped_, static_cast<DWORD>(eno));
      }
   }
   else {
      // 已收到(或已從RCVBUF複製進來), 但仍會觸發 OnIocp_Done() 事件,
      // 所以一律在 OnIocp_Done() 事件裡面處理即可.
   }
}
void IocpSocket::InvokeRecvEvent(Device& dev, DcQueueList& rxbuf, FnOpImpl_IsSocketAlive fnIsSocketAlive) {
   RecvBufferSize contRecvSize;
   for (;;) {
      DeviceOpQueue::ALockerForInvoke alocker{dev.OpQueue_, AQueueTaskKind::Recv};
      if (alocker.IsAllowInvoke_) {
         if (dev.OpImpl_GetState() == State::LinkReady
             && (fnIsSocketAlive == nullptr || fnIsSocketAlive(dev, this))) {
            alocker.UnlockForInvoke();
            contRecvSize = dev.Session_->OnDevice_Recv(dev, rxbuf);
            goto __CONTINUE_RECV;
         }
         rxbuf.MoveOut();
         goto __DROP_RECV;
      }
      this->RecvBuffer_.SetWaitingEventInvoke();
      alocker.AddAsyncTask(DeviceAsyncOp{[this, &rxbuf, fnIsSocketAlive](Device& dev) {
         fon9_LOG_TRACE("Async.Recv");
         if (dev.OpImpl_GetState() == State::LinkReady
             && (fnIsSocketAlive == nullptr || fnIsSocketAlive(dev, this)))
            this->ContinueRecv(dev.Session_->OnDevice_Recv(dev, rxbuf));
         else { // 到這兒才發現 this 被關閉, this 已經死掉了嗎? 此時要重啟 readable 偵測嗎?
                // => this 應該已經在 IocpTcpClient::OpImpl_ResetImplSP()
                //    或 OpImpl_Close() 裡面呼叫 shutdown(Send) 後
                //    => 然後在 OnIocp_Error(SendOverlapped_, operation_cancel) 裡面死亡了!
         }
      }});
      return;
   }

__CONTINUE_RECV:
   // 離開 alocker 之後, 才處理 ContinueRecv(),
   // 因為有可能在 this->ContinueRecv() 啟動 Recv 之後,
   // alocker 解構(解除 OpQueue_ 的 Recv 旗標)之前,
   // 就觸發了新的資料到達.
   this->ContinueRecv(contRecvSize);
   return;

__DROP_RECV:
   rxbuf.MoveOut();
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

std::errc IocpSocket::SendChecker::CheckSend(Device& dev) {
   if (fon9_UNLIKELY(dev.OpImpl_GetState() != State::LinkReady))
      // 這裡呼叫 dev.OpImpl_GetState() 雖然不安全.
      // 但因底下有 double check, 所以先排除「尚未連線」是可行的.
      return std::errc::no_link;
   DeviceOpQueue::ALockerForInvoke alocker{dev.OpQueue_, AQueueTaskKind::Send};
   if (fon9_LIKELY(alocker.IsAllowInvoke_) // 此時為 op safe, 才能安全的使用 impl & GetState().
       || alocker.IsWorkingSameTask() /* 不能 op safe 的原因是有其他人正在送(但已經unlock), 此時應放入 queue. */) {
      if (fon9_UNLIKELY(dev.OpImpl_GetState() != State::LinkReady))
         return std::errc::no_link;
      this->IocpSocket_ = this->OpImpl_GetIocpSocket(dev);
      this->ToSending_ = this->IocpSocket_->SendBuffer_.ToSendingAndUnlock(alocker);
      if (this->ToSending_) {
         assert(alocker.IsAllowInvoke_);
         // 允許立即送出.
         this->IocpSocket_->IocpSocketAddRef();
         // SendBuffer 已進入 Sending 狀態, 此時 alocker 可解構, 然後送出.
      }
      else {
         assert(alocker.IsWorkingSameTask());
         // SendBuffer 不是空的, 則需放到 queue, 等候先前的傳送結束時, 再處理 queue.
         this->PushTo(this->IocpSocket_->SendBuffer_.GetQueueForPush(alocker));
      }
   }
   else { // 移到 op thread 傳送?
      auto pbuf{MakeObjHolder<BufferList>()};
      this->PushTo(*pbuf);
      alocker.AddAsyncTask(DeviceAsyncOp{[pbuf](Device& dev) {
         dev.SendASAP(std::move(*pbuf));
      }});
   }
   return std::errc{};
}

} } // namespaces
