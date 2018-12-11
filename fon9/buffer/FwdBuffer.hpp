/// \file fon9/buffer/FwdBuffer.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_buffer_FwdBuffer_hpp__
#define __fon9_buffer_FwdBuffer_hpp__
#include "fon9/Exception.hpp"
#include "fon9/StrView.hpp"

namespace fon9 {

/// \ingroup Buffer
/// forward buffer: 從前方往尾端填入資料.
class FwdBuffer {
   fon9_NON_COPY_NON_MOVE(FwdBuffer);
protected:
   char* MemEnd_;
   char* MemCurrent_;

   explicit FwdBuffer() : MemEnd_{}, MemCurrent_{} {
   }
   virtual ~FwdBuffer() {
   }
   /// 由衍生者決定: 重新分配記憶體? throw exception? 增加 node?
   virtual char* OnFwdBufferAlloc(size_t) = 0;

public:
   FwdBuffer(void* beg, void* end) : MemEnd_{static_cast<char*>(end)}, MemCurrent_{static_cast<char*>(beg)} {
   }
   FwdBuffer(void* beg, size_t sz) : FwdBuffer{beg, static_cast<char*>(beg) + sz} {
   }
   /// - 分配得到的指標 pout, 必須從 *pout++ 開始填入資料.
   /// - 填妥後, 必須呼叫 SetSuffixUsed(pout); 告知實際用量.
   /// - 在尚未呼叫 SetSuffixUsed(pout) 之前, 再次呼叫 AllocSuffix(sz) 將傳回相同位置.
   char* AllocSuffix(size_t sz) {
      if (static_cast<size_t>(this->MemEnd_ - this->MemCurrent_) >= sz)
         return this->MemCurrent_;
      return this->OnFwdBufferAlloc(sz);
   }
   void SetSuffixUsed(char* pout) {
      assert(pout <= this->MemEnd_);
      this->MemCurrent_ = pout;
   }
   const char* GetCurrent() const {
      return this->MemCurrent_;
   }
};

/// \ingroup Buffer
/// 可用記憶體在建構時提供, 當空間不足時拋出例外: `fon9::BufferOverflow`;
class FwdBufferFixedMem : public FwdBuffer {
   fon9_NON_COPY_NON_MOVE(FwdBufferFixedMem);
   char* MemBegin_;
   virtual char* OnFwdBufferAlloc(size_t) {
      Raise<BufferOverflow>("FwdBufferFixedMem overflow");
   }
public:
   FwdBufferFixedMem(void* beg, size_t sz) : FwdBuffer(beg,sz), MemBegin_{MemCurrent_} {
   }
   /// 取得已填入的資料量.
   size_t GetUsedSize() const {
      // 介面名稱不適合使用 size(): size()=剩餘可用量? 原始容量? 已填入的資料量?
      return static_cast<size_t>(this->MemCurrent_ - this->MemBegin_);
   }
   const char* GetMemBegin() const {
      return this->MemBegin_;
   }
   void Rewind() {
      this->MemCurrent_ = this->MemBegin_;
   }

   template <class StrT>
   StrT ToStrT() const {
      return StrT{this->GetMemBegin(), this->GetCurrent()};
   }
   inline friend StrView ToStrView(FwdBufferFixedMem& fbuf) {
      return fbuf.ToStrT<StrView>();
   }
};
/// \ingroup Buffer
/// 內含一個固定大小緩衝區的 FwdBuffer.
template <size_t BufferSize>
class FwdBufferFixedSize : public FwdBufferFixedMem {
   fon9_NON_COPY_NON_MOVE(FwdBufferFixedSize);
   char  TempBuffer_[BufferSize];
public:
   FwdBufferFixedSize() : FwdBufferFixedMem{TempBuffer_, sizeof(TempBuffer_)} {
   }
};

/// \ingroup Buffer
/// 這裡示範一個「完全不檢查」是否超過還衝大小的 FwdBuffer.
/// 在您確定「不可能超過」建構時指定的大小時, 可以「加快一點點」速度.
class FwdBufferUncheck {
   char* MemCurrent_;
public:
   FwdBufferUncheck(void* beg, size_t sz) : MemCurrent_{static_cast<char*>(beg)} {
      (void)sz;
   }
   char* AllocSuffix(size_t /*sz*/) {
      return this->MemCurrent_;
   }
   void SetSuffixUsed(char* pout) {
      this->MemCurrent_ = pout;
   }
   char* GetCurrent() const {
      return this->MemCurrent_;
   }
};

} // namespace
#endif//__fon9_buffer_FwdBuffer_hpp__
