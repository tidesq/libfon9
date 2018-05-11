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
   this->OnFdrSocket_Error("Sendv", eno);
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

} } // namespaces
#endif
