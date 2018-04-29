/// \file fon9/io/SendBuffer.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_SendBuffer_hpp__
#define __fon9_io_SendBuffer_hpp__
#include "fon9/io/Device.hpp"
#include "fon9/buffer/DcQueueList.hpp"

namespace fon9 { namespace io {

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// 傳送緩衝區: 如果情況允許, 則提供 **立即可送** 的返回值, 由呼叫端負責立即送出。
/// - ASAP
///   - 提供一般消費函式 send(), write()... 使用的介面:
///     - `LockForASAP(const void* src, size_t size)` => `AfterASAP(const void* srcRemain, size_t sizeRemain)`
///     - `LockForASAP(BufferList&& src, DcQueueList*& outbuf)` => `AfterConsumed(DcQueueList* outbuf)`
///   - 提供可讓 Overlapped I/O 可用的介面:
///     - `PushSendBlockVector()` => `ContSendBlockVector（)`
/// - Buffered:
///   - `PushSend()` => in Send thread `DcQueueList* LockForConsume()` => `AfterConsumed(DcQueueList* outbuf)`
class fon9_API SendBuffer {
   fon9_NON_COPY_NON_MOVE(SendBuffer);

   DcQueueList Buffer_;

public:
   SendBuffer() = default;

   /// 只能在「Op thread 裡面」執行!
   void OpThr_ClearBuffer(const ErrC& errc) {
      this->Buffer_.ConsumeErr(errc);
   }

   enum class LockResult {
      /// 無法傳送: 未連線(`SendBufferState::NoLink`) or 連線準備中(`SendBufferState::Preparing`).
      NotReady,
      /// 允許立即送出, 返回後 **呼叫端有責任** 立即送出, 傳送完畢後需呼叫 `this->AfterASAP()`.
      /// 此時若其他 thread 有傳送需求, 則會放入排隊, 等到 `this->AfterASAP()` 處理.
      AllowASAP,
      /// 已將資料放入排隊.
      Queuing,
   };
   class ALocker : public DeviceOpQueue::ALocker {
      fon9_NON_COPY_NON_MOVE(ALocker);
      using base = DeviceOpQueue::ALocker;
   public:
      const LockResult  LockResult_;
      ALocker(DeviceOpQueue& opQueue, SendBuffer& sbuf)
         : base{opQueue, AQueueTaskKind::Send}
         , LockResult_{DeviceState_ != State::LinkReady ? LockResult::NotReady
                     : (IsAllowInvoke_ && sbuf.Buffer_.empty()) ? LockResult::AllowASAP
                     : LockResult::Queuing} {
      }

      /// \code
      ///   ALocker aSender{..., src, size};
      ///   if (aSender.LockResult_ == LockResult::AllowASAP) {
      ///      int wsz = send(so, src, size);
      ///      (1) 失敗: device link broken.
      ///      (2) 全部送完: 若 !sbuf.Buffer_.empty() (在 send() 返回前, 其他 thread 的傳送要求) => 啟動 Writable 偵測.
      ///      (3) 送出部分: aSender.SetRemain(src + wsz, size - wsz); => 放在 sbuf.Buffer_ 的前端!!
      ///          啟動 Writable 偵測.
      ///   }
      /// \endcode
      ALocker(DeviceOpQueue& opQueue, SendBuffer& sbuf, const void* src, size_t size)
         : ALocker{opQueue, sbuf} {
         if (this->LockResult_ == LockResult::Queuing)
            sbuf.Buffer_.Append(src, size);
      }
   };

   /// \retval LockResult::AllowASAP **呼叫端有責任** 立即送出 src,
   ///                   並將結果透過 `this->AfterASAP(const void* srcRemain, size_t sizeRemain);` 告知, 及處理後續應變.
   /// \retval 其他 呼叫端自行決定如何處理.
   LockResult LockForASAP(DeviceOpQueue& opQueue, const void* src, size_t size);

#if 0
   enum class AfterSent {
      /// 已無資料需要傳送.
      BufferEmpty,
      /// 上次傳送已送完(LockForConsume()之後,有全部處理完畢), 且有新資料等候傳送.
      /// **呼叫端有責任** 啟動後續的傳送作業.
      NewArrive,
      /// 上次傳送沒送完(LockForConsume()之後,沒有全部處理完).
      /// **呼叫端有責任** 啟動後續的傳送作業.
      HasRemain,
   };
   AfterSent AfterASAP(const void* srcRemain, size_t sizeRemain);

   /// \retval LockResult::AllowASAP   src 會移入 outbuf, **呼叫端有責任** 立即用來傳送, 傳送完畢後需呼叫 `this->AfterConsumed(outbuf)` 解鎖.
   /// \retval LockResult::NotReady    未連線, src 被取消: `src.ConsumeErr(std::errc::no_link)`.
   /// \retval LockResult::Queuing     已將資料放入排隊, 但無法立即送出.
   LockResult LockForASAP(BufferList&& src, DcQueueList*& outbuf);


