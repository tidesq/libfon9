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

class FdrService;
class FdrEventHandler;
using FdrEventHandlerSP = intrusive_ptr<FdrEventHandler>;

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

   /// 當無法提供服務時, 透過此旗標告知.
   OperationCanceled = 0x1000,
};
fon9_ENABLE_ENUM_BITWISE_OP(FdrEventFlag);

//--------------------------------------------------------------------------//

/// \ingroup io
/// 處理事件通知的 thread.
/// - 實際作法可能使用 select(), poll(), epoll(linux), kqueue(FreeBSD)...
/// - 每個 FdrThread 負責服務一批 FdrEventHandler
/// - 在 FdrEventHandler 建構時, 由 FdrService 決定該 handler 由哪個 FdrThread 服務
/// - 只有在全部的 FdrEventHandler 死亡後, 才會結束 thread.
class FdrThread : public intrusive_ref_counter<FdrThread> {
protected:
   using PendingReqsImpl = std::vector<FdrEventHandlerSP>;
   using PendingReqs = MustLock<PendingReqsImpl>;
   PendingReqs       PendingUpdates_;
   PendingReqs       PendingSends_;
   PendingReqs       PendingRemoves_;
   FdrNotify         WakeupFdr_;
   ThreadId::IdType  ThreadId_;
   std::atomic_uint_fast32_t  WakeupRequests_{0};

   void ClearWakeup() {
      assert(this->IsThisThread());
      this->WakeupFdr_.ClearWakeup();
      this->WakeupRequests_.store(0, std::memory_order_relaxed);
   }

   static void OnFdrEvent_Emit(FdrEventFlag evs, FdrEventHandler* handler);
   static void SetFdrEventHandlerBookmark(FdrEventHandler* handler, uint64_t bookmark);

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

private:
   std::thread Thread_;
   /// FdrThread 的自我保護措施: 在 ThrRun() 結束時才會 delete.
   friend void intrusive_ptr_deleter(const FdrThread* p);

   friend class FdrService;
   /// 由衍生者(FdrThreadEpoll...)負責實作.
   /// \code
   ///   while(this->use_count() > 0) {
   ///      ...
   ///   }
   /// \endcode
   virtual void ThrRunImpl(const ServiceThreadArgs& args) = 0;
   void ThrRun(ServiceThreadArgs args);
   void CancelReqs(PendingReqsImpl);

   friend class FdrEventHandler;
   void WakeupThread();
   void PushToPendingReqs(PendingReqs& reqs, FdrEventHandlerSP&& handler);
   void UpdateFdrEvent(FdrEventHandlerSP handler) {
      this->PushToPendingReqs(this->PendingUpdates_, std::move(handler));
   }
   void RemoveFdrEvent(FdrEventHandlerSP handler) {
      this->PushToPendingReqs(this->PendingRemoves_, std::move(handler));
   }
   void StartSendInFdrThread(FdrEventHandlerSP handler) {
      this->PushToPendingReqs(this->PendingSends_, std::move(handler));
   }
};
using FdrThreadSP = intrusive_ptr<FdrThread>;
extern void intrusive_ptr_deleter(const FdrThread* p);

//--------------------------------------------------------------------------//

/// \ingroup io
/// 負責管理 FdrThread, 決定 FdrEventHandler 要使用哪個 FdrThread.
class FdrService : public intrusive_ref_counter<FdrService> {
public:
   using FdrThreads = std::vector<FdrThreadSP>;

   /// 將 thrs 納入 Service, 並啟動 thrs 的 ThrRun().
   FdrService(FdrThreads thrs, const IoServiceArgs& ioArgs, const std::string& thrName);

   virtual ~FdrService();

   /// 預設使用 [fd % thrCount] 決定使用哪個 fdr thread.
   virtual FdrThreadSP AllocFdrThread(Fdr::fdr_t fd);

private:
   const FdrThreads  FdrThreads_;
};
using FdrServiceSP = intrusive_ptr<FdrService>;

//--------------------------------------------------------------------------//

