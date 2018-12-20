// \file fon9/FileRevRead.cpp
// \author fonwinz@gmail.com
#include "fon9/FileRevRead.hpp"

namespace fon9 {

FileRevRead::~FileRevRead() {
}
File::Result FileRevRead::OnFileRead(File& fd, File::PosType fpos, void* blockBuffer, size_t rdsz) {
   return fd.Read(fpos, blockBuffer, rdsz);
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
      res = this->OnFileRead(fd, this->BlockPos_ -= rdsz, blockBuffer, rdsz);
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
void FileRevSearch::MoveRemainBuffer() {
   if (size_t rsz = this->LastRemainSize_) {
      this->LastRemainSize_ = 0;
      if (rsz > this->MaxMessageBufferSize_)
         rsz = this->MaxMessageBufferSize_;
      memcpy(this->BlockBufferPtr_ + this->BlockSize_, this->BlockBufferPtr_, rsz);
      this->PayloadEnd_ = this->BlockBufferPtr_ + this->BlockSize_ + rsz;
   }
   else
      this->PayloadEnd_ = nullptr;
}
LoopControl FileRevSearch::RevSearchBlock(File::PosType fpos, char ch, size_t rdsz) {
   (void)fpos;
   char* pend = this->PayloadEnd_ ? this->PayloadEnd_ : (this->BlockBufferPtr_ + rdsz);
   while (void* const pfind = const_cast<void*>(memrchr(this->BlockBufferPtr_, ch, rdsz))) {
      if (this->OnFoundChar(reinterpret_cast<char*>(pfind), pend) == LoopControl::Break) {
         this->FoundDataEnd_ = pend;
         this->LastRemainSize_ = static_cast<size_t>(reinterpret_cast<char*>(pfind) - this->BlockBufferPtr_);
         return LoopControl::Break;
      }
      pend = reinterpret_cast<char*>(pfind);
      if ((rdsz = static_cast<size_t>(pend - this->BlockBufferPtr_)) <= 0)
         break;
   }
   this->FoundDataEnd_ = nullptr;
   this->LastRemainSize_ = rdsz; // 把剩下沒處理的資料: 下次 RevSearchBlock() 時, 移到緩衝區尾端, 接續處理.
   return LoopControl::Continue;
}

} // namespace
