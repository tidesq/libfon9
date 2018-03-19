// \file fon9/buffer/MemBlock.hpp
// \author fonwinz@gmail.com
#include "fon9/buffer/MemBlock.hpp"

namespace fon9 {

byte* MemBlock::Alloc(MemBlockSize sz) {
   // 在 MemPool 機制尚未完成前, 透過 malloc() 直接分配 mem.
   sz += 64;
   this->Size_ = -static_cast<SSizeT>(sz);
   return this->MemPtr_ = reinterpret_cast<byte*>(malloc(sz));
}
void MemBlock::FreeBlock(void* mem, MemBlockSize sz) {
   (void)sz;
   return free(mem);
}

} // namespace fon9