/// \ingroup io
/// 處理 fd 的事件通知.
class fon9_API FdrEventHandler {
   fon9_NON_COPY_NON_MOVE(FdrEventHandler);

public:
   /// 建構時由 iosv 分配 FdrThread.
   FdrEventHandler(FdrService& iosv, FdrAuto&& fd)
      : FdrThread_{iosv.AllocFdrThread(fd.GetFD())}
      , Fdr_{std::move(fd)} {
   }

   virtual ~FdrEventHandler();

   /// 更新or設定: fd(fd在建構時指定) 需要的事件.
   /// - 把要求丟到 fdr thread 處理.
   /// - 若已設定過, 則為更新.
   /// - fdr thread 會用 FdrEventHandlerSP 保存 this,
   ///   - 當不再需要事件時, 應呼叫 RemoveFdrEvent() 移除事件通知,
   ///   - 無法在解構時處理 (因為尚未移除前, fdr service 會擁有 this SP, 不可能造成解構).
   void UpdateFdrEvent() {
      this->FdrThread_->UpdateFdrEvent(this);
   }

   /// 從 fdr thread 移除事件處理者.
   /// - 實際的移除動作會到 fdr thread 裡面處理.
   ///   - 如果 this thread == fdr thread 則會立即移除.
   ///   - 僅把 this 加入等候移除的 qu 就返回:
   ///      - 所以返回後仍有可能收到 FdrEvent() 事件.
   /// - 一旦移除, 就不會再收到任何事件, 即使再呼叫 UpdateFdrEvent() 也不會有任何作用.
   void RemoveFdrEvent() {
      this->FdrThread_->RemoveFdrEvent(this);
   }

   /// 通常在 SendBuffered() 時使用:
   /// 到 fdr thread 送出: 透過 this->OnFdrEvent_StartSend();
   void StartSendInFdrThread() {
      this->FdrThread_->StartSendInFdrThread(this);
   }

   bool InFdrThread() const {
      return this->FdrThread_->IsThisThread();
   }
   uint64_t GetFdrEventHandlerBookmark() const {
      return this->FdrThreadBookmark_;
   }
   Fdr::fdr_t GetFD() const {
      return this->Fdr_.GetFD();
   }
   /// 取得需要哪些事件.
   /// - 一般而言只會在 fdr thread 呼叫.
   /// - FdrEventFlag::None 僅表示現在不須要事件,
   ///   但是沒有從 fdr thread 移除, 若要移除, 應使用 RemoveFdrEvent();
   virtual FdrEventFlag GetRequiredFdrEventFlag() const = 0;

private:
   friend class FdrThread;
   // 在建構時就決定了要使用哪個 FdrThread & 處理哪個 fd 的事件.
   const FdrThreadSP FdrThread_;
   const FdrAuto     Fdr_;
   uint64_t          FdrThreadBookmark_{0};

   /// 可能同時有多種事件通知.
   /// 只會在 fdr thread 裡面呼叫.
   virtual void OnFdrEvent_Handling(FdrEventFlag evs) = 0;

   /// 透過 StartSendInFdrThread() 啟動在 fdr thread 的傳送.
   virtual void OnFdrEvent_StartSend() = 0;

   virtual void OnFdrEvent_AddRef() = 0;
   virtual void OnFdrEvent_ReleaseRef() = 0;

   inline friend void intrusive_ptr_add_ref(const FdrEventHandler* p) {
      const_cast<FdrEventHandler*>(p)->OnFdrEvent_AddRef();
   }
   inline friend void intrusive_ptr_release(const FdrEventHandler* p) {
      const_cast<FdrEventHandler*>(p)->OnFdrEvent_ReleaseRef();
   }
};

//--------------------------------------------------------------------------//

inline void FdrThread::OnFdrEvent_Emit(FdrEventFlag evs, FdrEventHandler* handler) {
   handler->OnFdrEvent_Handling(evs);
}
inline void FdrThread::SetFdrEventHandlerBookmark(FdrEventHandler* handler, uint64_t bookmark) {
   handler->FdrThreadBookmark_ = bookmark;
}

} } // namespaces
#endif//__fon9_io_FdrService_hpp__
