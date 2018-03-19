// \file fon9/buffer/DcQueue.cpp
// \author fonwinz@gmail.com
#include "fon9/buffer/DcQueue.hpp"

namespace fon9 {

size_t DcQueue::DcQueueReadMore(byte* buf, size_t sz) {
   this->DcQueueRemoveMore(0);
   size_t rdsz = 0;
   while (size_t blkszCurr = this->GetCurrBlockSize()) {
      if (blkszCurr > sz)
         blkszCurr = sz;
      memcpy(reinterpret_cast<byte*>(buf) + rdsz, this->MemCurrent_, blkszCurr);
      this->PopConsumed(blkszCurr);
      rdsz += blkszCurr;
      if (sz -= blkszCurr <= 0)
         break;
   }
   return rdsz;
}

} // namespace
