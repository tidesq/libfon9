/// \file fon9/io/IoBufferSend.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_IoBufferSend_hpp__
#define __fon9_io_IoBufferSend_hpp__
#include "fon9/io/IoBuffer.hpp"
#include "fon9/DyObj.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace io {

/// \ingroup io
/// 實際使用請參考 IoBufferSend();
/// 此為一次性物件, 不可重複使用.
struct SendChecker {
   using ALocker = DeviceOpQueue::ALockerForInplace;

   void Destroy() {
      this->ALocker_.clear();
   }
   bool IsLinkReady(Device& dev) {
      // 這裡呼叫 dev.OpImpl_GetState() 雖然不安全.
      // 但因底下有 double check, 所以先排除「尚未連線」是可行的.
      if (fon9_LIKELY(dev.OpImpl_GetState() == State::LinkReady)) {
         this->ALocker_.emplace(dev.OpQueue_, AQueueTaskKind::Send);
         if (fon9_LIKELY(dev.OpImpl_GetState() == State::LinkReady))
            return true;
         this->Destroy();
      }
      return false;
   }
   /// 允許立即送出 or 放入 queue.
   bool IsAllowInplace() {
      ALocker* alocker = this->ALocker_.get();
      return alocker->IsAllowInplace_      // 此時為 op safe, 才能安全的使用 dev & IoBuffer.
         || alocker->IsWorkingSameTask(); // 不能 op safe 的原因是有其他人正在送(但已經unlock), 此時應放入 queue.
   }
   DcQueueList* ToSendingAndUnlock(IoBuffer& iobuf) {
      return iobuf.SendBuffer_.ToSendingAndUnlock(*this->ALocker_);
   }
   BufferList& GetQueueForPush(IoBuffer& iobuf) {
      return iobuf.SendBuffer_.GetQueueForPush(*this->ALocker_);
   }
   void AsyncSend(ObjHolderPtr<BufferList> pbuf, IoBuffer* impl, IoBuffer::FnOpImpl_IsBufferAlive fnIsBufferAlive) {
      this->ALocker_->AddAsyncTask(DeviceAsyncOp{[pbuf, impl, fnIsBufferAlive](Device& dev) {
         if (fnIsBufferAlive(dev, impl))
            dev.SendASAP(std::move(*pbuf));
         else
            BufferListConsumeErr(std::move(*pbuf), std::errc::operation_canceled);
      }});
   }
   void AsyncSend(ObjHolderPtr<BufferList> pbuf) {
      this->ALocker_->AddAsyncTask(DeviceAsyncOp{[pbuf](Device& dev) {
         dev.SendASAP(std::move(*pbuf));
      }});
   }
private:
   DyObj<ALocker> ALocker_;
};

//--------------------------------------------------------------------------//

struct SendAuxMem {
   const void* Src_;
   size_t      Size_;
   SendAuxMem(const void* src, size_t size) : Src_{src}, Size_{size} {
   }
   static void NoLink() {
   }
   void PushTo(BufferList& buf) {
      AppendToBuffer(buf, this->Src_, this->Size_);
   }
};

struct SendAuxBuf {
   BufferList* Src_;
   SendAuxBuf(BufferList& src) : Src_{&src} {
   }
   void NoLink() {
      BufferListConsumeErr(std::move(*this->Src_), std::errc::no_link);
   }
   void PushTo(BufferList& buf) {
      buf.push_back(std::move(*this->Src_));
   }
};

