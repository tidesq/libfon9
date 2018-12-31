/// \file fon9/buffer/DcQueue.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_buffer_DcQueue_hpp__
#define __fon9_buffer_DcQueue_hpp__
#include "fon9/buffer/RevBuffer.hpp"
#include "fon9/ErrC.hpp"
#include "fon9/StrView.hpp"
#include <string.h> // memcpy();

namespace fon9 {

fon9_MSC_WARN_DISABLE(4265); /* dtor isnot virtual. */
/// \ingroup Buffer
/// RevBuffer, FwdBuffer 建立好的輸出資料, 移動到這裡, 提供消費者使用.
/// 消費者大約分為2類:
/// - DeviceOutput: 依序使用區塊.
/// - Parser: 解析資料內容.
class fon9_API DcQueue {
   fon9_NON_COPY_NON_MOVE(DcQueue);
protected:
   /// 當無資料時, 應將 MemCurrent_, MemEnd_ 設為 nullptr.
   const byte* MemCurrent_;
   const byte* MemEnd_;

   /// 除了 MemCurrent_ 區塊之外, 是否仍有 sz bytes 的有效資料?
   /// 此時 sz 必定 > 0;
   virtual bool DcQueueHasMore(size_t sz) const = 0;

   /// 將後續資料的 sz bytes 複製到 tmpbuf, 但不移除.
   /// 此時 sz 必定 > 0;
   /// \retval false 資料不足, 沒有複製任何資料.
   virtual bool DcQueuePeekMore(byte* tmpbuf, size_t sz) = 0;

   /// - sz == 0, 表示移除 MemCurrent_ 區塊, 並取出新的區塊放到 MemCurrent_, MemEnd_;
   /// - sz > 0, 將後續資料移除 sz bytes 之後, 將後續資料放到 MemCurrent_, MemEnd_;
   /// - 若移除後無資料, 則必須將 MemCurrent_, MemEnd_ 設為 nullptr;
   virtual void DcQueueRemoveMore(size_t sz) = 0;

   /// 預設使用 PopConsumed() 移除, 然後從 CurrBlock 複製.
   /// 呼叫時的狀態: CurrBlock 已用完, 但尚未移除; sz 必定 > 0;
   /// \return 傳回實際複製到 buf 的資料量.
   virtual size_t DcQueueReadMore(byte* buf, size_t sz);

   /// 沒有直接刪除 DcQueue 的需求,
   /// 所以把解構放到 protected 並且不用 virtual.
   ~DcQueue() {
   }
   void ResetCurrBlock(const void* beg, const void* end) {
      this->MemCurrent_ = static_cast<const byte*>(beg);
      this->MemEnd_ = static_cast<const byte*>(end);
   }
   void ResetCurrBlock(const void* beg, size_t sz) {
      this->MemCurrent_ = static_cast<const byte*>(beg);
      this->MemEnd_ = static_cast<const byte*>(beg) + sz;
   }
   void ClearCurrBlock() {
      this->MemCurrent_ = this->MemEnd_ = nullptr;
   }
public:
   DcQueue() : MemCurrent_{nullptr}, MemEnd_{nullptr} {
   }
   DcQueue(const void* beg, const void* end)
      : MemCurrent_{static_cast<const byte*>(beg)}
      , MemEnd_{static_cast<const byte*>(end)} {
   }
   DcQueue(const void* mem, size_t sz) : DcQueue{mem, static_cast<const byte*>(mem) + sz} {
   }

   bool empty() const {
      return this->MemCurrent_ >= this->MemEnd_;
   }

   virtual size_t CalcSize() const = 0;

   /// 取得有效資料, 但不移除.
   /// 適合取得小量資料(例: 儲存封包大小的封包頭)
   /// \retval nullptr 有效資料量 < sz
   /// \retval !=nullptr 有效資料量 >= sz
   ///   - 若 CurrBlock 的資料量>=sz, 則直接傳回 CurrBlock 的資料開始位置.
   ///   - 否則將資料複製到 tmpbuf, 並傳回 tmpbuf.
   const void* Peek(void* tmpbuf, size_t sz) {
      const size_t blkszCurr = this->GetCurrBlockSize();
      if (fon9_LIKELY(sz <= blkszCurr))
         return this->MemCurrent_;
      if (!this->DcQueuePeekMore(reinterpret_cast<byte*>(tmpbuf) + blkszCurr, sz - blkszCurr))
         return nullptr;
      return memcpy(tmpbuf, this->MemCurrent_, blkszCurr);
   }
   const byte* Peek1() const {
      return this->MemCurrent_;
   }

   using DataBlock = std::pair<const byte*, size_t>;
   DataBlock PeekCurrBlock() const {
      return DataBlock{this->MemCurrent_, this->GetCurrBlockSize()};
   }
   size_t GetCurrBlockSize() const {
      return static_cast<size_t>(this->MemEnd_ - this->MemCurrent_);
   }

   bool IsSizeEnough(size_t sz) const {
      const size_t blkszCurr = this->GetCurrBlockSize();
      if (fon9_LIKELY(sz <= blkszCurr))
         return true;
      return this->DcQueueHasMore(sz - blkszCurr);
   }

