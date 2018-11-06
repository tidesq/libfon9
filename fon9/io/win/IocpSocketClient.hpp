/// \file fon9/io/win/IocpSocketClient.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_win_IocpSocketClient_hpp__
#define __fon9_io_win_IocpSocketClient_hpp__
#include "fon9/io/win/IocpSocket.hpp"

namespace fon9 { namespace io {

struct fon9_API IocpSocketClientImpl : public IocpSocket, public intrusive_ref_counter<IocpSocketClientImpl> {
   fon9_NON_COPY_NON_MOVE(IocpSocketClientImpl);

   virtual unsigned IocpSocketAddRef() override;
   virtual unsigned IocpSocketReleaseRef() override;

protected:
   enum class State {
      Idle,
      Closing,
      Connecting,
      Connected,
   };
   State State_{};

public:
   IocpSocketClientImpl(IocpServiceSP iosv, Socket&& so, SocketResult& soRes)
      : IocpSocket(std::move(iosv), std::move(so), soRes) {
   }
   bool IsClosing() const {
      return this->State_ == State::Closing;
   }
};

} } // namespaces
#endif//__fon9_io_win_IocpSocketClient_hpp__
