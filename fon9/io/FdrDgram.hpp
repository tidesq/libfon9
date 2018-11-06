/// \file fon9/io/FdrDgram.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_FdrDgram_hpp__
#define __fon9_io_FdrDgram_hpp__
#include "fon9/io/DgramBase.hpp"
#include "fon9/io/FdrSocketClient.hpp"

namespace fon9 { namespace io {

struct FdrDgramImpl : public FdrSocketClientImpl {
   fon9_NON_COPY_NON_MOVE(FdrDgramImpl);
   using base = FdrSocketClientImpl;
   virtual void OnFdrEvent_Handling(FdrEventFlag evs) override;
   virtual void OnFdrEvent_StartSend() override;
   virtual void OnFdrSocket_Error(std::string errmsg) override;
   virtual void SocketError(StrView fnName, int eno) override;

public:
   using OwnerDevice = DgramT<FdrServiceSP, FdrDgramImpl>;
   using OwnerDeviceSP = intrusive_ptr<OwnerDevice>;
   const OwnerDeviceSP  Owner_;

   FdrDgramImpl(OwnerDevice* owner, Socket&& so, SocketResult&)
      : base{*owner->IoService_, std::move(so)}
      , Owner_{owner} {
   }
   bool OpImpl_ConnectTo(const SocketAddress& addr, SocketResult& soRes);
};

//--------------------------------------------------------------------------//

/// \ingroup io
/// 使用 fd 的實作的 Dgram.
using FdrDgram = DeviceImpl_DeviceStartSend<FdrDgramImpl::OwnerDevice, FdrSocket>;

} } // namespaces
#endif//__fon9_io_FdrDgram_hpp__
