/// \file fon9/io/TcpClientBase.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/TcpClientBase.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 { namespace io {

//--------------------------------------------------------------------------//

TcpClientBase::~TcpClientBase() {
   this->ConnectTimer_.StopAndWait();
}

void TcpClientBase::OpThr_TcpClearLinking() {
   AsyncDnQuery_CancelAndWait(&this->DnReqId_);
   ZeroStruct(this->RemoteAddress_);
   this->NextAddrIndex_ = 0;
   this->AddrList_.clear();
   this->ConnectTimer_.StopNoWait();
}
void TcpClientBase::OnConnectTimeout(TimerEntry* timer, TimeStamp now) {
   (void)now;
   TcpClientBase& rthis = ContainerOf(*static_cast<Timer*>(timer), &TcpClientBase::ConnectTimer_);
   rthis.AddAsyncTask([](Device& dev) {
      TcpClientBase*  pthis = static_cast<TcpClientBase*>(&dev);
      if (pthis->DnReqId_) {
         AsyncDnQuery_CancelAndWait(&pthis->DnReqId_);
         pthis->OpThr_SetBrokenState("DN query timeout.");
      }
      else if (pthis->OpThr_GetState() == State::Linking)
         pthis->OpThr_ConnectToNext("Connect timeout.");
   });
}
void TcpClientBase::OpThr_ConnectToNext(StrView lastError) {
   if (this->NextAddrIndex_ >= this->AddrList_.size()) {
      // 全部的 addr 都嘗試過, 才進入 LinkError 狀態.
      this->NextAddrIndex_ = 0;
      if (lastError.empty())
         lastError = (this->AddrList_.empty() ? StrView{"No hosts."} : StrView{"All hosts cannot connect."});
      this->OpThr_SetState(State::LinkError, lastError);
      return;
   }
   const SocketAddress& addr = this->AddrList_[this->NextAddrIndex_++];
   std::string strmsg{"Connecting to: "};
   char        strbuf[SocketAddress::kMaxAddrPortStrSize];
   strmsg.append(strbuf, addr.ToAddrPortStr(strbuf));
   Socket       soCli;
   SocketResult soRes;
   if(soCli.CreateDeviceSocket(addr.GetAF(), SocketType::Stream, soRes)
   && soCli.SetSocketOptions(this->Config_.Options_, soRes)
   && soCli.Bind(this->Config_.AddrBind_, soRes)) {
      if (!lastError.empty()) {
         if(this->NextAddrIndex_ == 1)
            strmsg.append("|prev.err=");
         else {
            strmsg.append("|prev=");
            strmsg.append(strbuf, this->RemoteAddress_.ToAddrPortStr(strbuf));
            strmsg.append("|err=");
         }
         lastError.AppendTo(strmsg);
      }
      this->StartConnectTimer();
      this->OpThr_SetState(State::Linking, &strmsg);
      strmsg.clear();
      this->RemoteAddress_ = addr;
      if (this->OpThr_ConnectToImpl(std::move(soCli), soRes))
         return;
   }
   else
      strmsg.push_back('|');
   RevPrintAppendTo(strmsg, soRes);
   this->OpThr_SetState(State::LinkError, &strmsg);
}
void TcpClientBase::OpImpl_Open(std::string cfgstr) {
   this->OpThr_TcpClearLinking();
   if (OpThr_ParseDeviceConfig(*this, &cfgstr, FnOnTagValue{}))
      this->OpThr_ReopenImpl();
}
void TcpClientBase::OpImpl_Reopen() {
   this->OpThr_TcpClearLinking();
   this->OpThr_ReopenImpl();
}
void TcpClientBase::OpThr_OnDnQueryDone(DnQueryReqId id, const DomainNameParseResult& res) {
   if (this->DnReqId_ != id)
      return;
   this->DnReqId_ = 0;
   this->AddrList_.insert(this->AddrList_.end(), res.AddressList_.begin(), res.AddressList_.end());
   this->OpThr_ConnectToNext(&res.ErrMsg_);
}
void TcpClientBase::OpThr_ReopenImpl() {
   if (!this->Config_.AddrRemote_.IsAddrAny())
      this->AddrList_.emplace_back(this->Config_.AddrRemote_);
   if (!this->Config_.DomainNames_.empty()) {
      this->OpThr_SetState(State::Linking, ToStrView("DN querying: " + this->Config_.DomainNames_));
      this->StartConnectTimer();
      DeviceSP pthis{this};
      AsyncDnQuery(this->DnReqId_, this->Config_.DomainNames_, this->Config_.AddrRemote_.GetPort(),
                   [pthis](DnQueryReqId id, DomainNameParseResult& res) {
         pthis->AddAsyncTask([id, res](Device& dev) {
            static_cast<TcpClientBase*>(&dev)->OpThr_OnDnQueryDone(id, res);
         });
      });
      return;
   }
   if (this->AddrList_.empty())
      this->OpThr_SetState(State::ConfigError, "Config error: no remote address.");
   else
      this->OpThr_ConnectToNext(StrView{});
}
void TcpClientBase::OpThr_Connected(const Socket& soCli) {
   this->ConnectTimer_.StopNoWait();
   auto errc = soCli.LoadSocketErrC();
   if (errc) {
      this->OpThr_SetBrokenState(RevPrintTo<std::string>("err=", errc));
      return;
   }
   SocketAddress  addrLocal;
   socklen_t      addrLen = sizeof(addrLocal);
   getsockname(soCli.GetSocketHandle(), &addrLocal.Addr_, &addrLen);
   char     uidbuf[kMaxTcpConnectionUID];
   StrView  uidstr = MakeTcpConnectionUID(uidbuf, &this->RemoteAddress_, &addrLocal);
   if (this->RemoteAddress_ == addrLocal) {
      std::string errmsg{"err=self-connect|id="};
      uidstr.AppendTo(errmsg);
      this->OpThr_SetBrokenState(std::move(errmsg));
   }
   else {
      this->OpThr_SetDeviceId(uidstr.ToString());
      this->OpThr_SetLinkReady(std::string{});
   }
}
void TcpClientBase::OpImpl_Close(std::string cause) {
   this->OpThr_TcpClearLinking();
   this->OpThr_SetState(State::Closed, &cause);
   this->OpThr_SetDeviceId(std::string());
}
void TcpClientBase::OpImpl_StateChanged(const StateChangedArgs& e) {
   if (IsAllowContinueSend(e.Before_)) {
      if (e.After_ != State::Lingering)
         this->OpThr_TcpLinkBroken();
   }
   else if (e.Before_ == State::Linking && e.After_ != State::LinkReady)
      this->OpThr_TcpClearLinking();
   base::OpImpl_StateChanged(e);
}

} } // namespaces
