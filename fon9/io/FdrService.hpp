/// \file fon9/io/FdrService.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_FdrService_hpp__
#define __fon9_io_FdrService_hpp__
#include "fon9/io/IoBase.hpp"
#include "fon9/io/IoServiceArgs.hpp"
#include "fon9/FdrNotify.hpp"
#include "fon9/MustLock.hpp"
#include "fon9/ThreadId.hpp"

#include <thread>
#include <vector>

namespace fon9 { namespace io {

class FdrEventHandler;
using FdrEventHandlerSP = intrusive_ptr<FdrEventHandler>;

class FdrService;
using FdrServiceSP = intrusive_ptr<FdrService>;

enum class FdrEventFlag {
   /// 沒有要關心的事件.
   None = 0,
   /// 有資料可讀時觸發.
   Readable = 0x0001,
   /// 可寫入資料時觸發.
   Writable = 0x0010,
   /// 當有錯誤發生, 一定會通知,
   /// 註冊事件時不用提供此旗標.
   Error = 0x0100,
};
fon9_ENABLE_ENUM_BITWISE_OP(FdrEventFlag);

//--------------------------------------------------------------------------//

/// \ingroup io
/// 處理事件通知的 thread.
/// - 實際作法可能使用 select(), poll(), epoll(linux), kqueue(FreeBSD)...
/// - 每個 FdrThread 負責服務一批 FdrEventHandler
/// - 在 FdrEventHandler 建構時, 由 FdrService 決定該 handler 由哪個 FdrThread 服務
class FdrThread : public intrusive_ref_counter<FdrThread> {
   friend class FdrEventHandler;
   void Update_OnFdrEvent(FdrEventHandlerSP handler);
   void Remove_OnFdrEvent(FdrEventHandlerSP handler);
   void StartSendInFdrThread(FdrEventHandlerSP handler);
   void WakeupThread() {
      if (++this->WakeupRequests_ == 1 && !this->IsThisThread())
         this->WakeupFdr_.Wakeup();
   }
protected:
   enum ThrSt {
      ThrSt_NotRun,
      ThrSt_Running,
      ThrSt_Terminating,
      ThrSt_Terminated,
   };
   volatile ThrSt ThrSt_{ThrSt_NotRun};
   std::thread    Thr_;
   using TerminateMessage = MustLock<std::string>;
   TerminateMessage  TerminateMessage_;

   using PendingReqsImpl = std::vector<FdrEventHandlerSP>;
   using PendingReqs = MustLock<PendingReqsImpl>;
   PendingReqs       PendingReqs_;
   PendingReqs       PendingSends_;
   PendingReqs       PendingRemoves_;
   FdrNotify         WakeupFdr_;
   ThreadId::IdType  ThreadId_;
   std::atomic_uint_fast32_t  WakeupRequests_{0};

   friend class FdrService;
   std::thread StopThread(StrView cause);
   /// 由衍生者主動呼叫, 清理資源.
   /// 傳回: 結束原因 TerminateMessage_
   std::string ThrRunTerminated();
   template <class EvHandlers>
   void ThrRunTerminated(EvHandlers evHandlers);

   uint64_t ClearWakeup() {
      assert(this->IsThisThread());
      this->WakeupRequests_ = 0;
      return this->WakeupFdr_.ClearWakeup();
   }
   static void EmitOnFdrEvent(FdrEventFlag evs, FdrEventHandler* handler);
   static void ResetFdrEventHandlerIndex(FdrEventHandler* handler);
   static void SetFdrEventHandlerIndex(FdrEventHandler* handler, uint32_t index);

   static PendingReqsImpl MoveOutPendingImpl(PendingReqs& impl) {
      PendingReqs::Locker lk{impl};
      return std::move(*lk);
   }
   void ProcessPendingSends();
public:
   virtual ~FdrThread();
   bool IsThisThread() const {
      return this->ThreadId_ == ThisThread_.ThreadId_;
   }
};
using FdrThreadSP = intrusive_ptr<FdrThread>;

//--------------------------------------------------------------------------//

/// \ingroup io
/// 處理 fd 的事件通知.
class fon9_API FdrEventHandler {
   fon9_NON_COPY_NON_MOVE(FdrEventHandler);
public:
   /// 建構時由 iosv 分配 FdrThread.
   FdrEventHandler(FdrServiceSP iosv, FdrAuto&& fd)
      : FdrThread_{std::move(iosv)}
      , Fdr_{std::move(fd)} {
   }

   virtual ~FdrEventHandler();

