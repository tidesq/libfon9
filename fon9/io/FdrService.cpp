/// \file fon9/io/FdrService.cpp
/// \author fonwinz@gmail.com
#include "fon9/sys/Config.h"
#ifdef fon9_POSIX
#include "fon9/io/FdrService.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace io {

FdrService::FdrService(FdrThreads thrs, const IoServiceArgs& ioArgs, const std::string& thrName)
   : FdrThreads_{std::move(thrs)} {
   assert(!this->FdrThreads_.empty());
   size_t L = 0;
   for (auto& thr : this->FdrThreads_)
      thr->Thread_ = std::thread(&FdrThread::ThrRun, thr.get(), ServiceThreadArgs{ioArgs, thrName, L++});
}
FdrService::~FdrService() {
}
FdrThreadSP FdrService::AllocFdrThread(Fdr::fdr_t fd) {
   return this->FdrThreads_[static_cast<size_t>(fd) % this->FdrThreads_.size()];
}

//--------------------------------------------------------------------------//

void intrusive_ptr_deleter(const FdrThread* p) {
   if (p->Thread_.joinable())
      const_cast<FdrThread*>(p)->WakeupThread();
   else
      delete p;
}

FdrThread::~FdrThread() {
}
void FdrThread::CancelReqs(PendingReqsImpl reqs) {
   for (FdrEventHandlerSP& r : reqs)
      r->OnFdrEvent_Handling(FdrEventFlag::OperationCanceled);
}
void FdrThread::ThrRun(ServiceThreadArgs args) {
   args.OnThrRunBegin("FdrThread");
   this->ThreadId_ = ThisThread_.ThreadId_;
   this->ThrRunImpl(args);
   if (this->use_count() != 0) {
      // select(), poll(), epoll_wait()... error.
      fon9_LOG_FATAL("FdrThread.ThrRun.CancelMode|name=", args.Name_);
      while (this->use_count() > 0) {
         // 拒絕全部的要求, 直到沒有任何人擁有 this 的 FdrThreadSP 為止.
         this->CancelReqs(MoveOutPendingImpl(this->PendingSends_));
         this->CancelReqs(MoveOutPendingImpl(this->PendingUpdates_));
         MoveOutPendingImpl(this->PendingRemoves_);
         std::this_thread::yield();
      }
   }
   this->Thread_.detach();
   fon9_LOG_ThrRun("FdrThread.ThrRun.End|name=", args.Name_);
   delete this;
}
void FdrThread::ProcessPendingSends() {
   PendingReqsImpl reqs = this->MoveOutPendingImpl(this->PendingSends_);
   for (FdrEventHandlerSP& sender : reqs)
      sender->OnFdrEvent_StartSend();
}
void FdrThread::PushToPendingReqs(PendingReqs& reqs, FdrEventHandlerSP&& handler) {
   {
      PendingReqs::Locker lk{reqs};
      lk->emplace_back(std::move(handler));
   }
   this->WakeupThread();
}
void FdrThread::WakeupThread() {
   if (this->WakeupRequests_.fetch_add(1, std::memory_order_relaxed) == 0)
      if (!this->IsThisThread())
         this->WakeupFdr_.Wakeup();
}

//--------------------------------------------------------------------------//

FdrEventHandler::~FdrEventHandler() {
}

} } // namespaces
#endif//fon9_POSIX
