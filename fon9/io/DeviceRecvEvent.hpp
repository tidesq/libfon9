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
///      void ContinueRecv(RecvBuffer& rbuf, RecvBufferSize expectSize, bool isEnableReadable) const;
///      void DisableReadableEvent(RecvBuffer& rbuf);
///   };
/// \endcode
template <class Aux>
void DeviceRecvBufferReady(Device& dev, DcQueueList& rxbuf, Aux& aux) {
   RecvBufferSize contRecvSize;
   RecvBuffer&    rbuf = RecvBuffer::StaticCast(rxbuf);
   DeviceOpLocker rlocker;
   if (auto fnRecvDirect = dev.Session_->FnOnDevice_RecvDirect_) {
      struct RecvDirectAux : public RecvDirectArgs {
         fon9_NON_COPY_NON_MOVE(RecvDirectAux);
         Aux& OrigAux_;
         RecvDirectAux(DeviceOpLocker& opLocker, Device& dev, DcQueueList& rxbuf, Aux& origAux)
            : RecvDirectArgs(opLocker, dev, rxbuf)
            , OrigAux_(origAux) {
         }
         virtual bool IsRecvBufferAlive() const override {
            return this->OrigAux_.IsRecvBufferAlive(this->Device_, RecvBuffer::StaticCast(this->RecvBuffer_));
         }
         virtual SendDirectResult SendDirect(BufferList&& txbuf) override {
            return this->OrigAux_.SendDirect(*this, std::move(txbuf));
         }
      };
      RecvDirectAux raux{rlocker, dev, rxbuf, aux};
      fon9_WARN_DISABLE_SWITCH;
      switch (contRecvSize = (*fnRecvDirect)(raux)) {
      default:                               goto __UNLOCK_AND_CONTINUE_RECV;
      case RecvBufferSize::NoLink:           goto __UNLOCK_FOR_NO_LINK;
      case RecvBufferSize::AsyncRecvEvent:   break;
      }
      fon9_WARN_POP;
   }
   else {
      rlocker.Create(dev, AQueueTaskKind::Recv);
      if (fon9_LIKELY(rlocker.GetALocker().IsAllowInplace_)) {
         if (fon9_LIKELY(dev.OpImpl_GetState() == State::LinkReady && aux.IsRecvBufferAlive(dev, rbuf))) {
            rlocker.GetALocker().UnlockForInplace();
            contRecvSize = dev.Session_->OnDevice_Recv(dev, rxbuf);
__UNLOCK_AND_CONTINUE_RECV:
            rlocker.Destroy();
            // ContinueRecv() 必須在離開 alocker 之後才處理, 因為:
            // 有可能在 ContinueRecv() 啟動 Recv 之後, alocker 解構(解除 OpQueue_ 的 Recv 旗標)之前,
            // 就觸發了新的資料到達, 如果沒有先 Destroy(), 則後來的資料到達事件可能會到 op thread 裡面去處理.
            rbuf.SetContinueRecv();
            aux.ContinueRecv(rbuf, contRecvSize, false);
         }
         else {
__UNLOCK_FOR_NO_LINK:
            rlocker.Destroy();
            rxbuf.MoveOut();
         }
         return;
      }
      // 不能立即觸發事件, 就使用 async 機制, 到 op queue 觸發事件.
   }

   rbuf.SetWaitingEventInvoke();
   aux.DisableReadableEvent(rbuf);
   fon9_GCC_WARN_DISABLE("-Wshadow");
   rlocker.GetALocker().AddAsyncTask(DeviceAsyncOp{[&rxbuf, aux](Device& dev) {
      fon9_LOG_WARN("Async.DeviceRecvBufferReady");
      RecvBuffer& rbuf = RecvBuffer::StaticCast(rxbuf);
      if (dev.OpImpl_GetState() == State::LinkReady && aux.IsRecvBufferAlive(dev, rbuf)) {
         RecvBufferSize contRecvSize = dev.Session_->OnDevice_Recv(dev, rxbuf);
         rbuf.SetContinueRecv();
         aux.ContinueRecv(rbuf, contRecvSize, true);
      }
   }});
   fon9_GCC_WARN_POP;
}
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_DeviceRecvEvent_hpp__
