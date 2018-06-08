/// \file fon9/io/SocketAddressDN.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/SocketAddressDN.hpp"
#include "fon9/io/Socket.hpp"
#include "fon9/StrTools.hpp"
#include "fon9/DefaultThreadPool.hpp"
#include "fon9/Worker.hpp"
#include "fon9/ThreadId.hpp"

namespace fon9 { namespace io {

void DomainNameParser::Reset(std::string dn, port_t defaultPortNo) {
   this->DefaultPortNo_ = htons(defaultPortNo);
   this->LastPortNo_ = 0;
   this->DomainNames_ = std::move(dn);
   this->DnParser_ = ToStrView(this->DomainNames_);
   StrTrim(&this->DnParser_);
}

void DomainNameParser::Parse(StrView dnPort, DomainNameParseResult& res) {
   StrView dn1; // 把 dn1:port 分開.
   if (dnPort.Get1st() == '[') { // "[ipv6]:port"
      this->AddrHints_.ai_family = AF_INET6;
      dn1 = FetchFirstBr(dnPort);//取出 "[dn1]"
      StrTrim(&dn1);
   }
   else { // default address family
      dn1 = StrFetchTrim(dnPort, ':');
   }
   if (StrTrim(&dnPort).Get1st() == ':')
      StrTrimHead(&dnPort, dnPort.begin() + 1);

   *const_cast<char*>(dn1.end()) = '\0';
   *const_cast<char*>(dnPort.end()) = '\0';
   if (*dn1.begin() || *dnPort.begin()) {
      struct addrinfo* gaiRes;
      if (int eno = getaddrinfo(dn1.begin(), dnPort.begin(), &this->AddrHints_, &gaiRes)) {
         // "(dn1:dnPort=eno:message)"
      #ifdef fon9_WINDOWS
         eno = WSAGetLastError();
      #endif
         if (res.ErrMsg_.empty())
            res.ErrMsg_.push_back('(');
         else
            res.ErrMsg_.append(",(", 2);
         if (this->AddrHints_.ai_family == AF_INET6)
            res.ErrMsg_.push_back('[');
         dn1.AppendTo(res.ErrMsg_);
         if (this->AddrHints_.ai_family == AF_INET6)
            res.ErrMsg_.push_back(']');
         if (!dnPort.empty()) {
            res.ErrMsg_.push_back(':');
            dnPort.AppendTo(res.ErrMsg_);
         }
         res.ErrMsg_.push_back('=');
         NumOutBuf nbuf;
         res.ErrMsg_.append(SIntToStrRev(nbuf.end(), eno), nbuf.end());
         res.ErrMsg_.push_back(':');
      #ifdef fon9_WINDOWS
         res.ErrMsg_.append(GetSocketErrC(eno).message());
      #else
         res.ErrMsg_.append(gai_strerror(eno));
      #endif
         res.ErrMsg_.push_back(')');
      }
      else {
         //printf("%s:%s\n", dn1.begin(), dnPort.begin());
         port_t*           sPortPtr;
         SocketAddress     sAddr;
         struct addrinfo*  pAddrPrev = nullptr;
         for (struct addrinfo* pAddr = gaiRes; pAddr != nullptr; pAddr = (pAddrPrev = pAddr)->ai_next) {
            size_t addrSize;
            if (pAddr->ai_family == AF_INET) {
               addrSize = sizeof(sAddr.Addr4_);
               sPortPtr = &sAddr.Addr4_.sin_port;
            }
            else if (pAddr->ai_family == AF_INET6) {
               addrSize = sizeof(sAddr.Addr6_);
               sPortPtr = &sAddr.Addr6_.sin6_port;
            }
            else {
               continue;
            }
            if (pAddr->ai_addrlen != addrSize)
               continue;

            if (pAddrPrev && pAddrPrev->ai_addrlen == pAddr->ai_addrlen
                && memcmp(pAddrPrev->ai_addr, pAddr->ai_addr, addrSize) == 0) {
               // 可能取得相同的 ip:port?
               // Linux 4.4.0-87-generic #110-Ubuntu SMP Tue Jul 18 12:55:35 UTC 2017 x86_64 x86_64 x86_64 GNU/Linux
               // 使用 localhost 會取得 2 次 127.0.0.1 ??
               continue;
            }
            memcpy(&sAddr.Addr_, pAddr->ai_addr, addrSize);
            if (*sPortPtr == 0)
               *sPortPtr = this->DefaultPortNo_ ? this->DefaultPortNo_ : this->LastPortNo_;
            else
               this->LastPortNo_ = *sPortPtr;
            res.AddressList_.push_back(sAddr);

         #if 0
            printf("    ");
            for (const byte* p = reinterpret_cast<const byte*>(&addr.Addr_); addrSize > 0; --addrSize)
               printf("%02x ", *p++);
            char ip[fon9::io::SocketAddress::kMaxAddrPortStrSize];
            addr.ToAddrPortStr(ip);
            printf("/// %s\n", ip);
         #endif
         }
         freeaddrinfo(gaiRes);
      }
   }
}

bool DomainNameParser::Parse(DomainNameParseResult& res) {
   while (!this->DnParser_.empty()) {
      StrView dnPort = StrFetchTrim(this->DnParser_, ',');
      if (!dnPort.empty()) {
         const int oldfamily = this->AddrHints_.ai_family;
         this->Parse(dnPort, res);
         this->AddrHints_.ai_family = oldfamily;
         if (!this->DnParser_.empty())
            return true;
         break;
      }
   }
   if (this->LastPortNo_ && this->DefaultPortNo_ == 0) {
      for (SocketAddress& addr : res.AddressList_) {
         port_t& portRef = (addr.Addr_.sa_family == AF_INET6 ? addr.Addr6_.sin6_port : addr.Addr4_.sin_port);
         if (portRef != 0)
            break;
         portRef = this->LastPortNo_;
      }
   }
   return false;
}

//--------------------------------------------------------------------------//

namespace impl {
fon9_WARN_DISABLE_PADDING;
struct DnRequest {
   std::string             DomainName_;
   SocketAddress::port_t   DefaultPortNo_;
   FnOnSocketAddressList   FnOnReady_;
   DnRequest() = default;
   DnRequest(std::string&& dn, SocketAddress::port_t defaultPortNo, FnOnSocketAddressList&& fnOnReady)
      : DomainName_{std::move(dn)}
      , DefaultPortNo_{defaultPortNo}
      , FnOnReady_{std::move(fnOnReady)} {
   }
};

struct DnWorkContent : public WorkContentBase {
   fon9_NON_COPY_NON_MOVE(DnWorkContent);
   DnWorkContent() = default;
   using DnRequests = std::deque<DnRequest>;
   DnRequests        DnRequests_;
   DnQueryReqId      FrontId_{1}; // DnRequests_[0] 的 Id.
   DnQueryReqId      EmittingId_{};
};
fon9_WARN_POP;

class DnWorkController : public MustLock<DnWorkContent> {
   fon9_NON_COPY_NON_MOVE(DnWorkController);
   DomainNameParser        DnParser_;
   DomainNameParseResult   DnResult_;
public:
   DnWorkController() = default;
   using DnWorkerBase = Worker<DnWorkController>;

