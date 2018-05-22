/// \file fon9/io/FdrSocket.cpp
/// \author fonwinz@gmail.com
#include "fon9/sys/Config.h"
#ifdef fon9_POSIX
#include "fon9/io/FdrSocket.hpp"

namespace fon9 { namespace io {

FdrEventFlag FdrSocket::GetRequiredFdrEventFlag() const {
   return static_cast<FdrEventFlag>(this->EnabledEvents_.load(std::memory_order_relaxed));
}

void FdrSocket::SocketError(StrView fnName, int eno) {
   this->RemoveFdrEvent();
   std::string errmsg;
   if (eno)
      errmsg = RevPrintTo<std::string>(fnName, ':', GetSocketErrC(eno));
   else {
      fnName.AppendTo(errmsg);
      errmsg.append(":disconnected.");
   }
   this->OnFdrSocket_Error(std::move(errmsg));
}

int FdrSocket::Sendv(DeviceOpLocker& sc, DcQueueList& toSend) {
   struct iovec   bufv[IOV_MAX];
   size_t         bufCount = toSend.PeekBlockVector(bufv);
   ssize_t        wrsz = writev(this->GetFD(), bufv, static_cast<int>(bufCount));
   if (fon9_LIKELY(wrsz >= 0)) {
      toSend.PopConsumed(static_cast<size_t>(wrsz));
      if (fon9_LIKELY(toSend.empty()))
         this->CheckSendQueueEmpty(sc);
      else
         this->EnableEventBit(FdrEventFlag::Writable);
      return 0;
   }
   if (int eno = ErrorCannotRetry(errno)) {
      this->SocketError("Sendv", eno);
      return eno;
   }
   this->EnableEventBit(FdrEventFlag::Writable);
   return 0;
}

void FdrSocket::CheckSendQueueEmpty(DeviceOpLocker& sc) {
   auto& alocker = sc.GetALocker();
   alocker.Relock();
   if (fon9_LIKELY(!this->SendBuffer_.OpImpl_CheckSendQueue())) {
      sc.GetDevice().AsyncCheckSendEmpty(alocker);
      // 雖然返回後會 sc 就會死亡, 但是因為已經 Relock(),
      // 所以會有許多 call stack 的 local variables 也在 lock 的範圍內.
      // 會造成 unlock() 時 memory barrier 的負擔, 因此在這兒就先將 sc.Destroy(), 可以加快一些速度.
      sc.Destroy();
      return;
   }
   // 因為 this->StartSendInFdrThread(); 裡面通常會有 mutex 保護.
   //   - 為了避免 [sc保護 + FdrThread保護] 的多重 lock
   //   - 讓 sc 能盡快釋放資源.
   // 所以此處執行 sc.Destroy();
   // 但在 sc.Destroy() 之後, this 有可能會死亡, e.g. FdrTcpClient: ImplSP_.reset();
   // 所以使用 protectedThis 先將 this 保護起來, 再呼叫 this->StartSendInFdrThread();
   FdrEventHandlerSP protectedThis{this};
   sc.Destroy();
   this->StartSendInFdrThread();
}

bool FdrSocket::CheckRead(Device& dev, bool (*fnIsRecvBufferAlive)(Device& dev, RecvBuffer& rbuf)) {
   size_t   totrd = 0;
   if (fon9_LIKELY(this->RecvSize_ >= RecvBufferSize::Default)) {
      for (;;) {
         size_t expectSize = (this->RecvSize_ == RecvBufferSize::Default
                              ? 1024 * 4
                              : static_cast<size_t>(this->RecvSize_));
         if (expectSize < 64)
            expectSize = 64;

         struct iovec   bufv[2];
         bufv[1].iov_len = 0;

         size_t   bufCount = this->RecvBuffer_.GetRecvBlockVector(bufv, expectSize);
         ssize_t  bytesTransfered = readv(this->GetFD(), bufv, static_cast<int>(bufCount));
         if (fon9_LIKELY(bytesTransfered > 0)) {
            DcQueueList&   rxbuf = this->RecvBuffer_.SetDataReceived(bytesTransfered);

            struct RecvAux : public FdrRecvAux {
               bool (*FnIsRecvBufferAlive_)(Device& dev, RecvBuffer& rbuf);
               bool IsRecvBufferAlive(Device& dev, RecvBuffer& rbuf) const {
                  return this->FnIsRecvBufferAlive_ == nullptr || this->FnIsRecvBufferAlive_(dev, rbuf);
               }
            };
            RecvAux aux;
            aux.FnIsRecvBufferAlive_ = fnIsRecvBufferAlive;
            DeviceRecvBufferReady(dev, rxbuf, aux);

            // 實際取出的資料量, 比要求取出的少 => 資料已全部取出, 所以結束 Recv.
            if (fon9_LIKELY(static_cast<size_t>(bytesTransfered) < (bufv[0].iov_len + bufv[1].iov_len)))
               return true;

            // 避免一次占用太久, 所以先結束.
            if ((totrd += bytesTransfered) > 1024 * 256)
               return true;

            // 關閉 readable 偵測: 需要到 op thread 處理 Recv 事件, 所以結束 Recv.
            // 再根據 OnDevice_Recv() 事件處理結果, 決定是否重新啟用 readable 偵測.
            if (fon9_UNLIKELY(aux.IsNeedsUpdateFdrEvent_))
               return true;

            // Session 決定不要再處理 OnDevice_Recv() 事件, 所以拋棄全部已收到的資料.
            if (fon9_UNLIKELY(this->RecvSize_ < RecvBufferSize::Default))
               goto __DROP_RECV;
         }
         else if (bytesTransfered < 0)
            goto __READ_ERROR;
         else
            break;
      } // for (;;) readv();
   }
   else {
   __DROP_RECV:
      this->RecvBuffer_.Clear();
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
   this->SocketError("Recv", 0);
   return false;

__READ_ERROR:
   if (int eno = ErrorCannotRetry(errno)) {
      this->SocketError("Recv", eno);
      return false;
   }
   return true;
}

//--------------------------------------------------------------------------//

SendDirectResult FdrSocket::FdrRecvAux::SendDirect(RecvDirectArgs& e, BufferList&& txbuf) {
   FdrSocket&  so = ContainerOf(RecvBuffer::StaticCast(e.RecvBuffer_), &FdrSocket::RecvBuffer_);
   if (fon9_LIKELY(so.SendBuffer_.IsEmpty())) {
      // 使用 SendDirect() 不考慮另一 thread 同時送.
      DcQueueList    toSend{std::move(txbuf)};
      struct iovec   bufv[IOV_MAX];
      size_t         bufCount = toSend.PeekBlockVector(bufv);
      ssize_t        wrsz = writev(so.GetFD(), bufv, static_cast<int>(bufCount));
      if (fon9_LIKELY(wrsz >= 0)) {
         toSend.PopConsumed(static_cast<size_t>(wrsz));
         if (fon9_LIKELY(toSend.empty()))
            return SendDirectResult::Sent;
         txbuf.push_back(toSend.MoveOut());
      }
      else if (int eno = ErrorCannotRetry(errno)) {
         toSend.ConsumeErr(GetSysErrC(eno));
         return SendDirectResult::SendError;
      }
   }
   SendASAP_AuxBuf  aux{txbuf};
   return DeviceSendDirect(e, so.SendBuffer_, aux);
}

} } // namespaces
#endif
