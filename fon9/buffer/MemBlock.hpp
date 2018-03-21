/// \file fon9/buffer/MemBlock.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_buffer_MemBlock_hpp__
#define __fon9_buffer_MemBlock_hpp__
#include "fon9/sys/Config.hpp"
#include "fon9/Utility.hpp"

namespace fon9 {

using MemBlockSize = uint32_t;

fon9_WARN_DISABLE_PADDING;
/// \ingroup Buffer
/// 透過在每個 Thread 建立一個 MemPool 加快記憶體分配/釋放的速度, 分配出來的記憶體透過 MemBlock 來取用.
/// - 此處 **不是** 通用的記憶體配置管理.
/// - 通常用在 **非同步IO** 緩衝區使用, 例:
///   - Thread A 建立一筆訊息, 透過 Thread B 送出, 送完後釋放記憶體.
///   - Thread A 建立一筆Log訊息, 透過 Thread B 寫入檔案, 寫入後釋放記憶體.
/// - 會透過 GetDefaultTimerThread() 處理記憶體 **預先分配** & **多餘釋放** 的工作.
/// - 記憶體數量等級(可能的情況):
///   - 256B, 512B, 1024B, 2KB, 4KB... 最大64K
///   - 也就是: 要求量<=256B 分配 256B; 要求量<=512B 分配 512B; ...
class fon9_API MemBlock {
   fon9_NON_COPYABLE(MemBlock);
   /// <0 表示直接使用 MemPtr_ 直接從 malloc(-Size_) 得到, 釋放時須直接呼叫 free();
   using SSizeT = typename std::make_signed<MemBlockSize>::type;
   byte*    MemPtr_{nullptr};
   SSizeT   Size_{0};
public:
   class TCache;
   MemBlock() = default;

   /// 透過 MemBlock::Alloc(MemBlockSize) 建構.
   explicit MemBlock(MemBlockSize sz) {
      this->Alloc(sz);
   }

   MemBlock(MemBlock&& r) : MemPtr_(r.Release()), Size_(r.Size_) {
   }
   MemBlock& operator=(MemBlock&& r) {
      if (this->MemPtr_ != r.MemPtr_) {
         // 避免 x = std::move(x);
         this->Free();
         this->Size_ = r.Size_;
         this->MemPtr_ = r.Release();
      }
      return *this;
   }

   ~MemBlock() {
      this->Free();
   }

   /// 分配最少所需的記憶體數量.
   /// - 記憶體區塊的可用大小必定 >= size.
   /// - 記憶體不會初始化.
   /// - 若this原本已有分配, 則會先釋放.
   /// - 若記憶體不足則傳回 nullptr.
   byte* Alloc(MemBlockSize sz);
   /// 歸還記憶體.
   void Free() {
      MemBlockSize sz = this->size();
      FreeBlock(this->Release(), sz);
   }
   /// 取出被管理的記憶體, 並使 MemBlock 不再管理他, 之後您必須自行透過 FreeBlock() 來釋放.
   byte* Release() {
      byte* mem = this->MemPtr_;
      this->Size_ = 0;
      this->MemPtr_ = nullptr;
      return mem;
   }

   /// 取得已分配的記憶體開始位置.
   byte* begin() const {
      return this->MemPtr_;
   }
   /// 取得已分配的記憶體結束位置.
   byte* end() const {
      return this->MemPtr_ + this->size();
   }
   MemBlockSize size() const {
      return abs_cast(this->Size_);
   }

   /// 歸還一塊 Release() 取得的記憶體.
   static void FreeBlock(void* mem, MemBlockSize size);
};
fon9_WARN_POP;

} // namespaces
#endif//__fon9_buffer_MemBlock_hpp__