   void PopConsumed(size_t sz) {
      const size_t blkszCurr = this->GetCurrBlockSize();
      if (fon9_LIKELY(sz < blkszCurr))
         this->MemCurrent_ += sz;
      else
         this->DcQueueRemoveMore(sz - blkszCurr);
   }

   /// 資料輸出失敗, 清除全部的內容.
   virtual void ConsumeErr(const ErrC& errc) = 0;

   /// 移除 ch 之前的字元.
   /// \return 移除的 bytes 數.
   size_t PopUnknownChar(char ch);

   /// 將資料複製到 buf 並移除, 最多複製 sz bytes, 傳回實際複製的資料量.
   size_t Read(void* buf, const size_t sz) {
      const size_t blkszCurr = this->GetCurrBlockSize();
      const size_t usedCurr = (blkszCurr > sz ? sz : blkszCurr);
      if (usedCurr <= 0)
         return 0;
      memcpy(buf, this->MemCurrent_, usedCurr);
      if (fon9_LIKELY(sz <= usedCurr)) {
         this->PopConsumed(sz);
         return sz;
      }
      return usedCurr + this->DcQueueReadMore(reinterpret_cast<byte*>(buf) + usedCurr, sz - usedCurr);
   }

   /// 取出指定大小的資料量, 若資料量不足, 則不取出任何資料.
   /// \retval sz 取出的資料量, 返回前會移除(透過 PopConsumed).
   /// \retval 0  資料量不足, 沒取出資料.
   size_t Fetch(void* buf, size_t sz) {
      if (const void* ptr = this->Peek(buf, sz)) {
         if (buf != ptr)
            memcpy(buf, ptr, sz);
         this->PopConsumed(sz);
         return sz;
      }
      return 0;
   }
};

class DcQueueFixedMem : public DcQueue {
   fon9_NON_COPYABLE(DcQueueFixedMem);
   using base = DcQueue;
protected:
   virtual bool DcQueuePeekMore(byte* tmpbuf, size_t sz) override {
      (void)tmpbuf; (void)sz;
      return false;
   }
   virtual bool DcQueueHasMore(size_t sz) const override {
      (void)sz;
      return false;
   }
   virtual void DcQueueRemoveMore(size_t sz) override {
      (void)sz; assert(sz <= 0);
      this->ClearCurrBlock();
   }
public:
   DcQueueFixedMem() = default;
   DcQueueFixedMem(DcQueueFixedMem&& rhs) : base(rhs.MemCurrent_, rhs.MemEnd_) {
      rhs.ClearCurrBlock();
   }
   DcQueueFixedMem(const void* mem, size_t sz) : base(mem, sz) {
   }
   DcQueueFixedMem(const void* beg, const void* end) : base(beg, end) {
   }
   DcQueueFixedMem(const StrView& msg) : base(msg.begin(), msg.end()) {
   }
   template <class StrT, class T = decltype(ToStrView(*static_cast<StrT*>(nullptr)))>
   DcQueueFixedMem(const StrT& msg) : DcQueueFixedMem{ToStrView(msg)} {
   }
   template <class BufferT, class T = decltype(static_cast<BufferT*>(nullptr)->GetCurrent())>
   DcQueueFixedMem(BufferT& ibuf) : base(ibuf.GetCurrent(), ibuf.GetMemEnd()) {
   }

   DcQueueFixedMem& operator=(DcQueueFixedMem&& rhs) {
      this->ResetCurrBlock(rhs.MemCurrent_, rhs.MemEnd_);
      rhs.ClearCurrBlock();
      return *this;
   }

   template <class BufferT>
   void Reset(BufferT& ibuf) {
      this->ResetCurrBlock(ibuf.GetCurrent(), ibuf.GetMemEnd());
   }
   void Reset(const void* beg, const void* end) {
      this->ResetCurrBlock(beg, end);
   }

   virtual size_t CalcSize() const override {
      return this->GetCurrBlockSize();
   }
   virtual void ConsumeErr(const ErrC&) override {
      this->ClearCurrBlock();
   }
};
fon9_MSC_WARN_POP;

/// \ingroup Buffer
/// 把 buf 透過 fnWriter(blk, blksz) 消費掉,
/// 當作 DcQueue.PeekCurrBlock() 的使用範例.
/// 每次消費一個block, 直到: buf 用完, 或, fnWriter() 返回錯誤.
template <class DcQueue, class FnWriter>
auto DeviceOutputBlock(DcQueue&& buf, FnWriter fnWriter) -> decltype(fnWriter(nullptr, 0)) {
   using Outcome = decltype(fnWriter(nullptr, 0));
   typename Outcome::ResultType wsz{0};
   for (;;) {
      auto blk = buf.PeekCurrBlock();
      if (!blk.first)
         return Outcome{wsz};
      const Outcome res = fnWriter(blk.first, blk.second);
      if (!res) {
         buf.ConsumeErr(res.GetError());
         return res;
      }
      wsz += res.GetResult();
      buf.PopConsumed(res.GetResult());
   }
}

} // namespace
#endif//__fon9_buffer_DcQueue_hpp__
