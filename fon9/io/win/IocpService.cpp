/// \file fon9/io/IocpService.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/win/IocpService.hpp"
#include "fon9/ThreadTools.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace io {

static const ULONG_PTR  kIocpKey_StopThrRun = 0;

IocpHandler::~IocpHandler() {
}
IocpHandler::Result IocpHandler::IocpAttach(HANDLE handle) {
   if (CreateIoCompletionPort(handle, this->IocpService_->CompletionPort_->GetFD(), reinterpret_cast<ULONG_PTR>(this), 0))
      return Result::kSuccess();
   return Result{GetSysErrC()};
}
DWORD IocpHandler::Post(LPOVERLAPPED lpOverlapped, DWORD dwNumberOfBytesTransferred) {
   if (::PostQueuedCompletionStatus(this->IocpService_->CompletionPort_->GetFD(), dwNumberOfBytesTransferred, reinterpret_cast<ULONG_PTR>(this), lpOverlapped))
      return 0;
   DWORD eno = GetLastError();
   this->OnIocp_Error(lpOverlapped, eno);
   return eno;
}

//--------------------------------------------------------------------------//

IocpService::~IocpService() {
   this->StopAndWait();
}
void IocpService::StopAndWait() {
   for (size_t L = this->Threads_.size() * 2; L > 0; --L)
      ::PostQueuedCompletionStatus(this->CompletionPort_->GetFD(), 0, kIocpKey_StopThrRun, nullptr);
   JoinOrDetach(this->Threads_);
}
void IocpService::ThrRun(CompletionPortHandleSP cpHandleSP, ServiceThreadArgs args) {
   args.OnThrRunBegin("IocpService");
   OVERLAPPED* lpOverlapped;
   DWORD       bytesTransfered;
   const DWORD dwMilliseconds = (IsBlockWait(args.HowWait_) ? INFINITE : 0);
   HANDLE      cpHandle = cpHandleSP->GetFD();
   for (;;) {
      ULONG_PTR   iocpHandler = kIocpKey_StopThrRun;
      if (::GetQueuedCompletionStatus(cpHandle, &bytesTransfered, &iocpHandler, &lpOverlapped, dwMilliseconds)) {
         if (iocpHandler == kIocpKey_StopThrRun)
            break;
         reinterpret_cast<IocpHandler*>(iocpHandler)->OnIocp_Done(lpOverlapped, bytesTransfered);
      }
      else {
         if (lpOverlapped == nullptr) { // timeout.
            if (args.HowWait_ == HowWait::Yield)
               std::this_thread::yield();
            continue;
         }
         DWORD eno = GetLastError();
         if (iocpHandler)
            reinterpret_cast<IocpHandler*>(iocpHandler)->OnIocp_Error(lpOverlapped, eno);
         else {
            // CompletionPort 本身的錯誤??
            fon9_LOG_FATAL("IocpService.ThrRun"
                           "|iocpHandler=nullptr"
                           "|overlapped=", ToPtr{lpOverlapped},
                           "|bytesTransfered=", bytesTransfered,
                           "|err=", GetSysErrC(eno));
            break;
         }
      }
   }
   fon9_LOG_ThrRun("IocpService.ThrRun.End");
}

//--------------------------------------------------------------------------//

IocpServiceSP IocpService::MakeService(const IoServiceArgs& args, const std::string& thrName, MakeResult& err) {
   if(HANDLE cpHandle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)) {
      IocpServiceSP iosv{new IocpService};
      iosv->CompletionPort_ = MakeObjHolder<FdrAuto>(cpHandle);
      size_t thrCount = args.ThreadCount_;
      if (thrCount <= 0)
         thrCount = 1;
      iosv->Threads_.reserve(thrCount);
      for (size_t L = 0; L < thrCount; ++L)
         iosv->Threads_.emplace_back(&IocpService::ThrRun, iosv->CompletionPort_, ServiceThreadArgs{args, thrName, L});
      iosv->Threads_.shrink_to_fit();
      return iosv;
   }
   err = MakeResult{"CreateIoCompletionPort", GetSysErrC()};
   return IocpServiceSP{};
}

} } // namespaces
