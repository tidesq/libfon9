/// \file fon9/io/FdrTcpServer.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_FdrTcpServer_hpp__
#define __fon9_io_FdrTcpServer_hpp__
#include "fon9/io/FdrSocket.hpp"
#include "fon9/io/TcpServerBase.hpp"

namespace fon9 { namespace io {

class FdrTcpListener;

/// \ingroup io
/// TcpServer 使用 Fdr 機制.
using FdrTcpServer = TcpServerBase<FdrTcpListener, FdrServiceSP>;
using FdrTcpServerSP = DeviceSPT<FdrTcpServer>;

class fon9_API FdrTcpListener : public DeviceListener, public FdrEventHandler {
   fon9_NON_COPY_NON_MOVE(FdrTcpListener);
   using baseCounter = DeviceListener;
   class AcceptedClient;

   FdrTcpListener(FdrTcpServerSP&& server, Socket&& soListen);
   virtual void OnListener_Dispose() override;

   virtual FdrEventFlag GetRequiredFdrEventFlag() const override;
   virtual void OnFdrEvent_Handling(FdrEventFlag evs) override;
   virtual void OnFdrEvent_AddRef() override;
   virtual void OnFdrEvent_ReleaseRef() override;
   virtual void OnFdrEvent_StartSend() override;

   /// 必須啟動 dev.CommonTimerRunAfter(); 才會有此事件.
   void OnTcpServer_OnCommonTimer() {}
   friend FdrTcpServer;

public:
   const FdrTcpServerSP   Server_;
   static DeviceListenerSP CreateListener(FdrTcpServerSP server, SocketResult& soRes);
};

} } // namespaces
#endif//__fon9_io_FdrTcpServer_hpp__
