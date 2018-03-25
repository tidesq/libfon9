/// \file fon9/buffer/BufferNodeWaiter.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_buffer_BufferNodeWaiter_hpp__
#define __fon9_buffer_BufferNodeWaiter_hpp__
#include "fon9/buffer/BufferNode.hpp"
#include "fon9/CountDownLatch.hpp"

namespace fon9 {

/// \ingroup Buffer
/// 在 Buffer 裡面當此 node 被處理後, 呼叫 ThreadWaiter::Done().
/// 可用在等候資料同步.
class fon9_API BufferNodeWaiter : public BufferNodeVirtual {
   fon9_NON_COPY_NON_MOVE(BufferNodeWaiter);
   using base = BufferNodeVirtual;
   CountDownLatch* Waiter_{nullptr};
   friend class BufferNode;// for BufferNode::Alloc();
   using base::base;
protected:
   virtual ~BufferNodeWaiter();
   virtual void OnBufferConsumed() override;
   virtual void OnBufferConsumedErr(const ErrC& errc) override;
public:
   static BufferNodeWaiter* Alloc(CountDownLatch& waiter);

   /// 等候 list 裡面, 目前的 node 被消費完畢為止.
   /// 返回時, lockedList.empty() 必定為 true, 且仍在 lock() 狀態.
   template <class LockedBufferList>
   static void WaitFlushed(LockedBufferList& lockedList) {
      while (!lockedList->empty()) {
         CountDownLatch waiter{1};
         if (BufferNodeWaiter* node = BufferNodeWaiter::Alloc(waiter)) {
            lockedList->push_back(node);
            lockedList.unlock();
            waiter.Wait();
            lockedList.lock();
         }
      }
   }
};

} // namespaces
#endif//__fon9_buffer_BufferNodeWaiter_hpp__
