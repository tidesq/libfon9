/// \file fon9/SpinMutex.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_SpinMutex_hpp__
#define __fon9_SpinMutex_hpp__
#include "fon9/sys/Config.hpp"
#include "fon9/SleepPolicy.hpp"
#include <atomic>

namespace fon9 {

/// \ingroup Thrs
/// 使用 spin 機制模擬 mutex.
/// 您必須提供 SleepPolicy::Sleep() 在鎖不住時, 睡一會兒.
/// \tparam SleepPolicy 參考 fon9/SleepPolicy.hpp: BusySleepPolicy, YieldSleepPolicy, NanoSleepPolicy...
template <class SleepPolicy>
class SpinMutex : private SleepPolicy {
   fon9_NON_COPYABLE(SpinMutex);
   std::atomic_flag  Mx_ = ATOMIC_FLAG_INIT;
public:
   using SleepPolicy::SleepPolicy;
   SpinMutex() : SleepPolicy{} {
   }

   /// 當鎖不住時, 利用 SleepPolicy::Sleep() 睡一會兒, 然後重新嘗試再鎖一次, 直到鎖成功才返回.
   void lock() {
      while (!try_lock())
         this->Sleep();
   }

   /// 嘗試鎖住, 立即即返回: 鎖成功傳回 true, 無法鎖住傳回 false.
   bool try_lock() {
      return !this->Mx_.test_and_set(std::memory_order_acquire);
   }

   /// 解鎖, 不檢查現在鎖的狀態, 所以您必須自行確定: 之前有鎖成功.
   void unlock() {
      this->Mx_.clear(std::memory_order_release);
      this->WakeUp();
   }

   void Wait() {
      this->Sleep();
   }
};

using SpinBusy = SpinMutex<BusySleepPolicy>;

} // namespaces
#endif//__fon9_SpinMutex_hpp__
