/// \file fon9/buffer/RevBuffer.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_buffer_RevBuffer_hpp__
#define __fon9_buffer_RevBuffer_hpp__
#include "fon9/Exception.hpp"

namespace fon9 {

/// \ingroup Buffer
/// Reverse buffer: 從尾端往前方填入資料.
class RevBuffer {
   fon9_NON_COPY_NON_MOVE(RevBuffer);
protected:
   char* MemBegin_;
   char* MemCurrent_;

   explicit RevBuffer() : MemBegin_{}, MemCurrent_{} {
   }
   virtual ~RevBuffer() {
   }
   /// 由衍生者決定: 重新分配記憶體? throw exception? 增加 node?
   virtual char* OnRevBufferAlloc(size_t) = 0;

public:
   RevBuffer(void* beg, void* end) : MemBegin_{static_cast<char*>(beg)}, MemCurrent_{static_cast<char*>(end)} {
   }
   RevBuffer(void* beg, size_t sz) : RevBuffer{beg, static_cast<char*>(beg) + sz} {
   }
   /// - 分配得到的指標 pout, 必須從 *--pout 開始填入資料.
   /// - 填妥後, 必須呼叫 SetPrefixUsed(pout); 告知實際用量.
   /// - 在尚未呼叫 SetPrefixUsed(pout) 之前, 再次呼叫 AllocPrefix(sz) 將傳回相同位置.
   char* AllocPrefix(size_t sz) {
      if (static_cast<size_t>(this->MemCurrent_ - this->MemBegin_) >= sz)
         return this->MemCurrent_;
      return this->OnRevBufferAlloc(sz);
   }
   void SetPrefixUsed(char* pout) {
      assert(this->MemBegin_ <= pout);
      this->MemCurrent_ = pout;
   }
   const char* GetCurrent() const {
      return this->MemCurrent_;
   }
};

/// \ingroup Buffer
/// 可用記憶體在建構時提供, 當空間不足時拋出例外: `fon9::BufferOverflow`;
class RevBufferFixedMem : public RevBuffer {
   fon9_NON_COPY_NON_MOVE(RevBufferFixedMem);
   char* MemEnd_;
   virtual char* OnRevBufferAlloc(size_t) {
      Raise<BufferOverflow>("RevBufferFixedMem overflow");
   }
public:
   RevBufferFixedMem(void* beg, size_t sz) : RevBuffer(beg,sz), MemEnd_{MemCurrent_} {
   }
   /// 取得已填入的資料量.
   size_t GetUsedSize() const {
      // 介面名稱不適合使用 size(): size()=剩餘可用量? 原始容量? 已填入的資料量?
      return static_cast<size_t>(this->MemEnd_ - this->MemCurrent_);
   }
   const char* GetMemEnd() const {
      return this->MemEnd_;
   }
   void Rewind() {
      this->MemCurrent_ = this->MemEnd_;
   }
   void RewindEOS() {
      this->MemCurrent_ = this->MemEnd_;
      *(--this->MemCurrent_) = '\0';
   }
};
/// \ingroup Buffer
/// 內含一個固定大小緩衝區的 RevBuffer.
template <size_t BufferSize>
class RevBufferFixedSize : public RevBufferFixedMem {
   fon9_NON_COPY_NON_MOVE(RevBufferFixedSize);
   char  TempBuffer_[BufferSize];
public:
   RevBufferFixedSize() : RevBufferFixedMem{TempBuffer_, sizeof(TempBuffer_)} {
   }
};

/// \ingroup Buffer
/// 這裡示範一個「完全不檢查」是否超過還衝大小的 RevBuffer.
/// 在您確定「不可能超過」建構時指定的大小時, 可以「加快一點點」速度.
class RevBufferUncheck {
   char* MemCurrent_;
public:
   RevBufferUncheck(void* beg, size_t sz) : MemCurrent_{static_cast<char*>(beg) + sz} {
   }
   char* AllocPrefix(size_t /*sz*/) {
      return this->MemCurrent_;
   }
   void SetPrefixUsed(char* pout) {
      this->MemCurrent_ = pout;
   }
   char* GetCurrent() const {
      return this->MemCurrent_;
   }
};

} // namespace
#endif//__fon9_buffer_RevBuffer_hpp__
