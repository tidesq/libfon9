// \file fon9/FileRevRead.cpp
// \author fonwinz@gmail.com
#include "fon9/FileRevRead.hpp"

namespace fon9 {

FileRevRead::~FileRevRead() {
}
File::Result FileRevRead::Start(File& fd, void* blockBuffer, const size_t blockSize) {
   File::Result res = fd.GetFileSize();
   if (!res || res.GetResult() <= 0)
      return res;
   this->BlockPos_ = res.GetResult();
   size_t   rdsz = this->BlockPos_ % blockSize; // 先讀最尾端, 非 [blockSize整數倍] 的部分.
   if (rdsz == 0)
      rdsz = blockSize;
   for (;;) {
      res = fd.Read(this->BlockPos_ -= rdsz, blockBuffer, rdsz);
      if (!res || res.GetResult() != rdsz)
         return res;
      if (this->OnFileBlock(rdsz) == LoopControl::Break)
         break;
      if (this->BlockPos_ == 0)
         break;
      rdsz = blockSize;
   }
   return File::Result{this->BlockPos_};
}

FileRevSearch::~FileRevSearch() {
}
LoopControl FileRevSearch::RevSearchBlock(File::PosType fpos, char ch, size_t rdsz) {
   char* pend = (this->DataEnd_ ? this->DataEnd_ : (this->BlockBufferPtr_ + rdsz));
   while (void* pfind = const_cast<void*>(memrchr(this->BlockBufferPtr_, ch, rdsz))) {
      if (this->OnFoundChar(reinterpret_cast<char*>(pfind), pend) == LoopControl::Break) {
         this->DataEnd_ = pend;
         return LoopControl::Break;
      }
      pend = reinterpret_cast<char*>(pfind);
      if ((rdsz = static_cast<size_t>(pend - this->BlockBufferPtr_)) <= 0)
         break;
   }
   if (fon9_LIKELY(fpos > 0)) {
      // 把剩下沒處理的資料,放到緩衝區尾端. 下次讀入 block 之後接續處理.
      if (rdsz > this->MaxMessageBufferSize_)
         rdsz = this->MaxMessageBufferSize_;
      memcpy(this->BlockBufferPtr_ + this->BlockSize_, this->BlockBufferPtr_, rdsz);
      this->DataEnd_ = this->BlockBufferPtr_ + this->BlockSize_ + rdsz;
      return LoopControl::Continue;
   }
   this->DataEnd_ = this->BlockBufferPtr_ + rdsz;
   return LoopControl::Continue;
}

} // namespace
