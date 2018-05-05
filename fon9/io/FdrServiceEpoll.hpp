/// \file fon9/io/FdrServiceEpoll.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_FdrServiceEpoll_hpp__
#define __fon9_io_FdrServiceEpoll_hpp__
#ifdef __linux__
#include "fon9/io/FdrService.hpp"
#include "fon9/DyObjsPool.hpp"

#include "fon9/ConfigPush.hpp"
namespace fon9 { namespace io {

fon9_MSC_WARN_DISABLE(4820);
/// \ingroup io
/// 提供使用 epoll 處理 non-blocking fd 讀寫事件服務.
class FdrThreadEpoll : public FdrThread {
   struct EvHandler : public FdrEventHandlerSP {
      using FdrEventHandlerSP::FdrEventHandlerSP;
      FdrEventFlag  Events_{};
   };
   using EvHandlers = DyObjsPool<EvHandler>;
   void ProcessPendings(Fdr::fdr_t epFd, EvHandlers& evHandlers);
   void ThrRun(FdrAuto&& epFd, ServiceThrArgs args);
public:
   FdrThreadEpoll(const ServiceThrArgs& args, ResultFuncSysErr& res);
   virtual ~FdrThreadEpoll();

   /// 如果 epoll 建立失敗, 則會寫入 log, 並透過 soRes 告知.
   static FdrServiceSP CreateFdrService(const IoServiceArgs& ioArgs, ResultFuncSysErr& res, std::string thrName);
};

} } // namespaces
#include "fon9/ConfigPop.hpp"
#endif
#endif//__fon9_io_FdrServiceEpoll_hpp__
