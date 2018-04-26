/// \file fon9/Appender.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_Appender_hpp__
#define __fon9_Appender_hpp__
#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/Worker.hpp"
#include "fon9/SpinMutex.hpp"
#include "fon9/ThreadId.hpp"

namespace fon9 {

fon9_WARN_DISABLE_PADDING;
/// \ingroup Misc
/// - 可以同時多個 thread 呼叫 `Append()` 節點資料不會交錯.
/// - 衍生者可改寫:
///   - `virtual void MakeCallForWork(WorkContentLocker& lk);`
///     - 預設: `if (lk->SetToRinging()) this->MakeCallNow(lk);`
/// - 衍生者需實現:
///   - `virtual bool MakeCallNow(WorkContentLocker& lk) = 0;`
///   - `virtual void ConsumeAppendBuffer(DcQueueList& buffer) = 0;`
/// - 衍生者範例: `FileAppender`, `LogFile`, `io::SendBuffer`
class fon9_API Appender {
   fon9_NON_COPY_NON_MOVE(Appender);

protected:
   struct WorkContentController;
   class WorkContent : public WorkContentBase {
      fon9_NON_COPY_NON_MOVE(WorkContent);
      friend struct WorkContentController;
      friend class Appender;
      BufferList        QueuingBuffer_;
      // UnsafeWorkingBuffer_ 僅允許在 Appender::WorkContentController::TakeCall() 裡面 unlock 的狀態下使用.
      DcQueueList       UnsafeWorkingBuffer_;
      ThreadId::IdType  WorkingThreadId_{0};
      size_t            WorkingNodeCount_{0};
      // 在 ConsumeAppendBuffer() 之後的通知.
      BufferList        ConsumedWaiter_;
   public:
      WorkContent() = default;
      size_t GetQueuingNodeCount() const {
         return this->QueuingBuffer_.size();
      }
      size_t GetTotalNodeCount() const {
         return this->QueuingBuffer_.size() + this->WorkingNodeCount_;
      }
   };

   //using Mutex = std::mutex;
   //using Mutex = SpinBusy;
   using Mutex = SpinMutex<YieldSleepPolicy>;

   struct WorkContentController : public MustLock<WorkContent, Mutex> {
      fon9_NON_COPY_NON_MOVE(WorkContentController);
      WorkContentController() = default;

      void Dispose(Locker& lk) {
         if (lk->SetToDisposing())
            Appender::StaticCast(*this).MakeCallNow(lk);
      }
      void MakeCallForWork(Locker& lk) {
         Appender::StaticCast(*this).MakeCallForWork(lk);
      }
      void AddWork(Locker& lk, BufferNode* vnode) {
         assert(vnode != nullptr);
         lk->QueuingBuffer_.push_back(vnode);
         this->MakeCallForWork(lk);
      }
      void AddWork(Locker& lk, BufferList&& buf) {
         lk->QueuingBuffer_.push_back(std::move(buf));
         this->MakeCallForWork(lk);
      }
      void AddWork(Locker& lk, const void* buf, size_t size) {
         AppendToBuffer(lk->QueuingBuffer_, buf, size);
         this->MakeCallForWork(lk);
      }
      WorkerState TakeCall(Locker& lk);
   };
   using Worker = fon9::Worker<WorkContentController>;
   using WorkContentLocker = Worker::ContentLocker;
   Worker   Worker_;

   Appender() = default;
   virtual ~Appender();

   static Appender& StaticCast(WorkContentController& ctrl) {
      return ContainerOf(Worker::StaticCast(ctrl), &Appender::Worker_);
   }

   /// 在 Append() 把資料放入 buffer 之後, 返回前呼叫此處.
   /// 預設: if (lk->SetToRinging()) this->MakeCallNow(lk);
   /// 您可以覆寫, 並自行決定: 是否要呼叫、何時要呼叫 MakeCallNow();
   virtual void MakeCallForWork(WorkContentLocker& lk);

   /// (1) 非同步模式: add Worker to ThreadPool(after lk.unlock());
   /// (2) 同步模式:   立即呼叫 Worker::TakeCall(lk);
   /// 返回 false 表示 Appender 要下班了, 無法提供服務.
   virtual bool MakeCallNow(WorkContentLocker& lk) = 0;

   /// 由衍生者實現 Append(buffer);
   virtual void ConsumeAppendBuffer(DcQueueList& buffer) = 0;

   /// \retval true  必定已將資料寫完, 且 locker 仍在 lock() 狀態.
   /// \retval false 無法執行: 可能正在解構? this thread in Working thread?
   bool WaitFlushed(WorkContentLocker& locker);

   /// \retval true
   ///   強制呼叫 MakeCallNow(); 並在 ConsumeAppendBuffer(); 之後返回.
   ///   此時 lk 必定為 lock 狀態.
   /// \retval false 底下情況會立即返回:
   ///   - this thread is in ConsumeAppendBuffer();
   ///   - MakeCallNow() == false;
   bool WaitConsumed(WorkContentLocker& locker);

   bool WaitNodeConsumed(WorkContentLocker& locker, BufferList& buf);

public:
   void Append(BufferList&& outbuf) {
      this->Worker_.AddWork(std::move(outbuf));
   }
   void Append(const void* buf, size_t size) {
      this->Worker_.AddWork(buf, size);
   }
   void Append(const StrView& str) {
      this->Append(str.begin(), str.size());
   }
   void Append(BufferNode* vnode) {
      this->Worker_.AddWork(vnode);
   }

   /// 返回前, 必定已將呼叫前的資料寫完.
   /// 但無法保證: 其他 thread 的 append 要求, 在呼叫端接手前, 能全部寫完.
   bool WaitFlushed();
};
fon9_WARN_POP;

} // namespaces
#endif//__fon9_Appender_hpp__
