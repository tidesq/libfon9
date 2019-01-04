/// \file fon9/io/IocpService.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_win_IocpService_hpp__
#define __fon9_io_win_IocpService_hpp__
#include "fon9/io/IoBase.hpp"
#include "fon9/io/IoServiceArgs.hpp"
#include "fon9/Outcome.hpp"
#include "fon9/Fdr.hpp"

fon9_BEFORE_INCLUDE_STD;
#include <WinSock2.h>
fon9_AFTER_INCLUDE_STD;

namespace fon9 { namespace io {

class fon9_API IocpService;
using IocpServiceSP = intrusive_ptr<IocpService>;

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// 透過 Windows IOCP 處理的 要求端, 當IOCP完成或發現錯誤時透過這兒通知.
/// GetQueuedCompletionStatus() 的 lpCompletionKey;
class fon9_API IocpHandler {
   fon9_NON_COPY_NON_MOVE(IocpHandler);

   friend class IocpService;
   /// 當 IOCP 偵測到錯誤時通知.
   virtual void OnIocp_Error(OVERLAPPED* lpOverlapped, DWORD eno) = 0;
   /// 當 IOCP 作業完成時通知.
   virtual void OnIocp_Done(OVERLAPPED* lpOverlapped, DWORD bytesTransfered) = 0;

protected:
   using Result = Result3;

   /// 將 handle 與 IOCP 建立關聯.
   /// 關聯建立成功後: 當使用 Overlapped IO 就可以在 IOCP 的 thread pool 之中收到 IOCP 的通知.
   Result IocpAttach(HANDLE handle);

   Result IocpAttach(SOCKET so) {
      return this->IocpAttach(reinterpret_cast<HANDLE>(so));
   }

   /// \retval ==0 成功.
   /// \retval !=0 Post() 失敗, 則會立即呼叫 OnIocp_Error(); 返回 GetLastError();
   DWORD Post(LPOVERLAPPED lpOverlapped, DWORD dwNumberOfBytesTransferred);

public:
   const IocpServiceSP  IocpService_;

   IocpHandler(IocpServiceSP iocpSv) : IocpService_{std::move(iocpSv)} {
   }
   virtual ~IocpHandler();
};

/// \ingroup io
/// Windows IOCP 服務核心.
class fon9_API IocpService : public intrusive_ref_counter<IocpService> {
   fon9_NON_COPY_NON_MOVE(IocpService);
   using ThreadPool = std::vector<std::thread>;
   ThreadPool  Threads_;

   friend class IocpHandler;
   using CompletionPortHandleSP = intrusive_ptr<ObjHolder<FdrAuto>>;
   CompletionPortHandleSP  CompletionPort_;

   IocpService() = default;

public:
   using MakeResult = Result2;
   static IocpServiceSP MakeService(const IoServiceArgs& args, const std::string& thrName, MakeResult& err);

   virtual ~IocpService();
   
private:
   void StopAndWait();
   static void ThrRun(CompletionPortHandleSP cpHandleSP, ServiceThreadArgs args);
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_win_IocpService_hpp__
