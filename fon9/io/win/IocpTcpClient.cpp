/// \file fon9/io/win/IocpTcpClient.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/win/IocpTcpClient.hpp"
#include "fon9/Log.hpp"

fon9_WARN_DISABLE_PADDING;
namespace fon9 { namespace io {

//--------------------------------------------------------------------------//

unsigned IocpTcpClient::IocpClient::AddRef() {
   return intrusive_ptr_add_ref(this);
}
unsigned IocpTcpClient::IocpClient::ReleaseRef() {
   return intrusive_ptr_release(this);
}

void IocpTcpClient::IocpClient::OpThr_Close() {
   this->IsClosing_ = true;
   // 不考慮繼續傳送 SendBuffer_, 若有需要等候送完, 應使用 Device 的 LingerClose() 及系統的 linger 機制.
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
bool IocpTcpClient::IocpClient::OpThr_ConnectTo(const SocketAddress& addr, SocketResult& soRes) {
   // 使用 ConnectEx() 一定要先 bind 否則會: 10022: WSAEINVAL; 在 TcpClientBase 已經 bind 過了!
   this->IocpAttach(this->Socket_.GetSocketHandle());
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
void IocpTcpClient::IocpClient::OnIocpSocket_Error(OVERLAPPED* lpOverlapped, DWORD eno) {
   if (this->IsClosing_)
      return;
   DeviceOpQueue::ALockerForAsyncTask alocker{this->Owner_->OpQueue_, AQueueTaskKind::Get};
   if (alocker.IsAllowInvoke_) { // 檢查 Owner_ 是否仍在使用 this; 如果沒有在用 this, 則不用通知 Owner_;
      if (this->Owner_->ImplSP_.get() != this)
         return;
   }
   alocker.AddAsyncTask(DeviceAsyncOp{[=](Device& dev) {
      if (static_cast<IocpTcpClient*>(&dev)->ImplSP_ == this) {
         std::string errmsg{this->GetOverlappedKind(lpOverlapped).ToString()};
         errmsg.push_back(':');
         if (eno == WSAEDISCON)
            errmsg.append("disconnected.");
         else
            RevPrintAppendTo(errmsg, GetSysErrC(eno));
         static_cast<IocpTcpClient*>(&dev)->OpThr_ConnectToNext(&errmsg);
      }
   }});
}
void IocpTcpClient::IocpClient::OnIocpSocket_Recv(DcQueueList& rxbuf) {
   if (fon9_UNLIKELY(this->IsClosing_)) {
__CLOSING_DROP_RECV:
      rxbuf.MoveOut();
      return;
   }
   IocpTcpClient& owner = *this->Owner_;
   RecvBufferSize contRecvSize;
   for (;;) {
      DeviceOpQueue::ALockerForInvoke alocker{owner.OpQueue_, AQueueTaskKind::Recv};
      if (alocker.IsAllowInvoke_) {
         if (owner.ImplSP_.get() != this || owner.OpThr_GetState() != State::LinkReady)
            goto __CLOSING_DROP_RECV;
         alocker.UnlockForInvoke();
         contRecvSize = owner.Session_->OnDevice_Recv(owner, rxbuf);
         break;
      }
      this->RecvBuffer_.SetWaitingEventInvoke();
      alocker.AddAsyncTask(DeviceAsyncOp{[this, &rxbuf](Device& dev) {
         fon9_LOG_DEBUG("Async.Recv");
         IocpTcpClient& owner = *static_cast<IocpTcpClient*>(&dev);
         if (owner.ImplSP_.get() == this && owner.OpThr_GetState() == State::LinkReady)
            this->ContinueRecv(owner.Session_->OnDevice_Recv(owner, rxbuf));
         else { // 到這兒才發現 this 被關閉, this 已經死掉了嗎? 此時要重啟 readable 偵測嗎?
            // => this 應該已經在 OpThr_ResetImpl()
            //    或 OpThr_Close() 裡面呼叫 shutdown(Send) 後
            //    => 然後在 OnIocp_Error(SendOverlapped_, operation_cancel) 裡面死亡了!
         }
      }});
      return;
   }
   // 離開 alocker 之後, 才處理 ContinueRecv(),
   // 因為有可能在 this->ContinueRecv() 啟動 Recv 之後,
   // alocker 解構(解除 OpQueue_ 的 Recv 旗標)之前,
   // 就觸發了新的資料到達.
   this->ContinueRecv(contRecvSize);
}
void IocpTcpClient::IocpClient::OnIocpSocket_Writable(DWORD bytesTransfered) {
   //fon9_LOG_DEBUG("Writable|bytesTransfered=", bytesTransfered);
   DcQueueList* qu;
   if (fon9_UNLIKELY(this->IsClosing_)) {
__CLOSING_CONTINUE_SEND:
      if ((qu = this->SendBuffer_.ContinueSend(bytesTransfered)) != nullptr) {
__CONTINUE_SEND:
         this->AddRef();
         this->SendAfterAddRef(*qu);
         //fon9_LOG_DEBUG("Writable|AfterSent");
      }
      return;
   }
   if (fon9_UNLIKELY(!this->IsConnected_)) {
      this->IsConnected_ = true;
      // 任何预先设定的套接字选项或属性，都不会自动拷贝到连接套接字。
      // 为了达到这一目的，在建立连接之后，应用程序必须在套接字上调用 SO_UPDATE_CONNECT_CONTEXT。
      // 否則像是 shutdown(); 之類的操作都沒有任何作用!!
      setsockopt(this->Socket_.GetSocketHandle(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
   }
   DeviceOpQueue::ALockerForInvoke alocker{this->Owner_->OpQueue_, AQueueTaskKind::Send};
   if (alocker.IsAllowInvoke_) {
      if (this->Owner_->ImplSP_.get() != this)
         // 取消 alocker 的保護, 取出 qu, 然後送出.
         goto __CLOSING_CONTINUE_SEND;
      if (fon9_LIKELY(IsAllowContinueSend(this->Owner_->OpThr_GetState()))) {
         // 在 alocker 的保護下, 取出 qu, 然後送出(送出時不用 alocker 保護).
         if ((qu = this->SendBuffer_.ContinueSend(bytesTransfered)) != nullptr)
            goto __CONTINUE_SEND;
         this->Owner_->AsyncCheckLingerClose(alocker);
         return;
      }
   }
   alocker.AddAsyncTask(DeviceAsyncOp{[this, bytesTransfered](Device& dev) {
      if (static_cast<IocpTcpClient*>(&dev)->ImplSP_.get() != this)
         return;
      State st = static_cast<IocpTcpClient*>(&dev)->OpThr_GetState();
      if (fon9_LIKELY(IsAllowContinueSend(st))) {
         fon9_LOG_DEBUG("Async.Writable|bytesTransfered=", bytesTransfered);
         if (DcQueueList* qu = this->SendBuffer_.ContinueSend(bytesTransfered)) {
            this->AddRef();
            this->SendAfterAddRef(*qu);
         }
         else if (st == State::Lingering)
            static_cast<IocpTcpClient*>(&dev)->OpThr_CheckLingerClose(std::string{});
      }
      else if (st == State::Linking)
         static_cast<IocpTcpClient*>(&dev)->OpThr_Connected(this->Socket_);
   }});
}

//--------------------------------------------------------------------------//

void IocpTcpClient::OpThr_TcpLinkBroken() {
   this->OpThr_ResetImpl();
}
void IocpTcpClient::OpThr_TcpClearLinking() {
   base::OpThr_TcpClearLinking();
   this->OpThr_ResetImpl();
}
bool IocpTcpClient::OpThr_ConnectToImpl(Socket&& soCli, SocketResult& soRes) {
   this->OpThr_ResetImpl();
   this->ImplSP_.reset(new IocpClient{this, std::move(soCli)});
   return this->ImplSP_->OpThr_ConnectTo(this->RemoteAddress_, soRes);
}
void IocpTcpClient::OpThr_StartRecv(RecvBufferSize preallocSize) {
   if (IocpClient* impl = this->ImplSP_.get())
      impl->StartRecv(preallocSize);
}

//--------------------------------------------------------------------------//

struct IocpTcpClient::SendChecker {
   IocpClient*    Impl_{nullptr};
   DcQueueList*   ToSending_{nullptr};

   virtual void PushTo(BufferList&) = 0;

   std::errc SendASAP(IocpTcpClient& dev) {
      if (fon9_UNLIKELY(dev.OpThr_GetState() != State::LinkReady))
         // 這裡呼叫 this->OpThr_GetState() 雖然不安全.
         // 但因底下有 double check, 所以先排除「尚未連線」是可行的.
         return std::errc::no_link;
      DeviceOpQueue::ALockerForInvoke alocker{dev.OpQueue_, AQueueTaskKind::Send};
      if (fon9_LIKELY(alocker.IsAllowInvoke_) // 此時為 op safe, 才能安全的使用 impl & GetState().
          || alocker.IsWorkingSameTask() /* 不能 op safe 的原因是有其他人正在送(但已經unlock), 此時應放入 queue. */) {
         if (fon9_UNLIKELY(dev.OpThr_GetState() != State::LinkReady))
            return std::errc::no_link;
         this->Impl_ = dev.ImplSP_.get();
         assert(this->Impl_ != nullptr); // LinkReady 時 impl 必定不會是 nullptr.
         this->ToSending_ = this->Impl_->GetSendBuffer().ToSendingAndUnlock(alocker);
         if (this->ToSending_) {
            assert(alocker.IsAllowInvoke_);
            // 允許立即送出.
            this->Impl_->AddRef();
            // SendBuffer 已進入 Sending 狀態, 此時 alocker 可解構, 然後送出.
         }
         else {
            assert(alocker.IsWorkingSameTask());
            // SendBuffer 不是空的, 則需放到 queue, 等候先前的傳送結束時, 再處理 queue.
            this->PushTo(this->Impl_->GetSendBuffer().GetQueueForPush(alocker));
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
};

struct IocpTcpClient::SendCheckerMem : public SendChecker {
   const void* Mem_;
   size_t      Size_;
   SendCheckerMem(const void* src, size_t size) : Mem_{src}, Size_{size} {
   }
   virtual void PushTo(BufferList& buf) override {
      AppendToBuffer(buf, this->Mem_, this->Size_);
   }
};
struct IocpTcpClient::SendCheckerBuf : public SendChecker {
   BufferList* Src_;
   SendCheckerBuf(BufferList* src) : Src_{src} {
   }
   virtual void PushTo(BufferList& buf) override {
      buf.push_back(std::move(*this->Src_));
   }
};

Device::SendResult IocpTcpClient::CheckSend(const void* src, size_t size, DcQueueList* buffered) {
   if (fon9_UNLIKELY(size <= 0))
      return SendResult{0};
   SendCheckerMem sc{src, size};
   std::errc      err;
   if ((err = sc.SendASAP(*this)) == std::errc{}) {
      if (sc.ToSending_) {
         sc.ToSending_->Append(src, size);
         return sc.Impl_->SendAfterAddRef(buffered ? *buffered : *sc.ToSending_);
      }
      return SendResult{0};
   }
   return err;
}
Device::SendResult IocpTcpClient::CheckSend(BufferList&& src, DcQueueList* buffered) {
   SendCheckerBuf sc{&src};
   std::errc      err;
   if ((err = sc.SendASAP(*this)) == std::errc{}) {
      if (sc.ToSending_) {
         sc.ToSending_->push_back(std::move(src));
         return sc.Impl_->SendAfterAddRef(buffered ? *buffered : *sc.ToSending_);
      }
      return SendResult{0};
   }
   return err;
}

//--------------------------------------------------------------------------//
Device::SendResult IocpTcpClient::SendASAP(const void* src, size_t size) {
   return this->CheckSend(src, size, nullptr);
}
Device::SendResult IocpTcpClient::SendASAP(BufferList&& src) {
   return this->CheckSend(std::move(src), nullptr);
}
Device::SendResult IocpTcpClient::SendBuffered(const void* src, size_t size) {
   DcQueueList buffered;
   return this->CheckSend(src, size, &buffered);
}
Device::SendResult IocpTcpClient::SendBuffered(BufferList&& src) {
   DcQueueList buffered;
   return this->CheckSend(std::move(src), &buffered);
}

bool IocpTcpClient::IsSendBufferEmpty() const {
   bool res;
   this->OpQueue_.WaitInvoke(AQueueTaskKind::Send, DeviceAsyncOp{[&res](Device& dev) {
      if (IocpClient* impl = static_cast<IocpTcpClient*>(&dev)->ImplSP_.get())
         res = impl->IsSendBufferEmpty();
      else
         res = true;
   }});
   return res;
}

} } // namespaces
fon9_WARN_POP;
