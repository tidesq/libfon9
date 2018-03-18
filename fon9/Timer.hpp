/// \file fon9/Timer.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_Timer_hpp__
#define __fon9_Timer_hpp__
#include "fon9/SortedVector.hpp"
#include "fon9/intrusive_ref_counter.hpp"
#include "fon9/TimeStamp.hpp"
#include "fon9/ThreadController.hpp"

namespace fon9 {

enum class TimerSeqNo : uint64_t {
   /// - 沒有在 TimerThread 裡面等候執行.
   /// - 有可能的狀態:
   ///   - 正在 TimerThread 觸發 OnTimer 事件.
   ///   - 尚未啟動 Run...() 函式.
   NoWaiting = 0,
   /// Timer 已呼叫 Dispose...() 相關函式, 無法再啟動 Run...() 相關函式.
   Disposed = 1,
   /// 已放入 TimerThread 裡面等候執行.
   WaitInLine = 2,
};
inline bool IsTimerWaitInLine(TimerSeqNo seqno) {
   static_assert(std::is_unsigned<underlying_type_t<TimerSeqNo>>::value, "underlying_type_t<TimerSeqNo> must be unsigned");
   return seqno >= TimerSeqNo::WaitInLine;
}

struct TimerEntryKey {
   TimeStamp   EmitTime_{TimeStamp::Null()};
   /// 非0表示:已啟動計時器,等候觸發.
   TimerSeqNo  SeqNo_{TimerSeqNo::NoWaiting};

   /// 把時間較小的排到 SortedVector 尾端 (先觸發, vector 移除尾端的負擔很小),
   /// 所以傳回值是 > 的結果.
   bool operator< (const TimerEntryKey& rhs) const {
      return fon9_UNLIKELY(this->EmitTime_ == rhs.EmitTime_)
         ? this->SeqNo_ > rhs.SeqNo_
         : this->EmitTime_ > rhs.EmitTime_;
   }
};

/// \ingroup Thrs
/// - 每個 TimerThread 擁有一個自己的 thread.
/// - 每個 timer 啟動時, 不論設定的是 [間隔時間] or [絕對時間], 都會使用 TimeStamp_ 來處理.
///   - 所以如果使用「間隔時間」啟動 timer, 當系統時間有變動時, 則無法在正確的「時間間隔」觸發事件!
/// - thread 的睡眠時間: 最接近的 timer.TimeStamp_ - now;
class fon9_API TimerThread;

/// \ingroup Thrs
/// 這裡提供一個 fon9 預設的 TimerThread.
/// 第一次呼叫時才會啟動. 啟動後, 程式結束時才會解構.
fon9_API TimerThread& GetDefaultTimerThread();

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4251);//C4251: '...' needs to have dll-interface to be used by clients of class '...'
/// \ingroup Thrs
/// 使用一個「額外共用的 TimerThread」提供計時功能.
/// - 在 OnTimer() 被觸發時, 至少還有一個 TimerEntrySP 保留 timer.
/// - 如果要用 data member 的方式使用 TimerEntry:
///   \code
///   class TimerUser {
///      struct Timer : public fon9::TimerEntry {
///         Timer() : TimerEntry{fon9::GetDefaultTimerThread()} {
///         }
///         virtual void OnTimerEntryReleased() override {
///            // Timer 作為 data member, 不需要 delete, 所以 OnTimerEntryReleased(): do nothing.
///         }
///         virtual void EmitOnTimer(fon9::TimeStamp now) override {
///            TimerUser* user = fon9::ContainerOf(*this, &TimerUser::Timer_);
///            ... do something ...
///            delete user;
///         }
///      };
///      Timer Timer_;
///   public:
///      ~TimerUser() {
///         // 解構時必須確定(並等候) Timer 已停止計時, 並終止其他 thread 的 StopAndWait().
///         this->Timer_.DisposeAndWait();
///      }
///   };
///   \endcode
class fon9_API TimerEntry : public intrusive_ref_counter<TimerEntry> {
   fon9_NON_COPY_NON_MOVE(TimerEntry);
   /// default: do nothing;
   virtual void OnTimer(TimeStamp now);

