/// \file fon9/seed/Raw.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_Raw_hpp__
#define __fon9_seed_Raw_hpp__
#include "fon9/seed/Tab.hpp"
#include "fon9/Unaligned.hpp"

namespace fon9 { namespace seed {

/// \ingroup seed
/// 記憶體中儲存一筆資料的基礎類別。
class fon9_API Raw {
   fon9_NON_COPY_NON_MOVE(Raw);

   friend class RawRd;
   uint32_t DyMemPos_ = 0;
   uint32_t DyMemSize_ = 0;

   using DyBlobCountT = uint32_t;
   DyBlobCountT GetDyBlobCount() const {
      return this->DyMemSize_ <= 0 ? 0
         : GetUnaligned(reinterpret_cast<const DyBlobCountT*>(
            reinterpret_cast<const byte*>(this) + this->DyMemPos_ + this->DyMemSize_));
   }

protected:
   Raw() {}

   /// 解構時根據 this->GetDyBlobCount() 釋放 DyBlob 欄位.
   ~Raw();

   template <class RawT, class... ArgsT>
   friend RawT* MakeDyMemRaw(const Tab& tab, ArgsT&&... args);
};

/// \ingroup seed
/// new 一個有需要額外 DyMem 的 Raw 物件.
/// - 額外的 DyMem 初始化: 填入 0
/// - 不用時, 必須透過 delete 刪除.
template <class DerivedT, class... ArgsT>
inline DerivedT* MakeDyMemRaw(const Tab& tab, ArgsT&&... args) {
   size_t dyMemSize = tab.DyMemSize_;
   if (dyMemSize > 0) // 增加存放 DyBlobCount 的空間.
      dyMemSize += sizeof(Raw::DyBlobCountT);

   #ifdef new
      byte*  memptr = new byte[sizeof(DerivedT) + dyMemSize];
      #undef new
   #else
      byte*  memptr = reinterpret_cast<byte*>(::operator new (sizeof(DerivedT) + dyMemSize));
   #endif

   DerivedT*  derived = new (memptr) DerivedT{std::forward<ArgsT>(args)...};
   #ifdef DBG_NEW
   #define new DBG_NEW
   #endif

   Raw* raw = CastToBasePointer(derived, static_cast<Raw*>(nullptr));
   if ((raw->DyMemSize_ = tab.DyMemSize_) > 0) {
      memptr += sizeof(DerivedT);
      raw->DyMemPos_ = static_cast<uint32_t>(memptr - reinterpret_cast<byte*>(raw));
      memset(memptr, 0, tab.DyMemSize_);
      // 把 DyBlobCount 放到 DyMem 尾端.
      PutUnaligned(reinterpret_cast<Raw::DyBlobCountT*>(memptr + raw->DyMemSize_),
                   static_cast<Raw::DyBlobCountT>(tab.DyBlobCount_));
   }
   return derived;
}

} } // namespaces
#endif//__fon9_seed_Raw_hpp__
