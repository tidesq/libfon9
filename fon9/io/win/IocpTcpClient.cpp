/// \file fon9/io/win/IocpTcpClient.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/win/IocpTcpClient.hpp"
#include "fon9/Log.hpp"

fon9_WARN_DISABLE_PADDING;
namespace fon9 { namespace io {

unsigned IocpTcpClient::IocpClient::IocpSocketAddRef() {
   return intrusive_ptr_add_ref(this);
}
unsigned IocpTcpClient::IocpClient::IocpSocketReleaseRef() {
   return intrusive_ptr_release(this);
}

void IocpTcpClient::IocpClient::OpImpl_Close() {
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
bool IocpTcpClient::IocpClient::OpImpl_ConnectTo(const SocketAddress& addr, SocketResult& soRes) {
   // 使用 ConnectEx() 一定要先 bind 否則會: 10022: WSAEINVAL; 在 TcpClientBase 已經 bind 過了!
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
         std::string errmsg = this->GetErrorMessage(lpOverlapped, eno);
         static_cast<IocpTcpClient*>(&dev)->OpImpl_ConnectToNext(&errmsg);
      }
   }});
}

bool IocpTcpClient::IocpClient::OpImpl_IsSocketAlive(Device& dev, IocpSocket* impl) {
   return static_cast<IocpTcpClient*>(&dev)->ImplSP_.get() == impl;
}
void IocpTcpClient::IocpClient::OnIocpSocket_Received(DcQueueList& rxbuf) {
   if (fon9_LIKELY(!this->IsClosing_))
      this->InvokeRecvEvent(*this->Owner_, rxbuf, &OpImpl_IsSocketAlive);
   else
      rxbuf.MoveOut();
}
void IocpTcpClient::IocpClient::OnIocpSocket_Writable(DWORD bytesTransfered) {
   if (fon9_UNLIKELY(this->IsClosing_))
      this->ContinueSend(nullptr, bytesTransfered, nullptr);
   else if (fon9_LIKELY(this->IsConnected_))
      this->ContinueSend(this->Owner_.get(), bytesTransfered, &OpImpl_IsSocketAlive);
   else {
      this->IsConnected_ = true;
      // 任何预先设定的套接字选项或属性，都不会自动拷贝到连接套接字。
      // 为了达到这一目的，在建立连接之后，应用程序必须在套接字上调用 SO_UPDATE_CONNECT_CONTEXT。
      // 否則像是 shutdown(); 之類的操作都沒有任何作用!!
      setsockopt(this->Socket_.GetSocketHandle(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
      if (this->Owner_->OpImpl_GetState() >= State::LinkReady)
         return;
      this->Owner_->OpQueue_.AddTask(DeviceAsyncOp{[this](Device& dev) {
         if (dev.OpImpl_GetState() == State::Linking
             && static_cast<IocpTcpClient*>(&dev)->ImplSP_.get() == this)
               static_cast<IocpTcpClient*>(&dev)->OpImpl_Connected(this->Socket_);
      }});
   }
}

//--------------------------------------------------------------------------//

void IocpTcpClient::OpImpl_TcpLinkBroken() {
   this->OpImpl_ResetImplSP();
}
void IocpTcpClient::OpImpl_TcpClearLinking() {
   base::OpImpl_TcpClearLinking();
   this->OpImpl_ResetImplSP();
}
bool IocpTcpClient::OpImpl_TcpConnect(Socket&& soCli, SocketResult& soRes) {
   this->OpImpl_ResetImplSP();
   this->ImplSP_.reset(new IocpClient{this, std::move(soCli), soRes});
   if (soRes.IsError())
      return false;
   return this->ImplSP_->OpImpl_ConnectTo(this->RemoteAddress_, soRes);
}
void IocpTcpClient::OpImpl_StartRecv(RecvBufferSize preallocSize) {
   if (IocpClient* impl = this->ImplSP_.get())
      impl->StartRecv(preallocSize);
}

//--------------------------------------------------------------------------//

Device::SendResult IocpTcpClient::SendASAP(const void* src, size_t size) {
   SendCheckerMem sc{src, size};
   return sc.Send(*this, nullptr);
}
Device::SendResult IocpTcpClient::SendASAP(BufferList&& src) {
   SendCheckerBuf sc{&src};
   return sc.Send(*this, nullptr);
}
Device::SendResult IocpTcpClient::SendBuffered(const void* src, size_t size) {
   DcQueueList    buffered;
   SendCheckerMem sc{src, size};
   return sc.Send(*this, &buffered);
}
Device::SendResult IocpTcpClient::SendBuffered(BufferList&& src) {
   DcQueueList    buffered;
   SendCheckerBuf sc{&src};
   return sc.Send(*this, &buffered);
}

bool IocpTcpClient::IsSendBufferEmpty() const {
   bool res;
   this->OpQueue_.WaitInvoke(AQueueTaskKind::Send, DeviceAsyncOp{[&res](Device& dev) {
      if (IocpClient* impl = static_cast<IocpTcpClient*>(&dev)->ImplSP_.get())
         res = impl->GetSendBuffer().IsEmpty();
      else
         res = true;
   }});
   return res;
}

} } // namespaces
fon9_WARN_POP;
