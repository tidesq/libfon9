/// \file fon9/io/FdrTcpClient.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_FdrTcpClient_hpp__
#define __fon9_io_FdrTcpClient_hpp__
#include "fon9/io/TcpClientBase.hpp"
#include "fon9/io/FdrSocket.hpp"

namespace fon9 { namespace io {

struct FdrTcpClientImpl : public FdrSocket, public intrusive_ref_counter<FdrTcpClientImpl> {
   fon9_NON_COPY_NON_MOVE(FdrTcpClientImpl);
   using baseCounter = intrusive_ref_counter<FdrTcpClientImpl>;
   bool  IsClosing_{false};
   bool  IsConnected_{false};

   virtual void OnFdrEvent_AddRef() override;
   virtual void OnFdrEvent_ReleaseRef() override;
   virtual void OnFdrEvent_Handling(FdrEventFlag evs) override;
   virtual void OnFdrEvent_StartSend() override;
   virtual void OnFdrSocket_Error(std::string errmsg) override;

public:
   using OwnerDevice = TcpClientT<FdrServiceSP, FdrTcpClientImpl>;
   using OwnerDeviceSP = intrusive_ptr<OwnerDevice>;
   const OwnerDeviceSP  Owner_;

   FdrTcpClientImpl(OwnerDevice* owner, Socket&& so, SocketResult&)
      : FdrSocket{*owner->IoService_, std::move(so)}
      , Owner_{owner} {
   }

   bool IsClosing() const {
      return this->IsClosing_;
   }
   void OpImpl_Close();
   bool OpImpl_ConnectTo(const SocketAddress& addr, SocketResult& soRes);
};

//--------------------------------------------------------------------------//

/// \ingroup io
/// 使用 fd 的實作的 TcpClient.
using FdrTcpClient = DeviceImpl_DeviceStartSend<FdrTcpClientImpl::OwnerDevice, FdrSocket>;

} } // namespaces
#endif//__fon9_io_FdrTcpClient_hpp__