   /// default: OnTimer(); intrusive_ptr_release(this);
   virtual void EmitOnTimer(TimeStamp now);

   /// default: delete this;
   virtual void OnTimerEntryReleased();

   inline friend void intrusive_ptr_deleter(const TimerEntry* entry) {
      const_cast<TimerEntry*>(entry)->OnTimerEntryReleased();
   }

   friend class TimerThread;
   TimerEntryKey  Key_;

   void SetupRun(TimeStamp atTimePoint, const TimeInterval* after);
public:
   TimerThread& TimerThread_;
   TimerEntry(TimerThread& timerThread) : TimerThread_(timerThread) {
   }
   virtual ~TimerEntry();

   const TimerEntryKey& GetKey() const {
      return Key_;
   }

   /// 解構前的處置.
   /// 一旦呼叫 Dispose 就無法再進行計時.
   void DisposeNoWait();

   /// 解構前的處置.
   /// 若 TimerThread 正在觸發事件, 則等候事件處理完畢.
   void DisposeAndWait();

   /// 從計時隊列中移除, 但如果 this 已取出, 準備觸發 OnTimer(),
   /// 則有可能在返回後仍觸發 OnTimer();
   void StopNoWait();

   /// 從計時隊列中移除, 但如果 this 已取出, 準備觸發 OnTimer(),
   /// 則會等 OnTimer() 結束後才會返回 StopAndWait().
   /// 並將 EmitTime 設為 TimeStamp::Null()
   void StopAndWait();

   void RunAt(TimeStamp atTimePoint) {
      this->SetupRun(atTimePoint, nullptr);
   }

   void RunAfter(TimeInterval after) {
      this->SetupRun(TimeStamp{UtcNow() + after}, &after);
   }
};

/// \ingroup Thrs
/// 使用 data member 的方式處理 Timer.
class fon9_API DataMemberTimer : public TimerEntry {
   fon9_NON_COPY_NON_MOVE(DataMemberTimer);

   /// Timer 作為 data member, 不需要 delete, 所以 OnTimerEntryReleased(): do nothing.
   virtual void OnTimerEntryReleased() override;

   /// Timer 作為 data member, 必須採用 EmitOnTimer() 的方式觸發.
   /// 衍生者必須完成此函式.
   virtual void EmitOnTimer(TimeStamp now) override = 0;

public:
   /// 使用 GetDefaultTimerThread() 當作 timer thread.
   DataMemberTimer();
   DataMemberTimer(TimerThread& timerThread);
};

/// \ingroup Thrs
/// 使用 [data member] + [static member function] 的方式處理 Timer.
/// \code
///   class MyObject {
///      static void EmitOnTimer1(fon9::TimerEntry* timer, fon9::TimeStamp now) {
///         MyObject* pthis = fon9::ContainerOf(*static_cast<decltype(MyObject::Timer1_)*>(timer), &MyObject::Timer1_);
///         // 處理 Timer1 事件...
///      }
///      fon9::DataMemberEmitOnTimer<&MyObject::EmitOnTimer1> Timer1_;
///   };
/// \endcode
template <void (*pEmitOnTimer)(TimerEntry*, TimeStamp now)>
class DataMemberEmitOnTimer : public DataMemberTimer {
   fon9_NON_COPY_NON_MOVE(DataMemberEmitOnTimer);
   using base = DataMemberTimer;
   virtual void EmitOnTimer(TimeStamp now) override {
      pEmitOnTimer(this, now);
   }
public:
   using base::base;
};
fon9_MSC_WARN_POP;

using TimerEntrySP = intrusive_ptr<TimerEntry>;

