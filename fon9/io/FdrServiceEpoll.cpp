/// \file fon9/io/FdrServiceEpoll.cpp
/// \author fonwinz@gmail.com
#ifdef __linux__
#include "fon9/io/FdrServiceEpoll.hpp"
#include "fon9/Log.hpp"
#include <sys/epoll.h>

namespace fon9 { namespace io {

fon9_API FdrServiceSP MakeDefaultFdrService(const IoServiceArgs& ioArgs, const std::string& thrName, Result2& err) {
   return FdrServiceEpoll::MakeService(ioArgs, thrName, err);
}
FdrServiceSP FdrServiceEpoll::MakeService(const IoServiceArgs& ioArgs, const std::string& thrName, MakeResult& err) {
   size_t thrCount = ioArgs.ThreadCount_;
   if (thrCount <= 0)
      thrCount = 1;
   FdrService::FdrThreads thrs(thrCount);
   for (size_t L = 0; L < thrCount; ++L) {
      thrs[L].reset(new FdrThreadEpoll{err});
      if (err.IsError())
         return FdrServiceSP{};
   }
   thrs.shrink_to_fit();
   return FdrServiceSP{new FdrService{std::move(thrs), ioArgs, thrName + ".epoll"}};
}

//--------------------------------------------------------------------------//

FdrThreadEpoll::FdrThreadEpoll(FdrServiceEpoll::MakeResult& res)
   : FdrEpoll_{::epoll_create1(EPOLL_CLOEXEC)} {
   using Result = FdrServiceEpoll::MakeResult;
   if (!this->FdrEpoll_.IsReadyFD()) {
      res = Result{"epoll_create1", GetSysErrC()};
      return;
   }

   FdrNotify::Result resEvFd = this->WakeupFdr_.Open();
   if (resEvFd.IsError()) {
      res = Result{"WakeupFdr.Open", resEvFd.GetError()};
      return;
   }

   struct epoll_event evc;
   ZeroStruct(evc);
   evc.events = EPOLLIN;
   if (epoll_ctl(this->FdrEpoll_.GetFD(), EPOLL_CTL_ADD, this->WakeupFdr_.GetReadFD(), &evc) < 0) {
      res = Result{"epoll_ctl(WakeupFdr)", GetSysErrC()};
      return;
   }
}
FdrThreadEpoll::~FdrThreadEpoll() {
}

//--------------------------------------------------------------------------//

void FdrThreadEpoll::ThrRunImpl(const ServiceThreadArgs& args) {
   using EpollEvents = std::vector<struct epoll_event>;
   EpollEvents epEvents{16};
   EvHandlers  evHandlers{args.Capacity_};
   Fdr::fdr_t  epFdr = this->FdrEpoll_.GetFD();
   const int   kEpollWaitMS = (IsBlockWait(args.HowWait_) ? -1 : 0);
   while (this->use_count() > 0) {
      // 再次進入 epoll_wait() 之前, 必須先將 Pending Removes, Updates 處理完,
      // 因為: 在 OnFdrEvent_Emit() 裡面關閉 readable, writable 偵測, 必須確實執行.
      // 避免: 當 Device 必須回到 op thread 觸發 OnDevice_Recv() 或 執行 send,
      //       如果沒有確實禁止 readable, writable, 則可能會發生非預期的結果.
      int msWait = kEpollWaitMS;
      if (fon9_UNLIKELY(this->WakeupRequests_.load(std::memory_order_relaxed) != 0)) {
         this->ClearWakeup();
         this->ProcessPendings(epFdr, evHandlers);
         // 如果在 ProcessPendings() 時有再增加 Wakeup,
         // 則 epoll_wait() 應在偵測新進 ev 之後立即結束, 然後處理 ProcessPendings().
         if (this->WakeupRequests_.load(std::memory_order_relaxed) != 0)
            msWait = 0;
      }
      struct epoll_event* pEvBeg = &*epEvents.begin();
      int epRes = epoll_wait(epFdr, pEvBeg, static_cast<int>(epEvents.size()), msWait);
      if (fon9_LIKELY(epRes > 0)) {
         for (int L = 0; L < epRes; ++L, ++pEvBeg) {
            if (FdrEventHandler* hdr = static_cast<FdrEventHandler*>(pEvBeg->data.ptr)) {
               if (fon9_LIKELY(hdr->GetFdrEventHandlerBookmark() > 0)) {
                  const auto   eflags = pEvBeg->events;
                  FdrEventFlag evs = (eflags & EPOLLOUT) ? FdrEventFlag::Writable : FdrEventFlag::None;
                  if (eflags & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
                     evs |= FdrEventFlag::Readable;
                  if (fon9_UNLIKELY(eflags & (EPOLLHUP | EPOLLERR))) {
                     evs |= FdrEventFlag::Error;
                     // 避免 hdr 處理 error 期間, 這裡會一直觸發 error, 所以一旦 error, 就移除 handler.
                     hdr->RemoveFdrEvent();
                  }
                  this->OnFdrEvent_Emit(evs, hdr);
               }
            }
            else
               this->WakeupRequests_.store(1, std::memory_order_relaxed);
         }
         if (fon9_UNLIKELY(static_cast<size_t>(epRes) == epEvents.size())) {
            // epEvents 可能不足以容納一次的事件數量 => 擴充容量,
            // 若瞬間有大量事件,則可以減少 epoll_wait() 的呼叫次數.
            epEvents.resize(epRes * 2);
            epEvents.resize(epEvents.capacity());
         }
      }
      else if (fon9_LIKELY(epRes == 0)) { // 如果 !Block, 則 epRes==0 是常態!
         if (args.HowWait_ == HowWait::Yield)
            std::this_thread::yield();
      }
      else if (epRes < 0) {
         if (int eno = ErrorCannotRetry(errno))
            fon9_LOG_FATAL("FdrThreadEpoll.ThrRun|fn=epoll_wait|err=", GetSysErrC(eno));
      }
   }
}

void FdrThreadEpoll::ProcessPendings(Fdr::fdr_t epFdr, EvHandlers& evHandlers) {
   this->ProcessPendingSends();

   struct epoll_event evc;
   PendingReqsImpl reqs = this->MoveOutPendingImpl(this->PendingRemoves_);
   for (FdrEventHandlerSP& spRemove : reqs) {
      FdrEventHandler* hdr = spRemove.get();
      auto idx1 = hdr->GetFdrEventHandlerBookmark();
      if (fon9_UNLIKELY(idx1 <= 0))
         continue;
      if (!evHandlers.RemoveObj(idx1 - 1, hdr))
         fon9_LOG_ERROR("FdrServiceEpoll.Remove|fd=", hdr->GetFD(), "|idx=", idx1, "|hdr=", ToPtr{hdr}, "|err=Not found");
      if (fon9_UNLIKELY(epoll_ctl(epFdr, EPOLL_CTL_DEL, hdr->GetFD(), &evc) < 0)) {
         int eno = errno; // 必須先將 errno 取出, 否則進入 fon9_LOG_ERROR() 可能會破壞 errno 的值.
         fon9_LOG_ERROR("FdrServiceEpoll.DEL|fd=", hdr->GetFD(), "|err=", GetSysErrC(eno));
      }
      // fon9_LOG_TRACE("FdrServiceEpoll.Remove|fd=", hdr->GetFD(), "|idx=", idx1, "|hdr=", ToPtr{hdr});
      this->SetFdrEventHandlerBookmark(hdr, 0);
   }
   reqs = this->MoveOutPendingImpl(this->PendingUpdates_);
   for (FdrEventHandlerSP& sp : reqs) {
      FdrEventHandler* hdr = sp.get();
      auto idx1 = hdr->GetFdrEventHandlerBookmark();
      int  op;
      FdrEventFlag evs = hdr->GetRequiredFdrEventFlag();
      if (fon9_LIKELY(idx1 > 0)) {
         EvHandler*  pEvObj = evHandlers.GetObjPtr(idx1 - 1);
         if (fon9_UNLIKELY(pEvObj == nullptr))
            continue;
         if (pEvObj->get() != hdr || pEvObj->Events_ == evs)
            continue;
         pEvObj->Events_ = evs;
         op = EPOLL_CTL_MOD;
      }
      else {
         if (fon9_UNLIKELY(evs == FdrEventFlag::None))
            continue;
         EvHandler evh{hdr};
         evh.Events_ = evs;
         this->SetFdrEventHandlerBookmark(hdr, idx1 = evHandlers.Add(evh) + 1);
         op = EPOLL_CTL_ADD;
      }
      // EPOLLET 有其他問題? 莫名的斷線: 收到 events=0x2019? 0x19 = EPOLLHUP(0x10) + EPOLLERR(0x08) + EPOLLIN(0x01)
      evc.events = IsEnumContains(evs, FdrEventFlag::Readable)
         ? (EPOLLIN | EPOLLPRI | EPOLLRDHUP)
         : 0;
      if (IsEnumContains(evs, FdrEventFlag::Writable))
         evc.events |= EPOLLOUT;
      // if (IsEnumContains(evs, FdrEventFlag::Error))
      // 不論是否設定 FdrEventFlag::Error, 都要偵測錯誤事件.
         evc.events |= (EPOLLHUP | EPOLLERR);
      evc.data.ptr = hdr;
      if (epoll_ctl(epFdr, op, hdr->GetFD(), &evc) < 0) {
         int eno = errno; // 必須先將 errno 取出, 否則進入 fon9_LOG_ERROR() 可能會破壞 errno 的值.
         fon9_LOG_ERROR(op == EPOLL_CTL_ADD
                        ? StrView{"FdrServiceEpoll.ADD|fd="}
                        : StrView{"FdrServiceEpoll.MOD|fd="},
                        hdr->GetFD(),
                        "|evs=", evs,
                        "|err=", GetSysErrC(eno));
      }
   }
}

} } // namespaces
#endif
