/// \file fon9/io/DeviceStartSend.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_DeviceStartSend_hpp__
#define __fon9_io_DeviceStartSend_hpp__
#include "fon9/io/SendBuffer.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace io {

inline bool IsAllowInplaceSend(DeviceOpLocker::ALocker& alocker) {
   return alocker.IsAllowInplace_     // 此時為 op safe, 才能安全的使用 dev & sbuf.
      || alocker.IsWorkingSameTask(); // 不能 op safe 的原因是有其他人正在送(但已經unlock), 此時應放入 queue.
}

/// \ingroup io
/// 實際使用請參考 DeviceStartSend();
/// 此為一次性物件, 不可重複使用.
struct StartSendChecker : public DeviceOpLocker {
   typedef SendBuffer* (*FnOpImpl_GetSendBuffer)(Device& owner, void* impl);

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
      return IsAllowInplaceSend(*this->ALocker_);
   }
   DcQueueList* ToSendingAndUnlock(SendBuffer& sbuf) {
      return sbuf.ToSendingAndUnlock(*this->ALocker_);
   }
   BufferList& GetQueueForPush(SendBuffer& sbuf) {
      return sbuf.GetQueueForPush(*this->ALocker_);
   }
   void AsyncSend(ObjHolderPtr<BufferList> pbuf, void* impl, FnOpImpl_GetSendBuffer fnGetSendBuffer) {
      this->ALocker_->AddAsyncTask(DeviceAsyncOp{[pbuf, impl, fnGetSendBuffer](Device& dev) {
         if (fnGetSendBuffer(dev, impl))
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
///   // dev 不是 State::LinkReady.
///   // 在 !sc.IsLinkReady() 時呼叫, 通常清除要求送出的 src buffer:
///   // `BufferListConsumeErr(std::move(src), std::errc::no_link);`
///   void NoLink();
///
///   SendBuffer& GetSendBufferAtLocked(DeviceT& dev);
///
///   // 在 sc.IsAllowInplace() && sc.ToSendingAndUnlock(sbuf) 的情況下呼叫.
///   // [ASAP: 立即送] or [Buffered: 啟動 Writable 偵測, 在 Writable 時送].
///   // - ASAP:
///   //   - IOCP:
///   //     - IocpSocketAddRef()
///   //     - sc.Destroy();
///   //     - 要送出的資料填入 toSend, 
///   ///    - SendAfterAddRef(): 透過 WSASend(), 然後在 OnIocp_Done() 繼續送.
///   //   - Fdr:
///   //     - 直接送出(send, write).
///   //     - 若有剩餘資料, 則放入 toSend, 然後啟動 writable 偵測.
///   //     - 若無剩餘資料, 則應檢查 SendBuffer 是否有新資料:
///   //       - 若 SendBuffer_.IsEmpty():  dev.AsyncCheckLingerClose();
///   //       - 若 !SendBuffer_.IsEmpty(): StartSendInFdrThread();
///   // - Buffered:
///   //   - 將要送出的資料填入 toSend, 然後啟動 Writable 事件.
///   Device::SendResult StartToSend(DeviceOpLocker& sc, DcQueueList& toSend);
///
///   // 使用時機, 以下 2 種情況之一:
///   // (1) 取出 ToSending 失敗: SendBuffer 不是空的, 需放到 queue, 等候先前的傳送結束時處理.
///   //     => [存入 Queue] 就結束.
///   //     => 等前一個傳送者處理.
///   // (2) 把要送出的資料填入 buf, 移到 op thread 傳送.
///   void PushTo(BufferList& buf);
///
///   // 把 pbuf 移到 op thread 傳送.
///   void AsyncSend(DeviceT& dev, StartSendChecker& sc, ObjHolderPtr<BufferList>&& pbuf);
/// };
template <class DeviceT, class Aux>
Device::SendResult DeviceStartSend(DeviceT& dev, Aux& aux) {
   StartSendChecker sc;
   if (fon9_LIKELY(sc.IsLinkReady(dev))) {
      if (fon9_LIKELY(sc.IsAllowInplace())) {
         auto&  sbuf = aux.GetSendBufferAtLocked(dev);
         if (DcQueueList* toSend = sc.ToSendingAndUnlock(sbuf))
            return aux.StartToSend(sc, *toSend);
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
/// - IocpTcpClient.cpp
/// - IocpTcpServer.cpp: IocpTcpListener::AcceptedClient
/// - FdrTcpClient.cpp
template <class DeviceBase, class IoImpl>
class DeviceImpl_DeviceStartSend : public DeviceBase {
   fon9_NON_COPY_NON_MOVE(DeviceImpl_DeviceStartSend);
   DeviceImpl_DeviceStartSend() = delete;
public:
   using DeviceBase::DeviceBase;

   Device::SendResult SendASAP(const void* src, size_t size) {
      if (size <= 0)
         return Device::SendResult{0};
      typename DeviceBase::template SendAuxImpl<typename IoImpl::SendASAP_AuxMem>   aux{src, size};
      return DeviceStartSend(*this, aux);
   }
   Device::SendResult SendASAP(BufferList&& src) {
      if (src.empty())
         return Device::SendResult{0};
      typename DeviceBase::template SendAuxImpl<typename IoImpl::SendASAP_AuxBuf>   aux{src};
      return DeviceStartSend(*this, aux);
   }
   Device::SendResult SendBuffered(const void* src, size_t size) {
      if (size <= 0)
         return Device::SendResult{0};
      typename DeviceBase::template SendAuxImpl<typename IoImpl::SendBuffered_AuxMem>   aux{src, size};
      return DeviceStartSend(*this, aux);
   }
   Device::SendResult SendBuffered(BufferList&& src) {
      if (src.empty())
         return Device::SendResult{0};
      typename DeviceBase::template SendAuxImpl<typename IoImpl::SendBuffered_AuxBuf>   aux{src};
      return DeviceStartSend(*this, aux);
   }
};

//-////////////////////////////////////////////////////////////////////////-//

struct ContinueSendChecker : public DeviceOpLocker {
   bool IsAllowContinueSend(Device& dev) {
      if (fon9_LIKELY(fon9::io::IsAllowContinueSend(dev.OpImpl_GetState()))) {
         this->ALocker_.emplace(dev.OpQueue_, AQueueTaskKind::Send);
         if (fon9_LIKELY(fon9::io::IsAllowContinueSend(dev.OpImpl_GetState())))
            return true;
         this->Destroy();
      }
      return false;
   }
};

/// \ingroup io
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
///      // 在 alocker 的保護下, 取出繼續傳送用的緩衝物件:
///      // - IOCP 使用 sbuf.OpImpl_ContinueSend(bytesTransfered_); 取出
///      // - Fdr  使用 sbuf.OpImpl_CheckSendQueue(); 取出
///      // - retval != nullptr 則表示還有要送的, 接下來: aux.ContinueToSend(&sc, retval);
///      // - retval == nullptr 表示傳送緩衝已空, 接下來: dev.AsyncCheckLingerClose(alocker);
///      DcQueueList* GetContinueToSend(SendBuffer& sbuf) const;
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
///      // => 注意: 現在還是 alocker 的狀態!
///      void DisableWritableEvent(SendBuffer& sbuf);
///
///      // - IOCP: sc->Destroy(); WSASend(iocp);
///      //   - 因為 WSASend() 返回前, 可能會在另一 thread 觸發 OnIocp_Done(); 然後重進入 DeviceContinueSend();
///      //   - 所以 WSASend() 之前, 要先 sc->Destroy(); 避免重進入的 DeviceContinueSend(); 進入 Async 程序.
///      // - Fdr:
///      //   - DeviceContinueSend() 必定是在 io thread 的 writable 事件裡面呼叫.
///      //   - 
///      void ContinueToSend(ContinueSendChecker& sc, DcQueueList& toSend) const;
///   };
/// \endcode
template <class DeviceT, class Aux>
void DeviceContinueSend(DeviceT& dev, SendBuffer& sbuf, Aux& aux) {
   ContinueSendChecker sc;
   if (fon9_UNLIKELY(!sc.IsAllowContinueSend(dev)))
      return;

   if (fon9_LIKELY(sc.GetALocker().IsAllowInplace_)) {
      if (!aux.IsSendBufferAlive(dev, sbuf))
         return;
      if (DcQueueList* toSend = aux.GetContinueToSend(sbuf))
         aux.ContinueToSend(sc, *toSend);
      else
         dev.AsyncCheckLingerClose(sc.GetALocker());
      return;
   }

   fon9_WARN_DISABLE_PADDING;
   fon9_GCC_WARN_DISABLE_NO_PUSH("-Wshadow");
   aux.DisableWritableEvent(sbuf);
   sc.GetALocker().AddAsyncTask(DeviceAsyncOp{[&sbuf, aux](Device& dev) {
      fon9_LOG_DEBUG("Async.DeviceContinueSend|dev=", ToPtr{&dev});
      if (fon9_LIKELY(IsAllowContinueSend(dev.OpImpl_GetState()))
          && fon9_LIKELY(aux.IsSendBufferAlive(dev, sbuf))) {
         ContinueSendChecker sc;
         sc.Create(dev, AQueueTaskKind::Send);
         if (DcQueueList* toSend = aux.GetContinueToSend(sbuf))
            aux.ContinueToSend(sc, *toSend);
         else
            Device::OpThr_CheckLingerClose(dev, std::string{});
      }
   }});
   fon9_WARN_POP;
}

//-////////////////////////////////////////////////////////////////////////-//

/// \ingroup io
template <class Aux>
SendDirectResult DeviceSendDirect(RecvDirectArgs& e, SendBuffer& sbuf, Aux& aux) {
   e.OpLocker_.Create(e.Device_, AQueueTaskKind::Send);
   if (e.IsRecvBufferAlive() && e.Device_.OpImpl_GetState() == State::LinkReady) {
      if (IsAllowInplaceSend(e.OpLocker_.GetALocker())) {
         if (DcQueueList* toSend = sbuf.ToSendingAndUnlock(e.OpLocker_.GetALocker())) {
            aux.StartToSend(e.OpLocker_, *toSend);
            return SendDirectResult::Sent;
         }
         aux.PushTo(sbuf.GetQueueForPush(e.OpLocker_.GetALocker()));
         return SendDirectResult::Queue;
      }
      return SendDirectResult::NeedsAsync;
   }
   return SendDirectResult::NoLink;
}

} } // namespaces
#endif//__fon9_io_DeviceStartSend_hpp__
