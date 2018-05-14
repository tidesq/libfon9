/// \file fon9/io/DeviceStartSend.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_DeviceStartSend_hpp__
#define __fon9_io_DeviceStartSend_hpp__
#include "fon9/io/SendBuffer.hpp"
#include "fon9/DyObj.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace io {

/// \ingroup io
/// 實際使用請參考 DeviceStartSend();
/// 此為一次性物件, 不可重複使用.
struct SendChecker {
   using ALocker = DeviceOpQueue::ALockerForInplace;
   typedef bool (*FnOpImpl_IsSendBufferAlive)(Device& owner, SendBuffer& sbuf);

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
      return alocker->IsAllowInplace_     // 此時為 op safe, 才能安全的使用 dev & sbuf.
         || alocker->IsWorkingSameTask(); // 不能 op safe 的原因是有其他人正在送(但已經unlock), 此時應放入 queue.
   }
   DcQueueList* ToSendingAndUnlock(SendBuffer& sbuf) {
      return sbuf.ToSendingAndUnlock(*this->ALocker_);
   }
   BufferList& GetQueueForPush(SendBuffer& sbuf) {
      return sbuf.GetQueueForPush(*this->ALocker_);
   }
   void AsyncSend(ObjHolderPtr<BufferList> pbuf, SendBuffer& sbuf, FnOpImpl_IsSendBufferAlive fnIsSendBufferAlive) {
      this->ALocker_->AddAsyncTask(DeviceAsyncOp{[pbuf, &sbuf, fnIsSendBufferAlive](Device& dev) {
         if (fnIsSendBufferAlive(dev, sbuf))
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
///   // 在 alocker 死亡前, 必須先將 sbuf 保護起來.
///   // 因為 TcpClient 的 ImplSP_ 隨時可能會死亡,
///   // 如果不保護, 則 sbuf 可能會隨著 ImplSP_ 一起死.
///   // => [傳送準備(執行必要的保護措施), 避免 alocker 解構後相關物件的死亡]
///   // - TcpClient: 需要 AddRef()
///   //   - IOCP: WSASend() 完成後, 在 OnIocp_Done() 或 OnIocp_Error() 解除保護.
///   //   - Fdr:  writev() 或 EnableEventBit(Writable); 之後解除保護.
///   // - TcpAcceptedClient:
///   //   - IOCP: 需要 AddRef(): WSASend() 之前不用再 AddRef();
///   //   - Fdr:  直接 writev() 或 EnableEventBit(Writable); 不用額外的 AddRef(), Release();
///   struct SendBufferProtector {
///      SendBufferProtector(SendBuffer&);
///   };
///
///   // dev 不是 State::LinkReady.
///   // 在 !sc.IsLinkReady() 時呼叫, 通常清除要求送出的 src buffer:
///   // `BufferListConsumeErr(std::move(src), std::errc::no_link);`
///   void NoLink();
///
///   SendBuffer& GetSendBuffer(DeviceT& dev);
///
///   // 在沒有 alocker 的情況下呼叫, [ASAP: 立即送] or [Buffered: 啟動 Writable 偵測, 在 Writable 時送].
///   // - ASAP:
///   //   - IOCP: 先將要送出的資料填入 toSend, 然後送出(WSASend), 然後在 OnIocp_Done() 繼續送.
///   //   - Fdr:  直接送出(send, write)
///   //           - 若有剩餘資料, 則放入 toSend, 然後啟動 writable 偵測.
///   //           - 若無剩餘資料, 開啟 alocker:
///   //             - 若 sbuf.Queue_ 為空: dev.AsyncCheckSendEmpty(alocker);
///   //             - 若有資料: StartSendInFdrThread();
///   // - Buffered:
///   //   - 將要送出的資料填入 toSend, 然後啟動 Writable 事件.
///   // (2) OR send(data) & push remain to (*toSend) & enable Writable event.
///   // (3) if (!sbuf.Queue_.empty()) enable Writable event.
///   Device::SendResult StartSend(DeviceT& dev, DcQueueList& toSend);
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
Device::SendResult DeviceStartSend(DeviceT& dev, Aux& aux) {
   SendChecker sc;
   if (fon9_LIKELY(sc.IsLinkReady(dev))) {
      if (fon9_LIKELY(sc.IsAllowInplace())) {
         auto&  sbuf = aux.GetSendBuffer(dev);
         if (DcQueueList* toSend = sc.ToSendingAndUnlock(sbuf)) {
            typename Aux::SendBufferProtector   sbufProtector{sbuf}; // 因為 sc.Destroy() 之後 sbuf 就沒有保障了!
            sc.Destroy();
            return aux.StartSend(dev, *toSend);
         }
         aux.PushTo(sc.GetQueueForPush(sbuf));
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
/// 透過 IoImpl 及 DeviceBase 實作 SendASAP(), SendBuffer();
/// 請參考:
/// - IocpTcpClientDeviceBase
/// - IocpTcpListener::AcceptedClient
/// - FdrTcpClientDeviceBase
template <class DeviceBase, class IoImpl>
class DeviceImpl_DeviceStartSend : public DeviceBase {
   fon9_NON_COPY_NON_MOVE(DeviceImpl_DeviceStartSend);
   DeviceImpl_DeviceStartSend() = delete;
public:
   using DeviceBase::DeviceBase;

   Device::SendResult SendASAP(const void* src, size_t size) {
      if (size <= 0)
         return Device::SendResult{0};
      typename DeviceBase::template SendAux<typename IoImpl::SendASAP_AuxMem>   aux{src, size};
      return DeviceStartSend(*this, aux);
   }
   Device::SendResult SendASAP(BufferList&& src) {
      if (src.empty())
         return Device::SendResult{0};
      typename DeviceBase::template SendAux<typename IoImpl::SendASAP_AuxBuf>   aux{src};
      return DeviceStartSend(*this, aux);
   }
   Device::SendResult SendBuffered(const void* src, size_t size) {
      if (size <= 0)
         return Device::SendResult{0};
      typename DeviceBase::template SendAux<typename IoImpl::SendBuffered_AuxMem>   aux{src, size};
      return DeviceStartSend(*this, aux);
   }
   Device::SendResult SendBuffered(BufferList&& src) {
      if (src.empty())
         return Device::SendResult{0};
      typename DeviceBase::template SendAux<typename IoImpl::SendBuffered_AuxBuf>   aux{src};
      return DeviceStartSend(*this, aux);
   }
};

//-////////////////////////////////////////////////////////////////////////-//

/// 您必須自行確保一次只能有一個 thread 呼叫 DeviceContinueSend()
/// - io thread: at Writable event.
/// - op thread: at Async send request.
/// \code
///   // 必須是 copyable, 因為可能會到 alocker.AddAsyncTask() 裡面使用.
///   struct Aux {
///      // 若 sbuf 已不屬於 dev, 則放棄傳送.
///      // 若有需要等候送完, 應使用 (Device::LingerClose + 系統的 linger) 機制.
///      bool IsSendBufferAlive(DeviceT& dev, SendBuffer& sbuf);
///
///      // 在 alocker 的保護下, 取出 toSend, 然後送出, 送出時不用 alocker 保護:
///      // => 因為 sbuf 已進入 Sending 狀態.
///      // => 其他傳送要求在 sbuf.ToSendingAndUnlock(alocker) 會被阻擋
///      //    => 然後該次要求會放到 sbuf.Queue_ 裡面.
///      DcQueueList* GetToSend(SendBuffer& sbuf) const;
///
///      // 在 AddAsyncTask(task) 之前, 取消 writable 偵測.
///      // => 取消 writable 偵測的原因:
///      //    => 為了避免在 task 尚未執行前, socket 可能在 writable 狀態.
///      //    => 如果沒有關掉 writable 偵測, 則會一直觸發 writable 事件.
///      // => 為何需要在 AddAsyncTask(task) **之前** 取消?
///      //    => 因為在 AddAsyncTask(task) 返回前, task 有可能已執行完畢.
///      //       => task 可能會開啟 writable 偵測.
///      //    => 若在 AddAsyncTask() 之後才取消 writable 偵測,
///      //       則可能會關掉 task 開啟的 writable 偵測!
///      // => 可是要注意: 現在還是 alocker 的狀態!
///      void DisableWritableEvent(SendBuffer& sbuf);
///
///      // 在安全可立即送出的情況下, 會呼叫此處傳送.
///      // 安全檢查及確保方法:
///      // - 從 sbuf 安全的取出 toSend, 且 alocker 已解構.
///      //   您必須自行確保 DeviceContinueSend() 返回前 sbuf 不會死亡,
///      //   這樣才可以在沒 alocker 且沒其他保護的情況下呼叫 StartSend();
///      // - 或在 op thread 使用 aux.IsSendBufferAlive() 確定 sbuf 仍然存活時.
///      void StartContinueSend(DeviceT& dev, DcQueueList& toSend) const;
///   };
/// \endcode
template <class DeviceT, class Aux>
void DeviceContinueSend(DeviceT& dev, SendBuffer& sbuf, Aux& aux) {
   DcQueueList* toSend;
   for (;;) {
      DeviceOpQueue::ALockerForInplace alocker{dev.OpQueue_, AQueueTaskKind::Send};
      if (alocker.IsAllowInplace_) {
         if (!aux.IsSendBufferAlive(dev, sbuf))
            return;

         if (fon9_LIKELY(IsAllowContinueSend(dev.OpImpl_GetState()))) {
            if ((toSend = aux.GetToSend(sbuf)) != nullptr)
               goto __START_SEND;
            dev.AsyncCheckSendEmpty(alocker);
         }
         return;
      }

      aux.DisableWritableEvent(sbuf);
      fon9_WARN_DISABLE_PADDING;
      alocker.AddAsyncTask(DeviceAsyncOp{[&sbuf, aux](Device& adev) {
         fon9_LOG_WARN("Async.DeviceContinueSend|dev=", ToPtr{&adev});
         if (fon9_LIKELY(aux.IsSendBufferAlive(adev, sbuf)))
            if (fon9_LIKELY(IsAllowContinueSend(adev.OpImpl_GetState()))) {
               if (DcQueueList* aToSend = aux.GetToSend(sbuf))
                  aux.StartContinueSend(*static_cast<DeviceT*>(&adev), *aToSend);
               else
                  Device::OpThr_CheckSendEmpty(adev, std::string{});
            }
      }});
      fon9_WARN_POP;
      return;
   }

__START_SEND:
   aux.StartContinueSend(dev, *toSend);
}

} } // namespaces
#endif//__fon9_io_DeviceStartSend_hpp__
