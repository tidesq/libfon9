/// \file fon9/Worker.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_Worker_hpp__
#define __fon9_Worker_hpp__
#include "fon9/MustLock.hpp"

namespace fon9 {

enum class WorkerState {
   Sleeping,
   Ringing,
   Working,
   Disposing,
   Disposed,
};

fon9_WARN_DISABLE_PADDING;
class WorkContentBase {
   WorkerState State_{WorkerState::Sleeping};
   bool        IsAsyncTaking_{false};
public:
   WorkerState GetWorkerState() const {
      return this->State_;
   }
   void SetWorkerState(WorkerState st) {
      this->State_ = st;
   }

   /// \retval true  現在狀態為 Sleeping, 則設為 Ringing, 然後返回 true.
   /// \retval false 現在狀態不是 Sleeping, 不改變狀態.
   bool SetToRinging() {
      if (this->State_ == WorkerState::Sleeping) {
         this->State_ = WorkerState::Ringing;
         return true;
      }
      return false;
   }
   /// \retval true  現在狀態為 < Disposing, 則設為 Disposing, 然後返回 true.
   /// \retval false 現在狀態 >= Disposing, 不改變狀態.
   bool SetToDisposing() {
      if (this->State_ < WorkerState::Disposing) {
         this->State_ = WorkerState::Disposing;
         return true;
      }
      return false;
   }

   /// 正在喚醒「非同步工作」, 一旦喚醒就會開始工作。
   bool SetToAsyncTaking() {
      if (this->IsAsyncTaking_)
         return false;
      this->IsAsyncTaking_ = true;
      return true;
   }
   /// 「非同步工作」已喚醒, 接下來必定會呼叫 TakeCall();
   void SetAsyncTaken() {
      assert(this->IsAsyncTaking_ == true);
      this->IsAsyncTaking_ = false;
   }
};

/// \ingroup Thrs
/// 控制一次只會有一個 thread 執行 Worker，但是允許任意 thread 要求 Worker 做事。
/// - Multi Producer: 透過 Worker<> 通知 WorkContentController 執行、結束。
/// - Single Consumer: 管制 WorkContentController::TakeCall() 不會有重複進入的情況。
///
/// \tparam WorkContentController 必須提供(可參考 ThreadController_UT.cpp):
///   \code
///   struct MyWorkContent : public fon9::WorkContentBase {
///   };
///   class MyWorkContentController : public fon9::MustLock<MyWorkContent> {
///      // 在 Worker::Dispose() 轉呼叫到此.
///      void Dispose(Locker& lk, ...) {
///         if (this->SetToDisposing())
///            this->MakeNotifyAll(lk); // or this->MakeCall(lk); or this->NotifyOne(lk); or ...
///      }
///
///      // - 您應自行判斷: 如果 this->GetWorkerState() >= WorkerState::Disposing 要如何處理新加入的工作.
///      // - this->GetWorkerState() 有可能為 WorkerState::Working: 表示現在可能在 TakeCall() 裡面的 unlock 狀態.
///      // - 一般情況, 在加入新的工作後:
///      //   - this->GetWorkerState() == WorkerState::Sleeping, 則應喚醒 Worker.
///      //   - 在返回前(或 lk.unlock() 前) 設定 this->SetWorkerState(WorkerState::Ringing);
///      void AddWork(Locker& lk, ...);
///
///      // 取出並處理工作內容.
///      fon9::WorkerState TakeCall(Locker& lk) {
///         取出工作.
///         lk.unlock();
///         處理工作.
///         lk.lock();
///         return fon9::WorkerState::Working  = 還有工作, 返回後檢查現在狀態, 如果允許(現在非 Disposed), 則立即繼續工作;
///                其他 = 新的狀態, 結束此次工作階段:
///                  fon9::WorkerState::Sleeping  = 事情全部做完;
///                  fon9::WorkerState::Ringing   = 返回前已經調用過 this->NotifyOne(); or this->MakeCall(); or ...
///                  fon9::WorkerState::Disposing = 返回前已經呼叫 this->Dispose();
///                  fon9::WorkerState::Disposed  = 已進入結束狀態, 無法再透過 Worker::TakeCall() 進入 this->TakeCall(lk);
///      }
///   };
///   \endcode
template <class WorkContentController>
class Worker {
   fon9_NON_COPY_NON_MOVE(Worker);

   WorkContentController   Controller_;

public:
   using ContentLocker = typename WorkContentController::Locker;

   template <class... ArgsT>
   Worker(ArgsT&&... args) : Controller_{std::forward<ArgsT>(args)...} {
   }

   static Worker& StaticCast(WorkContentController& ctrl) {
      return ContainerOf(ctrl, &Worker::Controller_);
   }

   /// 進入結束狀態，之後都不會再呼叫 Notify.
   template <class... ArgsT>
   void Dispose(ArgsT&&... args) {
      ContentLocker ctx{this->Controller_};
      this->Controller_.Dispose(ctx, std::forward<ArgsT>(args)...);
   }

   template <class... ArgsT>
   void AddWork(ArgsT&&... args) {
      ContentLocker ctx{this->Controller_};
      this->Controller_.AddWork(ctx, std::forward<ArgsT>(args)...);
   }

   /// 如果現在是 Sleeping or Ringing or Disposing 狀態, 則會進入工作狀態.
   /// 可在任意 thread 呼叫, 但最多只會有其中一個進入工作階段，執行 WorkContentController::TakeCall()。
   WorkerState TakeCall() {
      ContentLocker ctx{this->Controller_};
      return this->TakeCall(ctx);
   }
   WorkerState TakeCall(ContentLocker& ctx) {
      switch (ctx->GetWorkerState()) {
      default:
      case WorkerState::Working:
      case WorkerState::Disposed:
         return ctx->GetWorkerState();
      case WorkerState::Sleeping:
      case WorkerState::Ringing:
         ctx->SetWorkerState(WorkerState::Working);
         break;
      case WorkerState::Disposing:
         break;
      }
      WorkerState res;
      do {
         res = this->Controller_.TakeCall(ctx);
         WorkerState cur = ctx->GetWorkerState();
         if (cur >= WorkerState::Disposed)
            return cur;
         assert(cur == WorkerState::Working || cur == WorkerState::Disposing);
      } while (res == WorkerState::Working || res == WorkerState::Disposing);
      ctx->SetWorkerState(res);
      return res;
   }

   /// 通常不會這樣使用 Worker.
   /// 一般而言當有需要 TakeCall() 時, 會把工作加入 ThreadPool, 然後在 ThreadPool 裡面呼叫 TakeCall().
   /// - 先呼叫 this->TakeCall(); 如果返回 WorkerState::Disposed 則結束 this->TakeNap().
   /// - 然後呼叫 WorkContentController::TakeNap();
   /// - 最後返回 return this->TakeCall();
   template <class... ArgsT>
   WorkerState TakeNap(ArgsT&&... args) {
      ContentLocker ctx{this->Controller_};
      WorkerState   res = this->TakeCall(ctx);
      if (res >= WorkerState::Disposed)
         return res;
      this->Controller_.TakeNap(ctx, std::forward<ArgsT>(args)...);
      return this->TakeCall(ctx);
   }

   /// FnWorkContent(ContentLocker&);
   template <class FnWorkContent>
   void GetWorkContent(FnWorkContent fnWorkContent) {
      ContentLocker ctx{this->Controller_};
      fnWorkContent(ctx);
   }
};
fon9_WARN_POP;

} // namespaces
#endif//__fon9_Worker_hpp__
