/// \file fon9/ByteVector.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ByteVector_hpp__
#define __fon9_ByteVector_hpp__
#include "fon9/StrView.hpp"
#include "fon9/Blob.h"
#include <string.h>

namespace fon9 {

/// \ingroup Misc
/// 可變大小的 byte 陣列型別.
/// - 在小量資料時(在x64系統, 大約23 bytes), 可以不用分配記憶體.
/// - assign(); append(); 可能丟出 bad_alloc 異常.
class fon9_API ByteVector : public Comparable<ByteVector> {
protected:
   struct Key {
      union {
         fon9_Blob   Blob_;
         byte        Bins_[sizeof(fon9_Blob)];
      };
      char  ExBinsBuffer_[7];
      char  BinsSize_{-1};
   };
   Key   Key_;

   void FreeBlob() {
      if (this->IsInBlob())
         fon9_Blob_Free(&this->Key_.Blob_);
   }
   /// 把現有資料放入 blob, 並額外保留 max(0, reversesz - this->size()) 個空間.
   /// assert(!this->IsInBlob());
   bool MoveToBlob(size_t reversesz);

   void CopyBytes(const void* mem, size_t sz);

protected:
   bool IsRefMem() const {
      return(this->Key_.BinsSize_ == 0 && this->Key_.Blob_.MemSize_ == 0);
   }
   bool IsInBlob() const {
      return(this->Key_.BinsSize_ == 0 && this->Key_.Blob_.MemSize_ > 0);
   }
   bool IsUseRefOrBlob() const {
      return(this->Key_.BinsSize_ == 0);
   }
   bool IsUseInternal() const {
      return(this->Key_.BinsSize_ != 0);
   }

   /// 在 this 的存續期間, 須自行確保 mem 的有效!
   ByteVector(const void* mem, size_t sz) {
      this->Key_.BinsSize_ = 0;
      this->Key_.Blob_.MemPtr_ = reinterpret_cast<byte*>(const_cast<void*>(mem));
      this->Key_.Blob_.MemSize_ = 0;
      this->Key_.Blob_.MemUsed_ = static_cast<fon9_Blob_Size_t>(sz);
   }

public:
   using value_type = byte;
   enum {
      kMaxBinsSize = offsetof(Key, BinsSize_)
   };

   ByteVector() = default;

   ByteVector(const ByteVector& rhs) {
      operator=(rhs);
   }
   ByteVector& operator=(const ByteVector& rhs);

   ByteVector(ByteVector&& rhs) {
      this->Key_ = rhs.Key_;
      rhs.Key_.BinsSize_ = -1;
   }
   ByteVector& operator=(ByteVector&&);

   explicit ByteVector(StrView str) {
      this->CopyBytes(str.begin(), str.size());
   }
   explicit ByteVector(const std::string& str) {
      this->CopyBytes(str.c_str(), str.size());
   }

   ~ByteVector() {
      this->FreeBlob();
   }

   static const ByteVector MakeRef(StrView str) {
      return ByteVector{str.begin(), str.size()};
   }
   static const ByteVector MakeRef(const std::string& str) {
      return ByteVector{str.c_str(), str.size()};
   }
   static const ByteVector MakeRef(const void* mem, size_t sz) {
      return ByteVector{mem,sz};
   }
   static const ByteVector MakeRef(const void* pbeg, const void* pend) {
      return ByteVector{pbeg,static_cast<size_t>(reinterpret_cast<const char*>(pend) - reinterpret_cast<const char*>(pbeg))};
   }

   int Compare(const ByteVector& rhs) const {
      return fon9_CompareBytes(this->begin(), this->size(), rhs.begin(), rhs.size());
   }
   bool operator< (const ByteVector& rhs) const {
      return this->Compare(rhs) < 0;
   }
   bool operator== (const ByteVector& rhs) const {
      size_t lsz = this->size();
      size_t rsz = rhs.size();
      return lsz == rsz && memcmp(this->begin(), rhs.begin(), rsz) == 0;
   }

   bool empty() const {
      return this->size() == 0;
   }
   size_t size() const {
      if (this->Key_.BinsSize_ < 0)
         return 0;
      return this->IsUseRefOrBlob() ? this->Key_.Blob_.MemUsed_ : static_cast<size_t>(this->Key_.BinsSize_);
   }
   byte* begin() {
      return this->IsUseRefOrBlob() ? this->Key_.Blob_.MemPtr_ : this->Key_.Bins_;
   }
   byte* end() {
      if (this->Key_.BinsSize_ < 0)
         return this->Key_.Bins_;
      return this->IsUseRefOrBlob()
         ? (this->Key_.Blob_.MemPtr_ + this->Key_.Blob_.MemUsed_)
         : (this->Key_.Bins_ + this->Key_.BinsSize_);
   }
   const byte* begin() const {
      return const_cast<ByteVector*>(this)->begin();
   }
   const byte* end() const {
      return const_cast<ByteVector*>(this)->end();
   }

   void clear();
   void reserve(size_t reversesz);
   void erase(size_t offset, size_t count);
   void Free() {
      if (this->IsInBlob())
         fon9_Blob_Free(&this->Key_.Blob_);
      this->Key_.BinsSize_ = -1;
   }

   void append(const void* mem, size_t sz);
   void append(const void* pbeg, const void* pend) {
      ptrdiff_t sz = reinterpret_cast<const byte*>(pend) - reinterpret_cast<const byte*>(pbeg);
      if (sz > 0)
         this->append(pbeg, static_cast<size_t>(sz));
   }
   void append(size_t sz, byte ch);
   void append(const StrView& src) {
      this->append(src.begin(), src.size());
   }

   void push_back(byte ch) {
      this->append(1, ch);
   }

   /// 分配記憶體,但不設定初值.
   /// 返回後應立即填入資料.
   /// - 若 sz == this->size() 則確定傳回 this->begin(); 不會重新分配(也不會釋放).
   ///   除非之前是用 MakeRef() 建立的.
   ///
   /// \retval nullptr  記憶體不足, 內容不變.
   /// \retval !nullptr 記憶體開始位置, 返回時 this->size()==sz, 內容無定義(原有資料不保留).
   void* alloc(size_t sz);

   void assign(const void* mem, size_t sz);
   void assign(const void* pbeg, const void* pend) {
      this->assign(pbeg, static_cast<size_t>(reinterpret_cast<const byte*>(pend) - reinterpret_cast<const byte*>(pbeg)));
   }
   void assign(const StrView& src) {
      this->assign(src.begin(), src.size());
   }

   size_t capacity() const {
      return this->IsInBlob() ? this->Key_.Blob_.MemSize_ : static_cast<size_t>(this->kMaxBinsSize);
   }
   void resize(size_t sz);
};

inline StrView CastToStrView(const ByteVector& bary) {
   return StrView{reinterpret_cast<const char*>(bary.begin()), bary.size()};
}

} // namespaces
#endif//__fon9_ByteVector_hpp__
