/// \file fon9/io/DeviceRecvEvent.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_DeviceRecvEvent_hpp__
#define __fon9_io_DeviceRecvEvent_hpp__
#include "fon9/io/RecvBuffer.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace io {

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// 輔助處理資料接收:
/// - 觸發資料到達事件: dev.Session_->OnDevice_Recv();
/// - 繼續接收: aux.ContinueRecv();
///
/// \code
///   // 必須是 copyable, 因為可能會到 alocker.AddAsyncTask() 裡面使用.
///   struct Aux {
///      bool IsRecvBufferAlive(Device& dev, RecvBuffer& rbuf) const;
///      void ContinueRecv(RecvBuffer& rbuf, RecvBufferSize expectSize) const;
///      void DisableReadableEvent(RecvBuffer& rbuf);
///   };
/// \endcode
template <class Aux>
void DeviceRecvBufferReady(Device& dev, DcQueueList& rxbuf, Aux& aux) {
   RecvBufferSize contRecvSize;
   RecvBuffer&    rbuf = RecvBuffer::StaticCast(rxbuf);
   for (;;) {
      DeviceOpQueue::ALockerForInplace alocker{dev.OpQueue_, AQueueTaskKind::Recv};
      if (fon9_LIKELY(alocker.IsAllowInplace_)) {
         if (fon9_LIKELY(dev.OpImpl_GetState() == State::LinkReady && aux.IsRecvBufferAlive(dev, rbuf))) {
            alocker.UnlockForInplace();
            contRecvSize = dev.Session_->OnDevice_Recv(dev, rxbuf);
            goto __UNLOCK_AND_CONTINUE_RECV;
         }
         goto __UNLOCK_AND_DROP_RECV;
      }
      rbuf.SetWaitingEventInvoke();
      aux.DisableReadableEvent(rbuf);
      alocker.AddAsyncTask(DeviceAsyncOp{[&rxbuf, aux](Device& adev) {
         fon9_LOG_WARN("Async.DeviceRecvBufferReady");
         RecvBuffer& arbuf = RecvBuffer::StaticCast(rxbuf);
         if (adev.OpImpl_GetState() == State::LinkReady && aux.IsRecvBufferAlive(adev, arbuf)) {
            RecvBufferSize aContRecvSize = adev.Session_->OnDevice_Recv(adev, rxbuf);
            arbuf.SetContinueRecv();
            aux.ContinueRecv(arbuf, aContRecvSize);
         }
      }});
      return;
   }

__UNLOCK_AND_CONTINUE_RECV:
   // ContinueRecv() 必須在離開 alocker 之後才處理, 因為:
   // 有可能在 ContinueRecv() 啟動 Recv 之後, alocker 解構(解除 OpQueue_ 的 Recv 旗標)之前,
   // 就觸發了新的資料到達.
   rbuf.SetContinueRecv();
   aux.ContinueRecv(rbuf, contRecvSize);
   return;

__UNLOCK_AND_DROP_RECV:
   rxbuf.MoveOut();
}
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_DeviceRecvEvent_hpp__
