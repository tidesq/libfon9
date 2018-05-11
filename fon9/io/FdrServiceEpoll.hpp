/// \file fon9/io/FdrServiceEpoll.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_FdrServiceEpoll_hpp__
#define __fon9_io_FdrServiceEpoll_hpp__
#ifdef __linux__
#include "fon9/io/FdrService.hpp"
#include "fon9/ObjPool.hpp"

namespace fon9 { namespace io {

struct FdrServiceEpoll {
   using MakeResult = Result2;
   static FdrServiceSP MakeService(const IoServiceArgs& ioArgs, const std::string& thrName, MakeResult& err);
};

/// \ingroup io
/// 提供使用 epoll 處理 non-blocking fd 讀寫事件服務.
class FdrThreadEpoll : public FdrThread {
   const FdrAuto  FdrEpoll_;
   struct EvHandler : public FdrEventHandlerSP {
      using FdrEventHandlerSP::FdrEventHandlerSP;
      FdrEventFlag  Events_{FdrEventFlag::None};
   };
   using EvHandlers = ObjPool<EvHandler>;

   void ProcessPendings(Fdr::fdr_t epFdr, EvHandlers& evHandlers);
   virtual void ThrRunImpl(const ServiceThreadArgs& args) override;

public:
   FdrThreadEpoll(FdrServiceEpoll::MakeResult& res);

   virtual ~FdrThreadEpoll();
};

} } // namespaces
#endif//__linux__
#endif//__fon9_io_FdrServiceEpoll_hpp__