   /// \ref LockForConsume()
   AfterSent AfterConsumed(DcQueueList* outbuf);

   /// 提供給 device 取得要送出的資料.
   /// \retval ==nullptr Buffer為空 or 有其他地方正在傳送.
   /// \retval !=nullptr 需要傳送的資料.
   ///      - **呼叫端有責任** 立即處理 retval.
   ///      - 資料送完後, **呼叫端有責任** 再呼叫 `retval->ConsumeErr()` 或 `retval->PopConsumed()` 移除用掉的資料.
   ///        或透過 `this->ContSendBlockVector()` 繼續送出排隊中的資料.
   ///      - **呼叫端有責任** 在處理完畢後呼叫 `this->AfterConsumed(retval)` 解鎖;
   ///      - 在呼叫 `this->AfterConsumed(retval)` 之前, 都無法再取得待送資料.
   ///      - `LockForConsume()` 與 `AfterConsumed()` 不一定要在同一個 thread!
   DcQueueList* LockForConsume();
   
   enum class PushResult {
      /// 未連線,無法傳送.
      NotReady,
      /// 首次將資料放入 buffer.
      New,
      /// 正在傳送中, 放入的資料排在尾端.
      Queuing,
   };
   /// \retval PushResult::New      首次將資料放入 buffer, **呼叫端有責任** 啟動傳輸作業.
   /// \retval PushResult::NotReady 未連線, src 被取消: `src.ConsumeErr(std::errc::no_link)`.
   /// \retval PushResult::Queuing  正在傳送中, 要求的資料排在尾端.
   PushResult PushSend(BufferList&& src) {
      Locker buf{this->Buffer_};
      return this->PushSend(buf, std::move(src));
   }
   PushResult PushSend(const void* src, size_t size) {
      Locker buf{this->Buffer_};
      return this->PushSend(buf, src, size);
   }

   /// 將資料加入緩衝, 取出可立即消費(送出)的資料量.
   /// 將 src 放入 buffer, 若可以立即送出, 則從 buffer 取得可送出的區塊放入 vect, 並傳回區塊數量.
   /// 通常用於 Windows 的 Overlapped I/O.
   /// \retval >0  **呼叫端有責任** 立即送出 vect, 傳送完畢後再透過 `ContSendBlockVector()` 取出後續資料.
   /// \retval =0  排隊中.
   /// \retval <0  未連線, src 被取消: `src.ConsumeErr(std::errc::no_link)`.
   template <typename T, size_t maxCount>
   int PushSendBlockVector(BufferList&& src, T (&vect)[maxCount]) {
      Locker buf{*this};
      return this->CheckPushSendBlockVector(buf, this->PushSend(buf, std::move(src)), vect);
   }
   template <typename T, size_t maxCount>
   int PushSendBlockVector(const void* src, size_t size, T (&vect)[maxCount]) {
      Locker buf{*this};
      return this->CheckPushSendBlockVector(buf, this->PushSend(buf, src, size), vect);
   }

   /// 移除已送出的資料, 並取得接下來要傳送的資料.
   /// \retval >=0 返回等候傳送的資料區塊數量, **呼叫端有責任** 立即送出 vect.
   /// \retval <0  no link.
   template <typename T, size_t maxCount>
   int ContSendBlockVector(size_t sentBytes, T (&vect)[maxCount]) {
      Locker buf{*this};
      assert(buf->Status_ == BufferStatus::Sending);
      if (sentBytes > 0)
         buf->SendingBuffer_.PopConsumed(sentBytes);
      if (DcQueueList* outbuf = this->GetBufferForSend(buf))
         if (size_t vcount = outbuf->PeekBlockVector(vect))
            return static_cast<int>(vcount);
      buf->Status_ = BufferStatus::LinkReady;
      return 0;
   }

private:
   template <typename T, size_t maxCount>
   static int CheckPushSendBlockVector(Locker& buf, PushResult res, T (&vect)[maxCount]) {
      switch (res) {
      case PushResult::NotReady:
         return -1;
      case PushResult::New:
         if (size_t vcount = buf->SendingBuffer_.PeekBlockVector(vect)) {
            buf->Status_ = BufferStatus::Sending;
            return static_cast<int>(vcount);
         }
         break;
      case PushResult::Queuing:
         assert(buf->Status_ == BufferStatus::Sending);
         break;
      }
      return 0;
   }

   void ClearBuffer(const ErrC& errc, BufferStatus st);
   static LockResult LockForASAP(Locker& buf);
   static DcQueueList* GetBufferForSend(Locker& buf);
   static PushResult PushSend(Locker& buf, BufferList&& src);
   static PushResult PushSend(Locker& buf, const void* src, size_t size);
#endif
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_SendBuffer_hpp__
