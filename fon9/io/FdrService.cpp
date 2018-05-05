/// \file fon9/io/FdrService.cpp
/// \author fonwinz@gmail.com
#include "fon9/sys/Config.h"
#ifdef fon9_POSIX
#include "fon9/io/FdrService.hpp"
#include "fon9/ThreadPool.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace io {

FdrService::FdrService(const IoServiceArgs& ioArgs, std::string thrName, std::function<FdrThreadSP(const ServiceThrArgs&)> fnFdrThreadCreater)
   : FdrThreads_{ioArgs.ThreadCount_ <= 0 ? 1 : ioArgs.ThreadCount_}
{
   size_t thrCount = this->FdrThreads_.size();
   for (size_t L = 0; L < thrCount; ++L)
      this->FdrThreads_[L] = fnFdrThreadCreater(ServiceThrArgs{ioArgs, thrName, L});
}
FdrService::~FdrService() {
   this->StopAndWait("FdrService.dtor");
}
FdrThreadSP FdrService::AllocFdrThread(Fdr::fdr_t fd) {
   return this->FdrThreads_[static_cast<size_t>(fd) % this->FdrThreads_.size()];
}
void FdrService::StopAndWait(const StrArg& cause) {
   std::vector<std::thread> thrs{this->FdrThreads_.size()};
   size_t L = 0;
   for (FdrThreadSP& thr : this->FdrThreads_)
      thrs[L++] = thr->StopThread(cause);
   ThreadJoin(thrs);
}
//-------------------------------------------------------------------
FdrThread::~FdrThread() {
}
std::thread FdrThread::StopThread(const StrArg& cause) {
   if (this->ThrSt_ < ThrSt_Terminating) {
      this->ThrSt_ = ThrSt_Terminating;
      if (!cause.IsNullOrEmpty()) {
         TerminateMessage::Locker lk{this->TerminateMessage_};
         *lk = cause.ToString();
      }
   }
   this->WakeupThread();
   return std::move(this->Thr_);
}
std::string FdrThread::ThrRunTerminated() {
   this->ThrSt_ = ThrSt_Terminated;
   this->MoveOutPendingImpl(this->PendingReqs_);
   this->MoveOutPendingImpl(this->PendingSends_);
   this->MoveOutPendingImpl(this->PendingRemoves_);
   TerminateMessage::Locker lk{this->TerminateMessage_};
   return std::move(*lk);
}
void FdrThread::ProcessPendingSends() {
   PendingReqsImpl reqs = this->MoveOutPendingImpl(this->PendingSends_);
   for (FdrEventHandlerSP& sender : reqs)
      if (sender->OnFdrEvent_Send() == FdrEventHandler::AfterSend_HasRemain)
         this->Update_OnFdrEvent(sender);
}
void FdrThread::Update_OnFdrEvent(FdrEventHandlerSP handler) {
   if (this->ThrSt_ != ThrSt_Running)
      return;
   else {
      PendingReqs::Locker lk{this->PendingReqs_};
      lk->emplace_back(std::move(handler));
   }
   this->WakeupThread();
}
void FdrThread::Remove_OnFdrEvent(FdrEventHandlerSP handler) {
   if (this->ThrSt_ != ThrSt_Running)
      return;
   else {
      PendingReqs::Locker lk{this->PendingRemoves_};
      lk->emplace_back(std::move(handler));
   }
   this->WakeupThread();
}
void FdrThread::StartSendInFdrThread(FdrEventHandlerSP handler) {
   if (this->ThrSt_ != ThrSt_Running)
      return;
   else {
      PendingReqs::Locker lk{this->PendingSends_};
      lk->emplace_back(std::move(handler));
   }
   this->WakeupThread();
}
//-------------------------------------------------------------------
FdrEventHandler::FdrEventHandler(FdrServiceSP fdrSv, Fdr::fdr_t fd)
   : FdrThread_{fdrSv->AllocFdrThread(fd)}
   , FdrIndex_{fd, 0u}
{
}
FdrEventHandler::~FdrEventHandler() {
}
} } // namespaces
#endif//fon9_POSIX