   void Dispose(Locker& ctx) {
      // 程式即將結束? 不用處理 dn 解析了!
      ctx->SetWorkerState(WorkerState::Disposed);
      auto reqs{std::move(ctx->DnRequests_)};
      ctx.unlock();
      reqs.clear();
      ctx.lock();
   }

   fon9::WorkerState TakeCall(Locker& ctx) {
      DnRequest&  req = ctx->DnRequests_[0];
      this->DnParser_.Reset(req.DomainName_, req.DefaultPortNo_);
      this->DnResult_.Clear();
      while (req.FnOnReady_) { // 若 !req.FnOnReady_ 表示: 已被 DnWorker::Cancel(); 清除, 此次要求已被取消.
         ctx.unlock();
         // 花時間的工作, 放在 unlock 之後.
         bool isDone = !this->DnParser_.Parse(this->DnResult_);

         ctx.lock();
         if (ctx->GetWorkerState() >= WorkerState::Disposing) {
            this->Dispose(ctx);
            return WorkerState::Disposed;
         }
         if (isDone) {
            if (FnOnSocketAddressList fnOnReady = std::move(req.FnOnReady_)) {
               DnQueryReqId id = ctx->EmittingId_ = ctx->FrontId_;
               ctx.unlock(); // 觸發結果事件, 在 unlock 之後.
               fnOnReady(id, this->DnResult_);
               ctx.lock();
            }
            break;
         }
      }
      ++ctx->FrontId_;
      ctx->DnRequests_.pop_front();
      return ctx->DnRequests_.empty() ? WorkerState::Sleeping : WorkerState::Working;
   }

   void AddWork(Locker& lk, DnQueryReqId& id, std::string&& dn, SocketAddress::port_t defaultPortNo, FnOnSocketAddressList&& fnOnReady) {
      id = lk->FrontId_ + lk->DnRequests_.size();
      lk->DnRequests_.emplace_back(std::move(dn), defaultPortNo, std::move(fnOnReady));
      if (lk->SetToRinging()) {
         lk.unlock();
         GetDefaultThreadPool().EmplaceMessage(std::bind(&DnWorkerBase::TakeCall, &DnWorkerBase::StaticCast(*this)));
      }
   }
};

struct DnWorker : public DnWorkController::DnWorkerBase {
   fon9_NON_COPY_NON_MOVE(DnWorker);
   DnWorker() = default;

   void Cancel(DnQueryReqId id) {
      if (id > 0)
         this->GetWorkContent([&](ContentLocker& ctx) {
            this->Cancel(ctx, id);
         });
   }
   void CancelAndWait(DnQueryReqId* id) {
      if (id && *id > 0) {
         this->GetWorkContent([&](ContentLocker& ctx) {
            this->Cancel(ctx, *id);
            if (ctx->InTakingCallThread())
               return;
            while (ctx->EmittingId_ == *id && ctx->FrontId_ <= *id) {
               ctx.unlock();
               std::this_thread::yield();
               ctx.lock();
            }
         });
         *id = 0;
      }
   }
public:
   void Cancel(ContentLocker& ctx, DnQueryReqId id) {
      size_t dnIndex = id - ctx->FrontId_;
      if (dnIndex < ctx->DnRequests_.size())
         ctx->DnRequests_[dnIndex] = DnRequest{};
   }
};
} // namespace impl
using DnWorker = impl::DnWorker;

static DnWorker& GetDefaultDnWorker() {
   static DnWorker   DnWorker_;
   return DnWorker_;
}

void AsyncDnQuery(DnQueryReqId& id, std::string dn, SocketAddress::port_t defaultPortNo, FnOnSocketAddressList fnOnReady) {
   return GetDefaultDnWorker().AddWork(id, std::move(dn), defaultPortNo, std::move(fnOnReady));
}
void AsyncDnQuery_CancelAndWait(DnQueryReqId* id) {
   return GetDefaultDnWorker().CancelAndWait(id);
}
void AsyncDnQuery_Cancel(DnQueryReqId id) {
   return GetDefaultDnWorker().Cancel(id);
}

} } // namespaces
