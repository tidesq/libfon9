/// \file fon9/io/FdrTcpClient.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_FdrTcpClient_hpp__
#define __fon9_io_FdrTcpClient_hpp__
#include "fon9/io/TcpClientBase.hpp"
#include "fon9/io/SendBuffer.hpp"
#include "fon9/io/RecvBuffer.hpp"
#include "fon9/io/FdrService.hpp"

namespace fon9 { namespace io {

/// \ingroup io
/// 使用 fd 的實作的 TcpClient.
class fon9_API FdrTcpClient : public TcpClientBase {
   fon9_NON_COPY_NON_MOVE(FdrTcpClient);
   FdrTcpClient() = delete;
   using base = TcpClientBase;

protected:
   Socket      Socket_;
   SendBuffer  SendBuffer_;
   RecvBuffer  RecvBuffer_;

   virtual bool OpImpl_TcpConnect(Socket&& soCli, SocketResult& soRes) override;

public:
   const FdrServiceSP IoService_;
      
   FdrTcpClient(FdrServiceSP ioService, SessionSP ses, ManagerSP mgr)
      : TcpClientBase(std::move(ses), std::move(mgr))
      , IoService_{std::move(ioService)} {
   }

   virtual bool IsSendBufferEmpty() const override;
   virtual SendResult SendASAP(const void* src, size_t size) override;
   virtual SendResult SendASAP(BufferList&& src) override;
   virtual SendResult SendBuffered(const void* src, size_t size) override;
   virtual SendResult SendBuffered(BufferList&& src) override;
};

} } // namespaces
#endif//__fon9_io_FdrTcpClient_hpp__
