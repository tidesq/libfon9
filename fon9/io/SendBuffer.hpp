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
/// - 所有的操作只能在 OpQueue safe 的狀態下進行.
/// - 提供可讓 Overlapped I/O 可用的介面:
///   - `ToSendingAndUnlock()` => `ContinueSend（)`
///   - if (!SendBuffer_.IsEmpty()) 將資料填入 `GetQueueForPush()`
/// - 提供一般消費函式 send(), write()... 使用的介面:
///   - ASAP
///   - Buffered
class SendBuffer {
   fon9_NON_COPY_NON_MOVE(SendBuffer);

   enum class Status {
      Empty,
      Sending,
      ErrorClearAfterSent,
   };
   Status      Status_{Status::Empty};
   BufferList  Queue_;
   DcQueueList Sending_;

public:
   SendBuffer() = default;

   bool IsEmpty() const {
      return this->Status_ == Status::Empty;
   }
   void OpThr_ClearBuffer(const ErrC& errc) {
      if (this->Status_ == Status::Sending)
         this->Status_ = Status::ErrorClearAfterSent;
      else
         this->ForceClearBuffer(errc);
   }
   void ForceClearBuffer(const ErrC& errc) {
      this->Sending_.push_back(std::move(this->Queue_));
      this->Sending_.ConsumeErr(errc);
      this->Status_ = Status::Empty;
   }

   using ALocker = DeviceOpQueue::ALockerForInvoke;

   /// 設定進入傳送中的狀態.
   /// \retval nullptr  現在正在傳送中, 設定失敗.
   /// \retval !nullptr 現在沒有在傳送, 成功進入 Sending 狀態:
   ///                  - 傳回值可用於填入資料後立即傳送.
   ///                  - 返回前會先執行 alocker.UnlockForInvoke();
   DcQueueList* ToSendingAndUnlock(ALocker& alocker) {
      if (!alocker.IsAllowInvoke_ || this->Status_ != Status::Empty)
         return nullptr;
      this->Status_ = Status::Sending;
      alocker.UnlockForInvoke();
      return &this->Sending_;
   }
   DcQueueList* ContinueSend(size_t bytesSent) {
      assert(this->Status_ == Status::Sending);
      this->Sending_.PopConsumed(bytesSent);
      this->Sending_.push_back(std::move(this->Queue_));
      if (!this->Sending_.empty())
         return &this->Sending_;
      this->Status_ = Status::Empty;
      return nullptr;
   }

   BufferList& GetQueueForPush(ALocker& alocker) {
      (void)alocker;
      assert(this->Status_ >= Status::Sending && alocker.owns_lock());
      return this->Queue_;
   }
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_SendBuffer_hpp__
