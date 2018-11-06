/// \file fon9/io/win/IocpDgram.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_win_IocpDgram_hpp__
#define __fon9_io_win_IocpDgram_hpp__
#include "fon9/io/win/IocpSocketClient.hpp"
#include "fon9/io/DgramBase.hpp"

namespace fon9 { namespace io {

fon9_WARN_DISABLE_PADDING;
struct fon9_API IocpDgramImpl : public IocpSocketClientImpl {
   fon9_NON_COPY_NON_MOVE(IocpDgramImpl);
   using base = IocpSocketClientImpl;

public:
   using OwnerDevice = DgramT<IocpServiceSP, IocpDgramImpl>;
   using OwnerDeviceSP = intrusive_ptr<OwnerDevice>;
   const OwnerDeviceSP  Owner_;

   IocpDgramImpl(OwnerDevice* owner, Socket&& so, SocketResult& soRes)
      : base(owner->IoService_, std::move(so), soRes)
      , Owner_{owner} {
   }

   void OpImpl_Close();
   bool OpImpl_ConnectTo(const SocketAddress& addr, SocketResult& soRes);

   virtual void OnIocpSocket_Received(DcQueueList& rxbuf) override;
   virtual void OnIocpSocket_Writable(DWORD bytesTransfered) override;
   virtual void OnIocpSocket_Error(OVERLAPPED* lpOverlapped, DWORD eno) override;
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

/// \ingroup io
/// Windows Dgram(UDP,Multicast) Device 使用 IOCP.
using IocpDgram = DeviceImpl_DeviceStartSend<IocpDgramImpl::OwnerDevice, IocpSocket>;

} } // namespaces
#endif//__fon9_io_win_IocpDgram_hpp__
