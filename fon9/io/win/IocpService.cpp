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
   if (CreateIoCompletionPort(handle, this->IocpService_->CompletionPort_, reinterpret_cast<ULONG_PTR>(this), 0))
      return Result::kSuccess();
   return Result{GetSysErrC()};
}
bool IocpHandler::IocpDetach() {
   return true;
}
//void IocpHandler::Post(LPOVERLAPPED lpOverlapped, DWORD dwNumberOfBytesTransferred) {
//   if (!::PostQueuedCompletionStatus(this->IocpService_->CompletionPort_, dwNumberOfBytesTransferred, reinterpret_cast<ULONG_PTR>(this), lpOverlapped))
//      this->OnIocp_Error(lpOverlapped, GetLastError());
//}

//--------------------------------------------------------------------------//

IocpService::~IocpService() {
   this->StopAndWait();
   ::CloseHandle(this->CompletionPort_);
}
void IocpService::StopAndWait() {
   for (size_t L = this->Threads_.size() * 2; L > 0; --L)
      ::PostQueuedCompletionStatus(this->CompletionPort_, 0, kIocpKey_StopThrRun, nullptr);
   JoinThreads(this->Threads_);
}
void IocpService::ThrRun(ServiceThreadArgs args) {
   args.OnThrRunBegin("IocpService");
   OVERLAPPED* lpOverlapped;
   DWORD       bytesTransfered;
   const DWORD dwMilliseconds = (IsBlockWait(args.HowWait_) ? INFINITE : 0);
   for (;;) {
      ULONG_PTR   iocpHandler = kIocpKey_StopThrRun;
      if (::GetQueuedCompletionStatus(this->CompletionPort_, &bytesTransfered, &iocpHandler, &lpOverlapped, dwMilliseconds)) {
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
            // this->CompletionPort_ 本身的錯誤.
            if (eno == ERROR_INVALID_HANDLE)
               break;
            fon9_LOG_ERROR("IocpService.ThrRun"
                           "|overlapped=", ToPtr{lpOverlapped},
                           "|err=", GetSysErrC(eno));
         }
      }
   }
   fon9_LOG_ThrRun("IocpService.ThrRun.End");
}

//--------------------------------------------------------------------------//

IocpServiceSP IocpService::MakeService(const IoServiceArgs& args, const std::string& thrName, MakeResult& err) {
   if(HANDLE cpHandle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)) {
      IocpServiceSP iosv{new IocpService};
      iosv->CompletionPort_ = cpHandle;
      size_t thrCount = args.ThreadCount_;
      if (thrCount <= 0)
         thrCount = 1;
      iosv->Threads_.reserve(thrCount);
      for (size_t L = 0; L < thrCount; ++L)
         iosv->Threads_.emplace_back(&IocpService::ThrRun, iosv.get(), ServiceThreadArgs{args, thrName, L});
      iosv->Threads_.shrink_to_fit();
      return iosv;
   }
   err = MakeResult{"CreateIoCompletionPort", GetSysErrC()};
   return IocpServiceSP{};
}

} } // namespaces
