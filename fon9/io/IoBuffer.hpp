/// \file fon9/io/IoBuffer.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_IoBuffer_hpp__
#define __fon9_io_IoBuffer_hpp__
#include "fon9/io/SendBuffer.hpp"
#include "fon9/io/RecvBuffer.hpp"

namespace fon9 { namespace io {

fon9_MSC_WARN_DISABLE(4265);/* class has virtual functions, but destructor is not virtual */
struct SendChecker;

/// \ingroup io
/// - 通常與 socket、fd 結合: e.g. FdrSocket、IocpSocket.
/// - 且 socket、fd 會是 const: 在解構時才關閉.
/// - 一旦進入關閉程序, 即使 SendBuffer_ 仍有資料, 也不會再送出.
/// - 若有需要等候送完, 應使用 (Device::LingerClose + 系統的 linger) 機制.
class IoBuffer {
   fon9_NON_COPY_NON_MOVE(IoBuffer);
protected:
   friend struct SendChecker;
   SendBuffer  SendBuffer_;
   RecvBuffer  RecvBuffer_;

   ~IoBuffer() {}
public:
   IoBuffer() = default;

   bool IsSendBufferEmpty() const {
      return this->SendBuffer_.IsEmpty();
   }

   typedef bool (*FnOpImpl_IsBufferAlive)(Device& owner, IoBuffer* impl);

   virtual void StartRecv(RecvBufferSize preallocSize) = 0;
   void ContinueRecv(RecvBufferSize expectSize) {
      this->RecvBuffer_.SetContinueRecv();
      this->StartRecv(expectSize);
   }
   void OnRecvBufferReady(Device& dev, DcQueueList& rxbuf, FnOpImpl_IsBufferAlive fnIsBufferAlive);

   /// 您必須自行確保一次只能有一個 thread 呼叫 ContinueSend()
   /// - io thread: at Writable event.
   /// - op thread: at Async send request.
   /// \code
   ///   // 必須是 copyable, 因為可能會到 alocker.AddAsyncTask() 裡面使用.
   ///   struct Aux {
   ///      // 若 iobuf 已不屬於 dev, 則放棄傳送.
   ///      // 若有需要等候送完, 應使用 (Device::LingerClose + 系統的 linger) 機制.
   ///      bool IsBufferAlive(Device& dev, IoBuffer* iobuf);
   ///
   ///      // 在 alocker 的保護下, 取出 toSend, 然後送出, 送出時不用 alocker 保護:
   ///      // => 因為 SendBuffer_ 已進入 Sending 狀態.
   ///      // => 其他傳送要求在 SendBuffer_.ToSendingAndUnlock(alocker) 會被阻擋
   ///      //    => 然後該次要求會放到 SendBuffer_.Queue_ 裡面.
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
   ///      void DisableWritableEvent(IoBuffer& iobuf);
   ///
   ///      // 在安全可立即送出的情況下, 會呼叫此處傳送.
   ///      // - 從 SendBuffer_ 安全的取出 toSend, 且 alocker 已解構.
   ///      //   由於 this->ContinueSend() 返回前 this 必然不會死亡,
   ///      //   所以可以在沒 alocker 且沒保護的情況下呼叫 StartSend(*this);
   ///      // - 或在 op thread 確定 iobuf 仍然存活時.
   ///      void StartSend(Device& dev, IoBuffer& iobuf, DcQueueList& toSend) const;
   ///   };
   /// \endcode
   template <class Aux>
   void ContinueSend(Device& dev, Aux& aux);
};
fon9_MSC_WARN_POP;

} } // namespaces
#endif//__fon9_io_IoBuffer_hpp__
