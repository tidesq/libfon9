/// \file fon9/CountDownLatch.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_CountDownLatch_hpp__
#define __fon9_CountDownLatch_hpp__
#include "fon9/WaitPolicy.hpp"

namespace fon9 {

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4251);
/// \ingroup Thrs
/// 等候指定數量的倒數.
/// 一旦倒數到 0, 就會叫醒所有的等候者, 且以後的等候都會立即返回.
class fon9_API CountDownLatch {
   fon9_NON_COPY_NON_MOVE(CountDownLatch);
   using Waiter = WaitPolicy_CV;
   using Mutex = Waiter::Mutex;
   using Locker = typename Waiter::Locker;
   Mutex    Mutex_;
   Waiter   Waiter_;
   signed   Counter_;
public:
   /// 建構時指定要倒數的次數.
   inline explicit CountDownLatch(unsigned count) : Counter_(static_cast<signed>(count)) {
   }
   /// 等候倒數到0, 才會返回.
   /// 這裡不會變動計數器.
   void Wait();

   /// 倒數一次, 倒數到0時, 觸發喚醒全部等候者.
   void CountDown();

   /// 強迫倒數結束.
   /// 返回強制中斷前剩餘倒數次數.
   int ForceWakeUp();
};
fon9_WARN_POP;

} // namespaces
#endif//__fon9_CountDownLatch_hpp__
