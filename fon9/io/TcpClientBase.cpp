/// \file fon9/io/TcpClientBase.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/TcpClientBase.hpp"
#include "fon9/RevPrint.hpp"
#include "fon9/Log.hpp"//todo: remove.

namespace fon9 { namespace io {

//--------------------------------------------------------------------------//

TcpClientBase::~TcpClientBase() {
   this->ConnectTimer_.StopAndWait();
}

void TcpClientBase::OpImpl_TcpClearLinking() {
   AsyncDnQuery_CancelAndWait(&this->DnReqId_);
   ZeroStruct(this->RemoteAddress_);
   this->NextAddrIndex_ = 0;
   this->AddrList_.clear();
   this->ConnectTimer_.StopNoWait();
}
void TcpClientBase::OnConnectTimeout(TimerEntry* timer, TimeStamp now) {
   (void)now;
   TcpClientBase& rthis = ContainerOf(*static_cast<Timer*>(timer), &TcpClientBase::ConnectTimer_);
   rthis.OpQueue_.AddTask(DeviceAsyncOp{[](Device& dev) {
      TcpClientBase*  pthis = static_cast<TcpClientBase*>(&dev);
      if (pthis->DnReqId_) {
         AsyncDnQuery_CancelAndWait(&pthis->DnReqId_);
         OpThr_SetBrokenState(dev, "DN query timeout.");
      }
      else if (pthis->OpImpl_GetState() == State::Linking)
         pthis->OpImpl_ConnectToNext("Connect timeout.");
   }});
}
void TcpClientBase::OpImpl_ConnectToNext(StrView lastError) {
   OpThr_SetDeviceId(*this, std::string{});
   if (this->NextAddrIndex_ >= this->AddrList_.size()) {
      // 全部的 addr 都嘗試過, 才進入 LinkError 狀態.
      this->NextAddrIndex_ = 0;
      if (lastError.empty())
         lastError = (this->AddrList_.empty() ? StrView{"No hosts."} : StrView{"All hosts cannot connect."});
      this->OpImpl_SetState(this->OpImpl_GetState() == State::LinkReady
                            ? State::LinkBroken : State::LinkError, lastError);
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
      this->OpImpl_SetState(State::Linking, &strmsg);
      strmsg.clear();
      this->RemoteAddress_ = addr;
      if (this->OpImpl_TcpConnect(std::move(soCli), soRes))
         return;
   }
   else
      strmsg.push_back('|');
   RevPrintAppendTo(strmsg, soRes);
   this->OpImpl_SetState(State::LinkError, &strmsg);
}
void TcpClientBase::OpImpl_Open(std::string cfgstr) {
   this->OpImpl_TcpClearLinking();
   if (OpThr_ParseDeviceConfig(*this, &cfgstr, FnOnTagValue{}))
      this->OpImpl_ReopenImpl();
}
void TcpClientBase::OpImpl_Reopen() {
   this->OpImpl_TcpClearLinking();
   this->OpImpl_ReopenImpl();
}
void TcpClientBase::OpImpl_OnDnQueryDone(DnQueryReqId id, const DomainNameParseResult& res) {
   if (this->DnReqId_ != id)
      return;
   this->DnReqId_ = 0;
   this->AddrList_.insert(this->AddrList_.end(), res.AddressList_.begin(), res.AddressList_.end());
   this->OpImpl_ConnectToNext(&res.ErrMsg_);
}
void TcpClientBase::OpImpl_ReopenImpl() {
   if (!this->Config_.AddrRemote_.IsAddrAny())
      this->AddrList_.emplace_back(this->Config_.AddrRemote_);
   if (!this->Config_.DomainNames_.empty()) {
      this->OpImpl_SetState(State::Linking, ToStrView("DN querying: " + this->Config_.DomainNames_));
      this->StartConnectTimer();
      DeviceSP pthis{this};
      AsyncDnQuery(this->DnReqId_, this->Config_.DomainNames_, this->Config_.AddrRemote_.GetPort(),
                   [pthis](DnQueryReqId id, DomainNameParseResult& res) {
         pthis->OpQueue_.AddTask(DeviceAsyncOp{[id, res](Device& dev) {
            static_cast<TcpClientBase*>(&dev)->OpImpl_OnDnQueryDone(id, res);
         }});
      });
      return;
   }
   if (this->AddrList_.empty())
      this->OpImpl_SetState(State::ConfigError, "Config error: no remote address.");
   else
      this->OpImpl_ConnectToNext(StrView{});
}
void TcpClientBase::OpImpl_Connected(Socket::socket_t so) {
   this->ConnectTimer_.StopNoWait();
   auto errc = Socket::LoadSocketErrC(so);
   if (errc) {
      OpThr_SetBrokenState(*this, RevPrintTo<std::string>("err=", errc));
      return;
   }
   SocketAddress  addrLocal;
   socklen_t      addrLen = sizeof(addrLocal);
   getsockname(so, &addrLocal.Addr_, &addrLen);
   char     uidbuf[kMaxTcpConnectionUID];
   StrView  uidstr = MakeTcpConnectionUID(uidbuf, &this->RemoteAddress_, &addrLocal);
   if (this->RemoteAddress_ == addrLocal) {
      std::string errmsg{"err=self-connect|id="};
      uidstr.AppendTo(errmsg);
      OpThr_SetBrokenState(*this, std::move(errmsg));
   }
   else {
      OpThr_SetDeviceId(*this, uidstr.ToString());
      OpThr_SetLinkReady(*this, std::string{});
   }
}
void TcpClientBase::OpImpl_Close(std::string cause) {
   this->OpImpl_TcpClearLinking();
   this->OpImpl_SetState(State::Closed, &cause);
   OpThr_SetDeviceId(*this, std::string{});
}
void TcpClientBase::OpImpl_StateChanged(const StateChangedArgs& e) {
   if (IsAllowContinueSend(e.Before_)) {
      if (e.After_ != State::Lingering)
         this->OpImpl_TcpLinkBroken();
   }
   else if (e.Before_ == State::Linking && e.After_ != State::LinkReady)
      this->OpImpl_TcpClearLinking();
   base::OpImpl_StateChanged(e);
}

} } // namespaces