   /// 更新or設定: fd(fd在建構時指定) 需要的事件.
   /// - 把要求丟到 fdr thread 處理.
   /// - 若已設定過, 則為更新.
   /// - fdr thread 會用 FdrEventHandlerSP 保存 this,
   ///   - 當不再需要事件時, 應呼叫 Remove_OnFdrEvent() 移除事件通知,
   ///   - 無法在解構時處理 (因為尚未移除前, fdr service 會擁有 this SP, 不可能造成解構).
   void Update_OnFdrEvent() {
      this->FdrThread_->Update_OnFdrEvent(this);
   }

   /// 通常在 SendBuffered() 時使用:
   /// 到 fdr thread 送出: 透過 this->OnFdrEvent_Send();
   void StartSendInFdrThread() {
      this->FdrThread_->StartSendInFdrThread(this);
   }

   /// 從 fdr thread 移除事件處理者.
   /// - 實際的移除動作會到 fdr thread 裡面處理.
   ///   - 如果 this thread == fdr thread 則會立即移除.
   ///   - 僅把 this 加入等候移除的 qu 就返回:
   ///      - 所以返回後仍有可能收到 OnFdrEvent() 事件.
   /// - 一旦移除, 就不會再收到任何事件, 即使再呼叫 Update_OnFdrEvent() 也不會有任何作用.
   void Remove_OnFdrEvent() {
      this->FdrThread_->Remove_OnFdrEvent(this);
   }

   /// 取得需要哪些事件.
   /// - 只會在 fdr thread 呼叫.
   /// - 每個 fd(建構時指定) 只能設定一組 evs + evHandler;
   ///   **不能** 針對不同 FdrEventFlag 設定不同的 evHandler.
   /// - FdrEventFlag::None 僅表示現在不須要事件, 但是沒有從 fdr thread 移除.
   virtual FdrEventFlag GetFdrEventFlagRequired() const = 0;

   bool InFdrThread() const {
      return this->FdrThread_->IsThisThread();
   }

   enum AfterSend {
      AfterSend_Error,
      AfterSend_Empty,
      AfterSend_HasRemain,
   };

private:
   friend class FdrThread;

   /// 可能同時有多種事件通知.
   /// 只會在 fdr thread 裡面呼叫.
   virtual void OnFdrEvent(FdrEventFlag evs) = 0;

   /// 透過 StartSendInFdrThread() 啟動在 fdr thread 的傳送.
   virtual AfterSend OnFdrEvent_Send() = 0;

   virtual void OnFdrEvent_AddRef() = 0;
   virtual void OnFdrEvent_ReleaseRef() = 0;

   inline friend void intrusive_ptr_add_ref(const FdrEventHandler* p) {
      const_cast<FdrEventHandler*>(p)->OnFdrEvent_AddRef();
   }
   inline friend void intrusive_ptr_release(const FdrEventHandler* p) {
      const_cast<FdrEventHandler*>(p)->OnFdrEvent_ReleaseRef();
   }

   // 在建構時就決定了要使用哪個 FdrThread & 處理哪個 fd 的事件.
   const FdrThreadSP FdrThread_;
   const FdrAuto     Fdr_;
};

//--------------------------------------------------------------------------//

/// \ingroup io
/// 負責管理 FdrThread, 決定 FdrEventHandler 要使用哪個 FdrThread.
class FdrService : public intrusive_ref_counter<FdrService> {
   using FdrThreads = std::vector<FdrThreadSP>;
   FdrThreads  FdrThreads_;
public:
   using ThreadCreater = std::function<FdrThreadSP(const ServiceThreadArgs&)>;
   FdrService(const IoServiceArgs& ioArgs, std::string thrName, ThreadCreater fnFdrThreadCreater);
   virtual ~FdrService();
   /// 結束服務, 並在 Thread 全部結束後返回.
   void StopAndWait(StrView cause);
   /// 預設使用 (fd % thrCount) 決定使用哪個 fdr thread.
   virtual FdrThreadSP AllocFdrThread(Fdr::fdr_t fd);
};



inline void FdrThread::EmitOnFdrEvent(FdrEventFlag evs, FdrEventHandler* handler) {
   handler->OnFdrEvent(evs);
}
inline void FdrThread::ResetFdrEventHandlerIndex(FdrEventHandler* handler) {
   handler->FdrIndex_.Fdr_ = Fdr::kInvalidValue;
}
inline void FdrThread::SetFdrEventHandlerIndex(FdrEventHandler* handler, uint32_t index) {
   handler->FdrIndex_.Index_ = index;
}
template <class EvHandlers>
void FdrThread::ThrRunTerminated(EvHandlers evHandlers) {
   std::string cause = this->ThrRunTerminated();
   for (auto& evh : evHandlers) {
      if (evh)
         evh->OnFdrEvent_FdrThreadTerminate(cause);
   }
}

} } // namespaces
#endif//__fon9_io_FdrService_hpp__
