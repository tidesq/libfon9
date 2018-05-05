/// \file fon9/io/FdrServiceEpoll.cpp
/// \author fonwinz@gmail.com
#ifdef __linux__
#include "fon9/io/FdrServiceEpoll.hpp"
#include "fon9/Log.hpp"
#include <sys/epoll.h>
namespace fon9 { namespace io {

FdrServiceSP FdrThreadEpoll::CreateFdrService(const IoServiceArgs& ioArgs, ResultFuncSysErr& res, std::string thrName) {
   return FdrServiceSP{new FdrService{ioArgs, thrName, [&res](const ServiceThrArgs& args) {
      ResultFuncSysErr  locRes;
      FdrThreadSP       retval{new FdrThreadEpoll{args, locRes}};
      if (locRes.IsError()) {
         fon9_LOG_FATAL("FdrThreadEpoll.create:|err=", locRes, "|name=", args.Name_);
         res = locRes;
      }
      return retval;
   }}};
}
//---------------------------------------------------
FdrThreadEpoll::FdrThreadEpoll(const ServiceThrArgs& args, ResultFuncSysErr& res) {
   FdrEvent::Result resEvFd = this->WakeupFdr_.Open();
   if (!resEvFd) {
      res.SetError("Create.WakeupFd", resEvFd.GetErrorCode());
      return;
   }
   FdrAuto epFd{::epoll_create1(EPOLL_CLOEXEC)};
   if (!epFd.IsReadyFD()) {
      res.SetError("epoll_create1", GetSysErr());
      return;
   }
   struct epoll_event evc;
   ZeroStruct(evc);
   evc.events = EPOLLIN;
   if (epoll_ctl(epFd.GetFD(), EPOLL_CTL_ADD, this->WakeupFdr_.GetReadFD(), &evc) < 0) {
      res.SetError("epoll_ctl(WakeupFd)", GetSysErr());
      return;
   }
   this->ThrSt_ = ThrSt_Running;
   this->Thr_ = std::thread{&FdrThreadEpoll::ThrRun, this, std::move(epFd), args};
}
FdrThreadEpoll::~FdrThreadEpoll() {
}
static void LogEpollError(StrView logHead, Fdr::fdr_t fd) {
   SysErr eno = GetSysErr();
   fon9_LOG_ERROR(logHead, fd, "|err=", eno, ':', GetErrCodeMessage(eno));
}
void FdrThreadEpoll::ThrRun(FdrAuto&& epFdAuto, ServiceThrArgs args) {
   args.OnThrRunBegin(this->Thr_, "FdrThreadEpoll");
   this->ThreadId_ = ThisThread_.ThreadId_;
   using SysRes = ResultCode<int>;
   SysRes sysRes;
   StrArg sysErrHead;

   using EpollEvents = std::vector<struct epoll_event>;
   EpollEvents epEvVec{16};
   EvHandlers  evHandlers{args.Capacity_};
   Fdr::fdr_t  epFd = epFdAuto.GetFD();
   const int   msTimeout = (IsWaitPolicyBlock(args.WaitPolicy_) ? -1 : 0);
   while (this->ThrSt_ < ThrSt_Terminating) {
      struct epoll_event* pEvBeg = &*epEvVec.begin();
      int epRes = epoll_wait(epFd, pEvBeg, static_cast<int>(epEvVec.size()), msTimeout);
      if (fon9_UNLIKELY(this->ThrSt_ >= ThrSt_Terminating))
         break;
      if (fon9_LIKELY(epRes == 0)) { // 如果 !Block, 則 epRes==0 是常態!
         if (args.WaitPolicy_ == WaitPolicySelect::Yield)
            std::this_thread::yield();
         continue;
      }
   //#define DEBUG_LATENCY
   #ifdef DEBUG_LATENCY
      TimeStamp wtm = TimeStamp::Now();  // for debug latency.
   #endif
      //fon9_LOG_DEBUG("FdrServiceEpoll.wait:|res=", epRes);
      // if (epRes == 0): epoll_wait() timeout
      if (fon9_LIKELY(epRes > 0)) {
         for (int L = 0; L < epRes; ++L) {
            if (fon9_UNLIKELY(pEvBeg->data.u32 == 0))
               this->WakeupRequests_ = 1;
            else if (EvHandler* pEvHdr = evHandlers.GetObjPtr(pEvBeg->data.u32 - 1)) {
               if (FdrEventHandler* hdr = pEvHdr->get()) {
                  const auto eflags = pEvBeg->events;
                  FdrEventFlag evs = (eflags & EPOLLOUT) ? FdrEventFlag::Writable : FdrEventFlag::None;
                  if (eflags & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
                     evs |= FdrEventFlag::Readable;
                  if (eflags & (EPOLLHUP | EPOLLERR)) {
                     evs |= FdrEventFlag::Error;
                     hdr->Remove_OnFdrEvent(); //避免 hdr 到另一 thread 處理 error, 這裡會一直觸發 error, 所以一旦 error, 就移除handler.
                  }
                  //fon9_LOG_DEBUG("FdrServiceEpoll:|fd=", hdr->GetFdrIndex().Fdr_, "|index=", hdr->GetFdrIndex().Index_, "|ev=", eflags, FmtHex{}, "|evs=", evs, "|u=", pEvHdr->Events_);
                  this->EmitOnFdrEvent(evs, hdr);
               }
            }
            ++pEvBeg;
         }
         if (fon9_UNLIKELY(this->WakeupRequests_ != 0)) {
            this->ClearWakeup();
            this->ProcessPendings(epFd, evHandlers);
         }
         if (fon9_UNLIKELY(static_cast<size_t>(epRes) >= epEvVec.size())) {
            // epEvVec 不足以容納一次的事件數量 => 擴充容量,
            // 若瞬間有大量事件,則可以減少 epoll_wait() 的呼叫次數.
            epEvVec.resize(epRes * 2);
            epEvVec.resize(epEvVec.capacity());
         }
      #ifdef DEBUG_LATENCY
         fon9_LOG_DEBUG("FdrThreadEpoll.ThrRun:|wakeup time=", wtm);
      #endif
      }
      else if (epRes < 0) {
         sysRes = SysRes{GetSysErr()};
         if (sysRes.GetErrorCode() == std::errc::interrupted)
            continue;
         sysErrHead = "FdrThreadEpoll.ThrRun:|fn=epoll_wait|err=";
         fon9_LOG_FATAL(sysErrHead, sysRes);
      }
      if (args.WaitPolicy_ == WaitPolicySelect::Yield) // 為了避免 busy poll 占用過多 CPU,
         std::this_thread::yield();                    // 所以每次事件處理完, 都要讓別人也有機會執行.
   }
   this->ThrRunTerminated(evHandlers.MoveOut());
}
void FdrThreadEpoll::ProcessPendings(Fdr::fdr_t epFd, EvHandlers& evHandlers) {
   struct epoll_event evc;
   this->ProcessPendingSends();
   PendingReqsImpl reqs = this->MoveOutPendingImpl(this->PendingRemoves_);
   //if (!reqs.empty()) fon9_LOG_DEBUG("FdrServiceEpoll.Removes:|size=", reqs.size());
   for (FdrEventHandlerSP& spRemove : reqs) {
      FdrEventHandler* hdr = spRemove.get();
      const FdrIndex&  idx = hdr->GetFdrIndex();
      if (fon9_UNLIKELY(idx.Fdr_ < 0))
         continue;
      //fon9_LOG_DEBUG("FdrServiceEpoll.Remove:|fd=", idx.Fdr_, "|index=", idx.Index_);
      evHandlers.Remove(idx.Index_ - 1, hdr);
      if (fon9_UNLIKELY(epoll_ctl(epFd, EPOLL_CTL_DEL, idx.Fdr_, &evc) < 0))
         LogEpollError("FdrServiceEpoll.Remove:|fd=", idx.Fdr_);
      this->ResetFdrEventHandlerIndex(hdr);
   }
   reqs = this->MoveOutPendingImpl(this->PendingReqs_);
   //if (!reqs.empty()) fon9_LOG_DEBUG("FdrServiceEpoll.Reqs:|size=", reqs.size());
   for (FdrEventHandlerSP& sp : reqs) {
      FdrEventHandler* hdr = sp.get();
      const FdrIndex&  idx = hdr->GetFdrIndex();
      if (fon9_UNLIKELY(idx.Fdr_ < 0))
         continue;
      int op;
      FdrEventFlag evs = hdr->GetFdrEventFlagRequired();
      if (fon9_LIKELY(idx.Index_ > 0)) {
         EvHandler*  pEvHdr = evHandlers.GetObjPtr(idx.Index_ - 1);
         if (fon9_UNLIKELY(pEvHdr == nullptr))
            continue;
         if (pEvHdr->get() != hdr || pEvHdr->Events_ == evs)
            continue;
         pEvHdr->Events_ = evs;
         op = EPOLL_CTL_MOD;
      }
      else {
         if (fon9_UNLIKELY(evs == FdrEventFlag{}))
            continue;
         EvHandler evh{hdr};
         evh.Events_ = evs;
         this->SetFdrEventHandlerIndex(hdr, static_cast<uint32_t>(evHandlers.Add(evh) + 1));
         op = EPOLL_CTL_ADD;
      }
      // EPOLLET 有其他問題? 莫名的斷線: 收到 events=0x2019? 0x19 = EPOLLHUP(0x10) + EPOLLERR(0x08) + EPOLLIN(0x01)
      evc.events = EnumIsContains(evs, FdrEventFlag::Readable)
         ? (EPOLLIN | EPOLLPRI | EPOLLRDHUP)
         : 0;
      if (EnumIsContains(evs, FdrEventFlag::Writable))
         evc.events |= EPOLLOUT;
      if (EnumIsContains(evs, FdrEventFlag::Error))
         evc.events |= (EPOLLHUP | EPOLLERR);
      evc.data.u32 = idx.Index_;
      //fon9_LOG_DEBUG(op == EPOLL_CTL_ADD ? StrArg{"FdrServiceEpoll.Add:|fd="} : StrArg{"FdrServiceEpoll.Mod:|fd="}, idx.Fdr_, "|index=", idx.Index_, "|evs=", evs);
      if (epoll_ctl(epFd, op, idx.Fdr_, &evc) < 0)
         LogEpollError(op == EPOLL_CTL_ADD ? StrArg{"FdrServiceEpoll.Add:|fd="} : StrArg{"FdrServiceEpoll.Mod:|fd="}, idx.Fdr_);
   }
}
   
} } // namespaces
#endif
