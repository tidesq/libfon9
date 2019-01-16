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

size_t DcQueue::PopUnknownChar(char ch) {
   size_t removedBytes = 0;
   for (;;) {
      auto  blk = this->PeekCurrBlock();
      if (blk.first == nullptr)
         break;
      const byte* pch = reinterpret_cast<const byte*>(memchr(blk.first, ch, blk.second));
      if (fon9_LIKELY(pch)) {
         if (const size_t sz = static_cast<size_t>(pch - blk.first)) {
            removedBytes += sz;
            this->PopConsumed(sz);
         }
         break;
      }
      this->PopConsumed(blk.second);
      removedBytes += blk.second;
   }
   return removedBytes;
}

//--------------------------------------------------------------------------//

void LinePeeker::PopConsumed(DcQueue& dcq) {
   assert(this->LineSize_ > 0);
   if (this->TmpBuf_.empty())
      dcq.PopConsumed(this->LineSize_);
   else
      this->TmpBuf_.clear();
   this->LineSize_ = 0;
}
const char* LinePeeker::PeekUntil(DcQueue& dcq, char chUntil) {
   for (;;) {
      auto blk = dcq.PeekCurrBlock();
      if (blk.first == nullptr)
         return nullptr;
      if (auto pfind = reinterpret_cast<const char*>(memchr(blk.first, chUntil, blk.second))) {
         const size_t lnsz = static_cast<size_t>(pfind - reinterpret_cast<const char*>(blk.first) + 1);
         if (this->TmpBuf_.empty()) {
            this->LineSize_ = lnsz;
            return reinterpret_cast<const char*>(blk.first);
         }
         this->TmpBuf_.append(reinterpret_cast<const char*>(blk.first), pfind + 1);
         dcq.PopConsumed(lnsz);
         this->LineSize_ = this->TmpBuf_.size();
         return this->TmpBuf_.c_str();
      }
      this->TmpBuf_.append(reinterpret_cast<const char*>(blk.first), blk.second);
      dcq.PopConsumed(blk.second);
   }
}

} // namespace
