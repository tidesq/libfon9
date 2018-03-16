/// \file fon9/WaitPolicy.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_WaitPolicy_hpp__
#define __fon9_WaitPolicy_hpp__
#include "fon9/SpinMutex.hpp"
#include <mutex>
#include <condition_variable>

namespace fon9 {

template <class MutexT = std::mutex>
struct WaitPolicy_NoWait {
   using Mutex = MutexT;
   using Locker = std::unique_lock<Mutex>;
};

/// \ingroup Thrs
/// 使用標準的 std::mutex, std::condition_variable 處理等候及喚醒.
class WaitPolicy_CV {
   fon9_NON_COPY_NON_MOVE(WaitPolicy_CV);
   std::condition_variable CV_;
public:
   using Mutex = std::mutex;
   using Locker = std::unique_lock<Mutex>;

   WaitPolicy_CV() = default;

   void Wait(Locker& locker) {
      this->CV_.wait(locker);
   }

   template<class Duration>
   void WaitFor(Locker& locker, const Duration& dur) {
      this->CV_.wait_for(locker, dur);
   }

   void NotifyAll(const Locker&) {
      this->CV_.notify_all();
   }

   void NotifyOne(const Locker&) {
      this->CV_.notify_one();
   }
};

/// \ingroup Thrs
/// 使用 spin 機制處理等候及喚醒.
/// MutexT 必須提供:
/// - MutexT::Wait();
template <class MutexT>
class WaitPolicy_Spin {
   fon9_NON_COPY_NON_MOVE(WaitPolicy_Spin);
   using NotifyIdType = size_t;
   // 雖然在 Notify 時有 lock, 但在 Wait loop 裡面是 unlock 狀態, 所以需要使用 atomic.
   // NotifyCount_ 重複的可能性:
   // - 假設每 1ns 呼叫一次 Notify, size_t=uint64_t
   // - 大約需要 18446744073 秒(約584年), 才會重複!
   std::atomic<NotifyIdType>  NotifyCount_{1};
public:
   WaitPolicy_Spin() = default;

   using Mutex = MutexT;
   using LockerBase = std::unique_lock<Mutex>;
   class Locker : public LockerBase {
      fon9_NON_COPYABLE(Locker);
      using base = LockerBase;
      NotifyIdType   NotifyId_{0};
      friend class WaitPolicy_Spin;
   public:
      Locker(Mutex& mx) : base{mx} {
      }
      Locker(Locker&& rhs) : base{std::move(rhs)}, NotifyId_{rhs.NotifyId_} {
         rhs.NotifyId_ = 0;
      }
      Locker& operator=(Locker&& rhs) {
         base::operator=(std::move(rhs));
         this->NotifyId_ = rhs.NotifyId_;
         rhs.NotifyId_ = 0;
      }
   };

   void Wait(Locker& locker) {
      assert(locker.owns_lock());
      auto currId = this->NotifyCount_.load(std::memory_order_relaxed);
      if (locker.NotifyId_ == currId) {
         locker.unlock();
         while (locker.NotifyId_ == (currId = this->NotifyCount_.load(std::memory_order_relaxed)))
            locker.mutex()->Wait();
         locker.lock();
      }
      locker.NotifyId_ = currId;
   }

   template<class Duration>
   void WaitFor(Locker& locker, const Duration&) {
      this->Wait(locker);
   }

   void NotifyAll(const LockerBase&) {
      if (this->NotifyCount_.fetch_add(1, std::memory_order_relaxed) + 1 == 0)
         this->NotifyCount_.store(1, std::memory_order_relaxed);
   }

   void NotifyOne(const LockerBase& locker) {
      this->NotifyAll(locker);
   }
};

using WaitPolicy_SpinBusy = WaitPolicy_Spin<SpinBusy>;

} // namespaces
#endif//__fon9_WaitPolicy_hpp__
