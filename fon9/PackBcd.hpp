/// \file fon9/PackBcd.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_PackBcd_hpp__
#define __fon9_PackBcd_hpp__
#include "fon9/DecBase.hpp"

namespace fon9 {

namespace impl {
template <typename NumT, unsigned PackedWidth>
struct AuxPackBcd {
   using next = AuxPackBcd<NumT, PackedWidth - 2>;
   constexpr static NumT RevTo(const unsigned char* pbcd) noexcept {
      return static_cast<NumT>(AuxPackBcd<NumT, 2>::RevTo(pbcd) + next::RevTo(pbcd - 1) * 100);
   }
   constexpr static void ToRev(unsigned char* pbuf, NumT val) noexcept {
      AuxPackBcd<NumT, 2>::ToRev(pbuf, (val % 100));
      next::ToRev(pbuf - 1, val / 100);
   }
};
template <typename NumT>
struct AuxPackBcd<NumT, 2> {
   constexpr static NumT RevTo(const unsigned char* pbcd) noexcept {
      return static_cast<NumT>((*pbcd >> 4) * 10 + (*pbcd & 0x0f));
   }
   constexpr static void ToRev(unsigned char* pbuf, NumT val) noexcept {
      *pbuf = static_cast<unsigned char>(((static_cast<unsigned char>(val) / 10) << 4) | (static_cast<unsigned char>(val) % 10));
   }
};
template <typename NumT>
struct AuxPackBcd<NumT, 1> {
   constexpr static NumT RevTo(const unsigned char* pbcd) noexcept {
      return static_cast<NumT>(*pbcd & 0x0f);
   }
   constexpr static void ToRev(unsigned char* pbuf, NumT val) noexcept {
      *pbuf = static_cast<unsigned char>(val);
   }
};
} // namespace impl
  
/// \ingroup AlNum
/// Pack BCD 轉整數輸出.
/// 不驗證 Pack BCD 的內容是否正確, 所以如果有不正確的資料, 則輸出結果無法預期!
/// 例: `PackBcdTo<6,unsigned>(pbcd);`
template <unsigned PackedWidth, typename IntT>
inline IntT PackBcdTo(const void* pbcd) {
   static_assert(PackedWidth > 0, "PackBcdTo() PackedWidth must > 0.");
   static_assert(std::is_unsigned<IntT>::value, "PackBcdTo() IntT must be unsigned.");
   return impl::AuxPackBcd<IntT, PackedWidth>::RevTo(reinterpret_cast<const unsigned char*>(pbcd) + ((PackedWidth - 1) / 2));
}

/// \ingroup AlNum
/// 整數轉 Pack BCD 輸出.
/// 不驗證緩衝區是否足夠, 您必須自行確定 pbuf 有足夠的空間!
/// 例: `ToPackBcd<6>(pbuf, 12345);`
/// 填入: pbuf[0]=0x01; pbuf[1]=0x23; pbuf[2]=0x45; 也就是 pbuf 必須有 3 bytes 的空間.
template <unsigned PackedWidth, typename IntT>
inline void ToPackBcd(void* pbuf, IntT num) {
   static_assert(PackedWidth > 0, "ToPackBcd() PackedWidth must > 0.");
   static_assert(std::is_unsigned<IntT>::value, "ToPackBcd() IntT must be unsigned.");
   impl::AuxPackBcd<IntT, PackedWidth>::ToRev(reinterpret_cast<unsigned char*>(pbuf) + ((PackedWidth - 1) / 2), num);
}

//--------------------------------------------------------------------------//

constexpr unsigned PackBcdWidthToSize(unsigned packedWidth) {
   return (packedWidth + 1) / 2;
}

template <unsigned PackedWidth>
using PackBcd = unsigned char[PackBcdWidthToSize(PackedWidth)];

template <typename IntT, unsigned sz>
inline IntT PackBcdTo(const unsigned char (&pbcd)[sz]) {
   static_assert(static_cast<IntT>(DecDivisor<uint64_t, sz * 2>::Divisor - 1) == DecDivisor<uint64_t, sz * 2>::Divisor - 1,
                 "The result may be overflow.");
   return PackBcdTo<sz * 2, IntT>(pbcd);
}

template <unsigned sz, typename IntT>
inline void ToPackBcd(unsigned char (&pbuf)[sz], IntT num) {
   ToPackBcd<sz * 2, IntT>(static_cast<void*>(pbuf), num);
}

} // namespace fon9
#endif//__fon9_PackBcd_hpp__
