/// \file fon9/fix/IoFixSender.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_IoFixSender_hpp__
#define __fon9_fix_IoFixSender_hpp__
#include "fon9/fix/FixSender.hpp"
#include "fon9/io/Device.hpp"

namespace fon9 { namespace fix {
   
/// \ingroup fix
/// 使用 fon9::io::Device 輸出 FIX 訊息.
class fon9_API IoFixSender : public FixSender {
   fon9_NON_COPY_NON_MOVE(IoFixSender);
   using base = FixSender;

   io::DeviceSP  Device_;

   void OnSendFixMessage(const Locker&, BufferList buf) override;

   void SwapDevice(io::DeviceSP& dev) {
      Locker lk{this->Lock()};
      this->Device_.swap(dev);
   }

public:
   using base::base;
   ~IoFixSender();

   void OnFixSessionDisconnected() {
      io::DeviceSP dev;
      this->SwapDevice(dev);
   }
   void OnFixSessionConnected(io::DeviceSP dev) {
      this->SwapDevice(dev);
   }
};
using IoFixSenderSP = intrusive_ptr<IoFixSender>;

} } // namespaces
#endif//__fon9_fix_IoFixSender_hpp__