/// \ingroup Thrs
/// 如果「時間到」的時候「使用 weak_ptr 參考的 owner」依然存活,
/// 才會觸發 owner->OnTimer(this) 事件, TimerEntry_OwnerWP 寫法範例:
/// \code
/// class MySession : public std::enable_shared_from_this<MySession> {
///    fon9_NON_COPY_NON_MOVE(MySession);
///
///    void OnTimer(fon9::TimerEntry* timer, fon9::TimeStamp now) {
///       ... 計時時間到 ...
///       timer->RunAfter(fon9::TimeInterval_Second(1)); // 如果有需要計時, 則需要再啟動一次.
///    }
///    using FlowTimer = fon9::TimerEntry_OwnerWP<MySession, &MySession::OnTimer>;
///    fon9::TimerEntrySP FlowTimer_;
///
///    MySession() = default;
/// public:
///    ~MySession() {
///       // 解構時不須針對 FlowTimer_ 做額外處置:
///       // - 進入解構程序時, FlowTimer_->Owner_ (weak_ptr) lock 會失敗, 所以不可能進入 MySession::OnTimer()
///       // - 當最後一個 EntrySP 死亡時, FlowTimer_ 就會自然死亡了!
///    }
///    static std::shared_ptr<MySession> Make(fon9::TimerThread& timerThread) {
///       std::shared_ptr<MySession> res{new MySession{}};
///       res->FlowTimer_.reset(new FlowTimer{timerThread,res}); // 建立計時器.
///       res->FlowTimer_->RunAfter(fon9::TimeInterval_Second(1)); // 啟動計時器.
///       return res;
///    }
/// };
/// \endcode
template <class OwnerT, void (OwnerT::*pOnTimer)(TimerEntry*, TimeStamp now)>
class TimerEntry_OwnerWP : public TimerEntry {
   fon9_NON_COPY_NON_MOVE(TimerEntry_OwnerWP);
   std::weak_ptr<OwnerT> Owner_;
protected:
   virtual void OnTimer(TimeStamp now) override {
      if (std::shared_ptr<OwnerT> owner = this->Owner_.lock())
         (owner.get()->*pOnTimer)(this, now);
   }
public:
   TimerEntry_OwnerWP(TimerThread& timerThread, std::weak_ptr<OwnerT> owner)
      : TimerEntry{timerThread}
      , Owner_{std::move(owner)} {
   }
};

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4251);// C4251: '...' needs to have dll-interface to be used by clients of class '...'
/// \ingroup Thrs
/// 實際的 TimerEntry 放在 TimerThread 裡面執行.
class fon9_API TimerThread {
   fon9_NON_COPY_NON_MOVE(TimerThread);
   friend class TimerEntry;

   struct TimerThreadData {
      void Erase(TimerEntryKey& key) {
         if (key.SeqNo_ == TimerSeqNo::NoWaiting)
            return;
         auto ifind = this->Timers_.find(key);
         if (ifind != this->Timers_.end())
            this->Timers_.erase(ifind);
      }

      using Timers = SortedVector<TimerEntryKey, TimerEntrySP>;
      TimerSeqNo        LastSeqNo_{TimerSeqNo::WaitInLine};
      Timers            Timers_;
      TimeInterval      CvWaitSecs_;
      /// 如果在 TimerThread 正在觸發, 則會設定此值.
      /// 讓另一 thread 呼叫 TimerEntry::StopAndWait() 時, 可以等到 OnTimer() 真的結束後才返回.
      TimerEntry* CurrEntry_{};
   };
   using TimerController = ThreadController<TimerThreadData, WaitPolicy_CV>;
   using Locker = TimerController::Locker;
   TimerController   TimerController_;
   std::thread       Thread_;

   bool CheckCurrEmit(Locker& timerThread, TimerEntry& timer);

protected:
   void ThrRun(std::string timerName);
   /// Terminate() & Wait thread join.
   void WaitTerminate();
   void Terminate() {
      this->TimerController_.Terminate();
   }

public:
   TimerThread(std::string timerName);
   ~TimerThread();

   bool InThisThread() const {
      return (this->Thread_.get_id() == std::this_thread::get_id());
   }
};
fon9_MSC_WARN_POP;

} // namespace fon9
#endif//__fon9_Timer_hpp__
