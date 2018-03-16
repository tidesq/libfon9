/// \file fon9/CyclicBarrier.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_CyclicBarrier_hpp__
#define __fon9_CyclicBarrier_hpp__
#include "fon9/WaitPolicy.hpp"

namespace fon9 {

fon9_MSC_WARN_DISABLE(4251);
/// \ingroup Thrs
/// 當同時等候中的 Threads 到達指定數量時, 則喚醒所有等候者.
/// 喚醒等候者之後, 計數器會歸0, 重複等候邏輯.
class fon9_API CyclicBarrier {
   fon9_NON_COPY_NON_MOVE(CyclicBarrier);
   using Waiter = WaitPolicy_CV;
   using Mutex = Waiter::Mutex;
   using Locker = typename Waiter::Locker;
   Mutex    Mutex_;
   Waiter   Latch_;
   unsigned WaittingCount_;
public:
   /// 期待等候的數量.
   const unsigned ExpectedCount_;
   explicit CyclicBarrier(unsigned expectedCount) : WaittingCount_(0), ExpectedCount_(expectedCount) {
   }
   /// 當同時等候的Threads到達指定數量時返回.
   void Wait();
};
fon9_MSC_WARN_POP;

} // namespaces
#endif//__fon9_CyclicBarrier_hpp__
