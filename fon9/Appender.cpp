/// \file fon9/Appender.cpp
/// \author fonwinz@gmail.com
#include "fon9/Appender.hpp"
#include "fon9/buffer/BufferNodeWaiter.hpp"
#include "fon9/buffer/FwdBufferList.hpp"

namespace fon9 {

Appender::~Appender() {
}
void Appender::MakeCallForWork(WorkContentLocker&& lk) {
   if (lk->SetToRinging())
      this->MakeCallNow(std::move(lk));
}

bool Appender::WaitNodeConsumed(WorkContentLocker&& locker, BufferList& buf) {
   if (locker->InTakingCallThread()) {
      // 到達此處的情況: 處理 ConsumeAppendBuffer(); 時 => 有控制節點要求 flush.
      return false;
   }
   CountDownLatch waiter{1};
   if (BufferNodeWaiter* node = BufferNodeWaiter::Alloc(waiter)) {
      buf.push_back(node);
      if (!this->MakeCallNow(std::move(locker)))
         return false;
      if (locker.owns_lock())
         locker.unlock();
      waiter.Wait();
      locker.lock();
      return true;
   }
   return false;
}
bool Appender::WaitFlushed(WorkContentLocker&& locker) {
   while (!locker->QueuingBuffer_.empty() || locker->IsTakingCall()) {
      if (!locker->IsTakingCall()) {
         this->Worker_.TakeCallLocked(std::move(locker));
         continue;
      }
      if (!this->WaitNodeConsumed(std::move(locker), locker->QueuingBuffer_))
         return false;
   }
   return true;
}
bool Appender::WaitFlushed() {
   return this->WaitFlushed(this->Worker_.Lock());
}
bool Appender::WaitConsumed(WorkContentLocker&& locker) {
   return this->WaitNodeConsumed(std::move(locker), locker->ConsumedWaiter_);
}

//--------------------------------------------------------------------------//

WorkerState Appender::WorkContentController::TakeCall(Locker&& lk) {
   DcQueueList& workingBuffer = lk->UnsafeWorkingBuffer_;
   workingBuffer.push_back(std::move(lk->QueuingBuffer_));
   lk->WorkingNodeCount_ = workingBuffer.GetNodeCount();
   lk.unlock();

   Appender::StaticCast(*this).ConsumeAppendBuffer(workingBuffer);

   lk.lock();
   lk->WorkingNodeCount_ = workingBuffer.GetNodeCount();
   if (fon9_UNLIKELY(!lk->ConsumedWaiter_.empty())) {
      BufferList cbuf{std::move(lk->ConsumedWaiter_)};
      lk.unlock();
      while (BufferNode* cnode = cbuf.pop_front()) {
         if (BufferNodeVirtual* vnode = BufferNodeVirtual::CastFrom(cnode))
            vnode->OnBufferConsumed();
         FreeNode(cnode);
      }
      lk.lock();
   }
   if (lk->QueuingBuffer_.empty() && lk->WorkingNodeCount_ == 0)
      return WorkerState::Sleeping;
   return WorkerState::Working;
}

} // namespace
