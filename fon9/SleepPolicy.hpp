/// \file fon9/SleepPolicy.hpp
/// \brief 提供各種 [睡一會兒] 的策略
/// \author fonwinz@gmail.com
#ifndef __fon9_SleepPolicy_hpp__
#define __fon9_SleepPolicy_hpp__
#include "fon9/sys/Config.hpp"

fon9_BEFORE_INCLUDE_STD;
#include <thread>
fon9_AFTER_INCLUDE_STD;

namespace fon9 {

/// \ingroup Thrs
/// 不睡覺,立即返回.
class BusySleepPolicy {
public:
   /// 不睡, 立即返回.
   static void Sleep() {
   }
   /// 因為不睡, 所以不用喚醒: do nothing.
   static void WakeUp() {
   }
};

/// \ingroup Thrs
/// 用 std::this_thread::yield() 睡覺.
class YieldSleepPolicy {
public:
   /// 利用 std::this_thread::yield() 睡覺, 然後立即返回.
   static void Sleep() {
      std::this_thread::yield();
   }
   /// 等候 std::this_thread::yield() 返回自然會醒, 所以這裡: do nothing.
   static void WakeUp() {
   }
};

/// \ingroup Thrs
/// 用 std::this_thread::sleep_for() 睡覺.
/// \tparam  ns  沉睡時間(10^-9 秒)
template <unsigned ns>
class NanoSleepPolicy {
public:
   /// 沉睡 ns (10^-9 秒), 然後返回, 睡醒前無法中斷.
   static void Sleep() {
      std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
   }
   /// 等時間到自然會醒, 沒有喚醒功能: do nothing.
   static void WakeUp() {
   }
};

} // namespaces
#endif//__fon9_SleepPolicy_hpp__