/// \ingroup io
/// struct Aux {
///   // 取出 ToSending 成功後, SendBuffer 已進入 Sending 狀態, 此時可解構 alocker, 然後送出.
///   // 在 alocker 死亡前, 必須先將 impl 保護起來.
///   // 因為 TcpClient 的 ImplSP_ 隨時可能會死亡,
///   // 如果不保護, 則 impl 可能會隨著 ImplSP_ 一起死.
///   // => [傳送準備(執行必要的保護措施), 避免 alocker 解構後相關物件的死亡]
///   // - TcpClient: 需要 AddRef()
///   //   - IOCP: WSASend() 完成後, 在 OnIocp_Done() 或 OnIocp_Error() 解除保護.
///   //   - Fdr:  writev() 或 EnableEventBit(Writable); 之後解除保護.
///   // - TcpAcceptedClient:
///   //   - IOCP: 需要 AddRef(): WSASend() 之前不用再 AddRef();
///   //   - Fdr:  直接 writev() 或 EnableEventBit(Writable); 不用額外的 AddRef(), Release();
///   struct ImplProtector {
///      ImplProtector(Impl*);
///   };
///
///   // dev 不是 State::LinkReady.
///   // 在 !sc.IsLinkReady() 時呼叫, 通常清除要求送出的 src buffer:
///   // `BufferListConsumeErr(std::move(src), std::errc::no_link);`
///   void NoLink();
///
///   // Impl 必須是繼承自 IoBuffer, **不可傳回 nullptr**
///   Impl* GetImpl(DeviceT& dev);
///
///   // 在沒有 alocker 的情況下呼叫, [ASAP: 立即送] or [Buffered: 啟動 Writable 偵測, 在 Writable 時送].
///   // - ASAP:
///   //   - IOCP: 先將要送出的資料填入 toSend, 然後送出(WSASend), 然後在 OnIocp_Done() 繼續送.
///   //   - Fdr:  直接送出(send, write)
///   //           - 若有剩餘資料, 則放入 toSend, 然後啟動 writable 偵測.
///   //           - 若無剩餘資料, 開啟 alocker:
///   //             - 若 SendBuffer_.Queue_ 為空: dev.AsyncCheckSendEmpty(alocker);
///   //             - 若有資料: StartSendInFdrThread();
///   // - Buffered:
///   //   - 將要送出的資料填入 toSend, 然後啟動 Writable 事件.
///   // (2) OR send(data) & push remain to (*toSend) & enable Writable event.
///   // (3) if (!SendBuffer_.Queue_.empty()) enable Writable event.
///   Device::SendResult Send(Impl& impl, DcQueueList& toSend);
///
///   // 使用時機, 以下 2 種情況之一:
///   // (1) 取出 ToSending 失敗: SendBuffer 不是空的, 需放到 queue, 等候先前的傳送結束時處理.
///   //     => [存入 Queue] 就結束.
///   //     => 等前一個傳送者處理.
///   // (2) 把要送出的資料填入 buf, 移到 op thread 傳送.
///   void PushTo(BufferList& buf);
///
///   // 把 pbuf 移到 op thread 傳送.
///   void AsyncSend(DeviceT& dev, SendChecker& sc, ObjHolderPtr<BufferList>&& pbuf);
/// };
template <class DeviceT, class Aux>
Device::SendResult IoBufferSend(DeviceT& dev, Aux& aux) {
   SendChecker sc;
   if (fon9_LIKELY(sc.IsLinkReady(dev))) {
      if (fon9_LIKELY(sc.IsAllowInplace())) {
         auto*  impl = aux.GetImpl(dev);
         assert(impl);
         if (DcQueueList* toSend = sc.ToSendingAndUnlock(*impl)) {
            typename Aux::ImplProtector   implProtector{impl}; // 因為 sc.Destroy() 之後 impl 就沒有保障了!
            sc.Destroy();
            return aux.Send(dev, *impl, *toSend);
         }
         aux.PushTo(sc.GetQueueForPush(*impl));
      }
      else { // 移到 op thread 傳送.
         auto pbuf{MakeObjHolder<BufferList>()};
         aux.PushTo(*pbuf);
         aux.AsyncSend(dev, sc, std::move(pbuf));
      }
      return Device::SendResult{0};
   }
   aux.NoLink();
   return Device::SendResult{std::errc::no_link};
}

//--------------------------------------------------------------------------//

/// \ingroup io
/// 請參考:
/// - IocpTcpClientDeviceBase
/// - IocpTcpListener::AcceptedClient
/// - FdrTcpClientDeviceBase
template <class DeviceBase, class IoImpl>
class DeviceImpl_IoBufferSend : public DeviceBase {
   fon9_NON_COPY_NON_MOVE(DeviceImpl_IoBufferSend);
   DeviceImpl_IoBufferSend() = delete;
public:
   using DeviceBase::DeviceBase;

   Device::SendResult SendASAP(const void* src, size_t size) {
      if (size <= 0)
         return Device::SendResult{0};
      typename DeviceBase::template SendAux<typename IoImpl::SendASAP_AuxMem>   aux{src, size};
      return IoBufferSend(*this, aux);
   }
   Device::SendResult SendASAP(BufferList&& src) {
      if (src.empty())
         return Device::SendResult{0};
      typename DeviceBase::template SendAux<typename IoImpl::SendASAP_AuxBuf>   aux{src};
      return IoBufferSend(*this, aux);
   }
   Device::SendResult SendBuffered(const void* src, size_t size) {
      if (size <= 0)
         return Device::SendResult{0};
      typename DeviceBase::template SendAux<typename IoImpl::SendBuffered_AuxMem>   aux{src, size};
      return IoBufferSend(*this, aux);
   }
   Device::SendResult SendBuffered(BufferList&& src) {
      if (src.empty())
         return Device::SendResult{0};
      typename DeviceBase::template SendAux<typename IoImpl::SendBuffered_AuxBuf>   aux{src};
      return IoBufferSend(*this, aux);
   }
};

//-////////////////////////////////////////////////////////////////////////-//

template <class Aux>
void IoBuffer::ContinueSend(Device& dev, Aux& aux) {
   DcQueueList* toSend;
   for (;;) {
      DeviceOpQueue::ALockerForInplace alocker{dev.OpQueue_, AQueueTaskKind::Send};
      if (alocker.IsAllowInplace_) {
         if (!aux.IsBufferAlive(dev, this))
            return;

         if (fon9_LIKELY(IsAllowContinueSend(dev.OpImpl_GetState()))) {
            if ((toSend = aux.GetToSend(this->SendBuffer_)) != nullptr)
               goto __START_SEND;
            dev.AsyncCheckSendEmpty(alocker);
         }
         return;
      }

      aux.DisableWritableEvent(*this);
      fon9_WARN_DISABLE_PADDING;
      alocker.AddAsyncTask(DeviceAsyncOp{[this, aux](Device& adev) {
         fon9_LOG_TRACE("Async.ContinueSend|dev=", ToPtr{&adev});
         if (fon9_LIKELY(aux.IsBufferAlive(adev, this)))
            if (fon9_LIKELY(IsAllowContinueSend(adev.OpImpl_GetState()))) {
               if (DcQueueList* aToSend = aux.GetToSend(this->SendBuffer_))
                  aux.StartSend(adev, *this, *aToSend);
               else
                  Device::OpThr_CheckSendEmpty(adev, std::string{});
            }
      }});
      fon9_WARN_POP;
      return;
   }

__START_SEND:
   aux.StartSend(dev, *this, *toSend);
}

} } // namespaces
#endif//__fon9_io_IoBufferSend_hpp__
