/// \file fon9/io/IoBuffer.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/IoBuffer.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace io {

void IoBuffer::OnRecvBufferReady(Device& dev, DcQueueList& rxbuf, FnOpImpl_IsBufferAlive fnIsBufferAlive) {
   RecvBufferSize contRecvSize;
   for (;;) {
      DeviceOpQueue::ALockerForInplace alocker{dev.OpQueue_, AQueueTaskKind::Recv};
      if (fon9_LIKELY(alocker.IsAllowInplace_)) {
         if (fon9_LIKELY(dev.OpImpl_GetState() == State::LinkReady
                         && (fnIsBufferAlive == nullptr || fnIsBufferAlive(dev, this)))) {
            alocker.UnlockForInplace();
            contRecvSize = dev.Session_->OnDevice_Recv(dev, rxbuf);
            goto __UNLOCK_AND_CONTINUE_RECV;
         }
         goto __UNLOCK_AND_DROP_RECV;
      }
      this->RecvBuffer_.SetWaitingEventInvoke();
      alocker.AddAsyncTask(DeviceAsyncOp{[this, &rxbuf, fnIsBufferAlive](Device& adev) {
         fon9_LOG_TRACE("Async.Recv");
         if (adev.OpImpl_GetState() == State::LinkReady
               && (fnIsBufferAlive == nullptr || fnIsBufferAlive(adev, this)))
            this->ContinueRecv(adev.Session_->OnDevice_Recv(adev, rxbuf));
         else { // 到這兒才發現 this 被關閉, this 已經死掉了嗎? 此時要重啟 readable 偵測嗎?
                // => this 應該已經在 IocpTcpClient::OpImpl_ResetImplSP()
                //    或 OpImpl_Close() 裡面呼叫 shutdown(Send) 後
                //    => 然後在 OnIocp_Error(SendOverlapped_, operation_cancel) 裡面死亡了!
         }
      }});
      return;
   }

__UNLOCK_AND_CONTINUE_RECV:
   // 離開 alocker 之後, 才處理 ContinueRecv(),
   // 因為有可能在 this->ContinueRecv() 啟動 Recv 之後,
   // alocker 解構(解除 OpQueue_ 的 Recv 旗標)之前,
   // 就觸發了新的資料到達.
   this->ContinueRecv(contRecvSize);
   return;

__UNLOCK_AND_DROP_RECV:
   rxbuf.MoveOut();
}

} } // namespaces
