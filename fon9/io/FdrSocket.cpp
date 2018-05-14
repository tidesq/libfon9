/// \file fon9/io/FdrSocket.cpp
/// \author fonwinz@gmail.com
#include "fon9/sys/Config.h"
#ifdef fon9_POSIX
#include "fon9/io/FdrSocket.hpp"

namespace fon9 { namespace io {

FdrEventFlag FdrSocket::GetRequiredFdrEventFlag() const {
   return static_cast<FdrEventFlag>(this->EnabledEvents_.load(std::memory_order_relaxed));
}

void FdrSocket::StartRecv(RecvBufferSize expectSize) {
   this->RecvSize_ = expectSize;
   this->EnableEventBit(FdrEventFlag::Readable);
}

void FdrSocket::SocketError(StrView fnName, int eno) {
   this->RemoveFdrEvent();
   std::string errmsg;
   if (eno)
      errmsg = RevPrintTo<std::string>(fnName, ':', GetSysErrC(eno));
   else {
      fnName.AppendTo(errmsg);
      errmsg.append(":disconnected.");
   }
   this->OnFdrSocket_Error(std::move(errmsg));
}


int FdrSocket::Sendv(Device& dev, DcQueueList& toSend) {
   struct iovec   bufv[IOV_MAX];
   size_t         bufCount = toSend.PeekBlockVector(bufv);
   ssize_t        wrsz = writev(this->GetFD(), bufv, static_cast<int>(bufCount));
   if (fon9_LIKELY(wrsz >= 0)) {
      toSend.PopConsumed(static_cast<size_t>(wrsz));
      if (fon9_LIKELY(toSend.empty()))
         this->CheckSendQueueEmpty(dev);
      else
         this->EnableEventBit(FdrEventFlag::Writable);
      return 0;
   }
   int eno = errno;
   this->SocketError("Sendv", eno);
   return eno;
}

void FdrSocket::CheckSendQueueEmpty(Device& dev) {
   {
      DeviceOpQueue::ALockerForInplace alocker{dev.OpQueue_, AQueueTaskKind::Send};
      if (fon9_LIKELY(alocker.IsAllowInplace_ && !this->SendBuffer_.OpImpl_CheckSendQueue())) {
         dev.AsyncCheckSendEmpty(alocker);
         return;
      }
   }
   // StartSendInFdrThread() 不需要 alocker, 所以在沒有 alocker 的情況下啟動, 避免佔用 alocker 太久.
   this->StartSendInFdrThread();
}

bool FdrSocket::CheckRead(Device& dev, bool (*fnIsRecvBufferAlive)(Device& dev, RecvBuffer& rbuf)) {
   size_t   totrd = 0;
   if (fon9_LIKELY(this->RecvSize_ >= RecvBufferSize::Default)) {
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
   this->SocketError("Recv", errno);
   return false;
}

} } // namespaces
#endif
