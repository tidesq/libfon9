/// \file fon9/io/Device.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/Device.hpp"
#include "fon9/TimeStamp.hpp"
#include "fon9/DefaultThreadPool.hpp"

namespace fon9 { namespace io {

Device::~Device() {
   this->State_ = State::Destructing;
   this->Session_->OnDevice_Destructing(*this);
   if (this->Manager_)
      this->Manager_->OnDevice_Destructing(*this);
}

void Device::Initialize() {
   assert(this->State_ == State::Initializing);
   this->Session_->OnDevice_Initialized(*this);
   if (this->Manager_)
      this->Manager_->OnDevice_Initialized(*this);
   this->State_ = State::Initialized;
}

void Device::MakeCallForWork() {
   intrusive_ptr_add_ref(this);
   GetDefaultThreadPool().EmplaceMessage([this]() {
      this->OpQueue_.TakeCall();
      intrusive_ptr_release(this);
   });
}

void Device::OpThr_Open(std::string cfgstr) {
   State st = this->State_;
   if (st >= State::Disposing)
      return;
   if (!cfgstr.empty()) // force reopen, use new config(cfgstr)
      return this->OpImpl_Open(std::move(cfgstr));
   // 沒有 cfgstr, 檢查現在狀態是否允許 reopen.
   if (st == State::Listening ||
       st == State::LinkReady ||
       st == State::WaitingLinkIn ||
       st == State::ConfigError)
      return;
   this->OpImpl_Reopen();
}

void Device::OpThr_Close(std::string cause) {
   if (this->State_ < State::Closing)
      this->OpImpl_Close(std::move(cause));
}
void Device::OpThr_LingerClose(std::string cause) {
   if (this->State_ == State::LinkReady) {
      this->OpThr_SetState(State::Lingering, &cause);
      if (this->IsSendBufferEmpty() && this->State_ == State::Lingering)
         this->OpImpl_Close(std::move(cause));
   }
   else {
      if (this->State_ < State::Lingering)
         this->OpImpl_Close(std::move(cause));
   }
}

void Device::OpImpl_Dispose(std::string cause) {
   (void)cause;
}
void Device::OpThr_Dispose(std::string cause) {
   this->OpThr_Close(cause);
   if (this->OpThr_SetState(State::Disposing, &cause))
      this->OpImpl_Dispose(std::move(cause));
}

void Device::OpThr_StartRecv(RecvBufferSize preallocSize) {
   (void)preallocSize;
}
void Device::OpThr_SetLinkReady(std::string stmsg) {
   if (!this->OpThr_SetState(State::LinkReady, &stmsg))
      return;
   // 避免在處理 OnDevice_StateChanged() 之後 State_ 被改變(例:特殊情況下關閉了 Device),
   // 所以在此再判斷一次 State_, 必需仍為 LinkReady, 才需觸發 OnDevice_LinkReady() 事件.
   if (this->State_ == State::LinkReady) {
      RecvBufferSize preallocSize = this->Session_->OnDevice_LinkReady(*this);
      if (preallocSize == RecvBufferSize::NoRecvEvent)
         this->CommonOptions_ |= DeviceCommonOption::NoRecvEvent;
      else
         this->CommonOptions_ -= DeviceCommonOption::NoRecvEvent;
      this->OpThr_StartRecv(preallocSize);
   }
}
void Device::OpThr_SetBrokenState(std::string cause) {
   switch (this->State_) {
   case State::Initializing:
   case State::Initialized:
   case State::ConfigError:
      //if (this->State_ < State::Opening   //尚未開啟
   case State::Closing:
   case State::Closed:
   case State::Disposing:
   case State::Destructing:
      // || State::Closing <= this->State_) //已經關閉(or 關閉中)
      // 不理會 Broken 狀態.
   case State::LinkError:
   case State::LinkBroken:
   case State::ListenBroken:
      // 已經是 Broken or Error, 不處理 Broken 狀態.
      return;

   case State::Lingering:
      this->OpThr_Close(std::move(cause));
      break;
   case State::Opening:
   case State::WaitingLinkIn:
   case State::Linking:
      //if (State::Opening <= this->State_ && this->State_ <= State::Linking)
      this->OpThr_SetState(State::LinkError, &cause);
      break;
   case State::LinkReady:
      this->OpThr_SetState(State::LinkBroken, &cause);
      break;
   case State::Listening:
      this->OpThr_SetState(State::ListenBroken, &cause);
      break;
   }
}

void Device::OpImpl_StateChanged(const StateChangedArgs& e) {
   (void)e;
}
bool Device::OpThr_SetState(State afst, StrView stmsg) {
   State bfst = this->State_;
   if (bfst == afst) {
      if (afst < State::Disposing) { // Disposing 訊息不需要重複更新.
         StateUpdatedArgs e{afst, stmsg};
         this->Session_->OnDevice_StateUpdated(*this, e);
         if (this->Manager_)
            this->Manager_->OnDevice_StateUpdated(*this, e);
      }
      return false;
   }
   if (bfst >= State::Disposing && afst < bfst)
      // 狀態 >= State::Disposing 之後, 就不可再回頭!
      return false;
   this->State_ = afst;
   StateChangedArgs e{stmsg, this->DeviceId_};
   e.Before_ = bfst;
   e.After_ = afst;
   this->OpImpl_StateChanged(e);
   this->Session_->OnDevice_StateChanged(*this, e);
   if (this->Manager_)
      this->Manager_->OnDevice_StateChanged(*this, e);
   return true;
}

std::string Device::WaitGetDeviceId() {
   std::string res;
   this->OpQueue_.WaitInvoke(AQueueTaskKind::Get,
                             DeviceAsyncOp{[&res](Device& dev) {
      res = dev.DeviceId_;
   }});
   return res;
}

void Device::OpImpl_AppendDeviceInfo(std::string& info) {
   (void)info;
}
std::string Device::WaitGetDeviceInfo() {
   std::string res;
   res.reserve(300);
   res.append("|tm=");
   this->OpQueue_.WaitInvoke(AQueueTaskKind::Get,
                             DeviceAsyncOp{[&res](Device& dev) {
      if (const char* tmstr = ToStrRev_Full(UtcNow()))
         res.append(tmstr, kDateTimeStrWidth);
      res.append("|st=");
      GetStateStr(dev.State_).AppendTo(res);
      res.append("|id={");
      res.append(dev.DeviceId_);
      res.append("}|info=");
      dev.OpImpl_AppendDeviceInfo(res);
   }});
   return res;
}

std::string Device::WaitSetProperty(StrView strTagValue) {
   CountDownLatch waiter{1};
   std::string    res;
   this->OpQueue_.AddTask(DeviceAsyncOp{[&](Device& dev) {
      res = dev.OpImpl_SetPropertyList(strTagValue);
      waiter.ForceWakeUp();
   }});
   waiter.Wait();
   return res;
}
std::string Device::OpImpl_SetPropertyList(StrView propList) {
   StrView tag, value;
   while (FetchTagValue(propList, tag, value)) {
      std::string res = this->OpImpl_SetProperty(tag, value);
      if (!res.empty())
         return res;
   }
   return std::string();
}
std::string Device::OpImpl_SetProperty(StrView tag, StrView value) {
   if (tag == "SendASAP") {
      if (toupper(value.Get1st()) == 'N')
         this->CommonOptions_ -= DeviceCommonOption::SendASAP;
      else
         this->CommonOptions_ |= DeviceCommonOption::SendASAP;
      return std::string();
   }
   return tag.ToString("unknown property name:");
}

std::string Device::OnDevice_Command(StrView cmd, StrView param) {
   if (cmd == "open")
      this->AsyncOpen(param.ToString());
   else if (cmd == "close")
      this->AsyncClose(param.ToString("DeviceCommand.close:"));
   else if (cmd == "lclose")
      this->AsyncLingerClose(param.ToString("DeviceCommand.lclose:"));
   else if (cmd == "dispose")
      this->AsyncDispose(param.ToString("DeviceCommand.dispose:"));
   else if (cmd == "info")
      return this->WaitGetDeviceInfo();
   else if (cmd == "set")
      return this->WaitSetProperty(param);
   else if (cmd == "ses")
      return this->Session_->SessionCommand(*this, param);
   else return std::string{"unknown device command"};
   return std::string();
}
std::string Device::DeviceCommand(StrView cmdln) {
   StrView cmd = StrFetchTrim(cmdln, &isspace);
   return this->OnDevice_Command(cmd, StrTrim(&cmdln));
}

} } // namespaces
