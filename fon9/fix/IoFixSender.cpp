// \file fon9/fix/IoFixSender.cpp
// \author fonwinz@gmail.com
#include "fon9/fix/IoFixSender.hpp"

namespace fon9 { namespace fix {
   
IoFixSender::~IoFixSender() {
}
void IoFixSender::OnSendFixMessage(const Locker& lk, BufferList buf) {
   assert(lk.owns_lock()); (void)lk;// FixRecorder 仍在鎖定狀態!
   if (io::Device* dev = this->Device_.get()) {
      // 使用 SendASAP() 一定會 low latency 嗎?
      // 因為會立即呼叫 send() or WSASend(), 可能會花不少時間.
      // - 這段時間如果可以處理 n 筆 Request
      // - 且瞬間有多筆要求在等著處理
      // - 那這些等待中的 Request, latency 就會變很高了!

      //dev->SendASAP(std::move(buf));     // for low latency?
      //dev->SendBuffered(std::move(buf)); // for high throughput?
      // 由 dev 的 property 決定是否 ASAP.
      dev->Send(std::move(buf));
   }
}

} } // namespaces
