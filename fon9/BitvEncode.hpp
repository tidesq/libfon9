// \file fon9/BitvEncode.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_BitvEncode_hpp__
#define __fon9_BitvEncode_hpp__
#include "fon9/Bitv.h"
#include "fon9/RevPut.hpp"
#include "fon9/Decimal.hpp"
#include "fon9/Endian.hpp"
#include "fon9/Exception.hpp"
#include "fon9/ByteVector.hpp"
#include <array>

namespace fon9 {

inline size_t RevPutBitv(RevBuffer& rbuf, fon9_BitvV v) {
   RevPutChar(rbuf, static_cast<char>(v));
   return 1;
}

inline size_t ToBitv(RevBuffer& rbuf, bool value) {
   return RevPutBitv(rbuf, value ? fon9_BitvV_BoolTrue : fon9_BitvV_BoolFalse);
}

inline size_t ToBitv(RevBuffer& rbuf, char value) {
   if (value == 0)
      return RevPutBitv(rbuf, fon9_BitvV_Char0);
   char* pout = rbuf.AllocPrefix(2);
   *--pout = value;
   *--pout = 0;
   rbuf.SetPrefixUsed(pout);
   return 2;
}

//--------------------------------------------------------------------------//
// Integer to Bitv.

template <class RevBuffer, typename T>
inline enable_if_t<std::is_unsigned<T>::value, size_t>
IntToBitv_NoZero(RevBuffer& rbuf, T value, fon9_BitvT type) {
   assert(type == fon9_BitvT_IntegerNeg || value != 0);
   char* pout = rbuf.AllocPrefix(sizeof(value) + 1);
   byte  vByteCount = PackBigEndianRev(pout, value);
   *(pout -= vByteCount + 1) = static_cast<char>(type | (vByteCount - 1));
   rbuf.SetPrefixUsed(pout);
   return vByteCount + 1u;
}

template <class RevBuffer, typename T>
inline enable_if_t<std::is_unsigned<T>::value, size_t>
IntToBitv(RevBuffer& rbuf, T value) {
   if (value == 0)
      return RevPutBitv(rbuf, fon9_BitvV_Number0);
   return IntToBitv_NoZero(rbuf, value, fon9_BitvT_IntegerPos);
}

template <class RevBuffer, typename T>
inline enable_if_t<std::is_signed<T>::value, size_t>
IntToBitv(RevBuffer& rbuf, T value) {
   if (value == 0)
      return RevPutBitv(rbuf, fon9_BitvV_Number0);
   using UnsignedT = make_unsigned_t<T>;
   if (value > 0)
      return IntToBitv_NoZero(rbuf, static_cast<UnsignedT>(value), fon9_BitvT_IntegerPos);
   return IntToBitv_NoZero(rbuf, static_cast<UnsignedT>(~value), fon9_BitvT_IntegerNeg);
}

template <class ValueT>
inline enable_if_t<std::is_integral<ValueT>::value, size_t>
ToBitv(RevBuffer& rbuf, ValueT value) {
   return IntToBitv(rbuf, value);
}

template <class ValueT>
inline enable_if_t<std::is_enum<ValueT>::value, size_t>
ToBitv(RevBuffer& rbuf, ValueT value) {
   return ToBitv(rbuf, static_cast<typename std::underlying_type<ValueT>::type>(value));
}

//--------------------------------------------------------------------------//
// Decimal to Bitv.

/// \ingroup AlNum
/// 這裡不考慮 value == Null, 若有需要:
/// - 您必須在呼叫前自行考慮: value == Null, 填入 RevPutBitv(rbuf, fon9_BitvV_NumberNull);
fon9_API size_t DecToBitv(RevBuffer& rbuf, uintmax_t value, DecScaleT scale, bool isNeg);
fon9_API size_t DecToBitv(RevBuffer& rbuf, uintmax_t value, DecScaleT scale);
fon9_API size_t DecToBitv(RevBuffer& rbuf, intmax_t value, DecScaleT scale);

template <typename IntTypeT, DecScaleT ScaleN>
inline size_t ToBitv(RevBuffer& rbuf, const Decimal<IntTypeT, ScaleN>& value) {
   if (value.GetOrigValue() == value.OrigNull)
      return RevPutBitv(rbuf, fon9_BitvV_NumberNull);
   return DecToBitv(rbuf, static_cast<ToImax_t<IntTypeT>>(value.GetOrigValue()), ScaleN);
}

//--------------------------------------------------------------------------//
// ByteArray(std::string, char[], byte[]...) to Bitv.

enum : uint32_t {
   /// 最多使用 4 bytes 儲存 byteCount, 所以需保留 5 bytes 儲存 fon9_BitvT & byteCount
   kBitvMaxByteArraySize = UINT32_MAX - 5
};

inline size_t ByteArraySizeToBitvT_AboveX80(RevBuffer& rbuf, char* pout, uint32_t byteCount) {
   byte vByteCount = PackBigEndianRev(pout, byteCount - 0x80 - 1);
   pout -= vByteCount;
   if (vByteCount > 1 && *pout == 1)
      *pout |= static_cast<char>((fon9_BitvT_ByteArray | ((vByteCount - 2) << 1)));
   else {
      *--pout = static_cast<char>((fon9_BitvT_ByteArray | ((vByteCount - 1) << 1)));
      ++vByteCount;
   }
   rbuf.SetPrefixUsed(pout);
   return vByteCount;
}

inline size_t ByteArraySizeToBitvT_NoEmpty(RevBuffer& rbuf, char* pout, uint32_t byteCount) {
   assert(byteCount > 0);
   if (fon9_LIKELY(byteCount <= 0x80)) {
      *--pout = static_cast<char>(byteCount - 1);
      rbuf.SetPrefixUsed(pout);
      return 1;
   }
   return ByteArraySizeToBitvT_AboveX80(rbuf, pout, byteCount);
}

inline size_t ByteArraySizeToBitvT(RevBuffer& rbuf, size_t byteCount) {
   if (fon9_UNLIKELY(byteCount > kBitvMaxByteArraySize))
      Raise<std::length_error>("ByteArraySizeToBitvT: byteCount too big.");
   if (byteCount == 0)
      return RevPutBitv(rbuf, fon9_BitvV_ByteArrayEmpty);
   if (byteCount <= 0x80) {
      RevPutChar(rbuf, static_cast<char>(byteCount - 1));
      return 1;
   }
   char* pout = rbuf.AllocPrefix(sizeof(uint32_t) + 1);
   return ByteArraySizeToBitvT_AboveX80(rbuf, pout, static_cast<uint32_t>(byteCount));
}

fon9_API size_t ByteArrayToBitv_NoEmpty(RevBuffer& rbuf, const void* byteArray, size_t byteCount);

//--------------------------------------------------------------------------//

inline size_t ByteArrayToBitv(RevBuffer& rbuf, const void* byteArray, size_t byteCount) {
   if (byteCount > 0)
      return ByteArrayToBitv_NoEmpty(rbuf, byteArray, byteCount);
   return RevPutBitv(rbuf, fon9_BitvV_ByteArrayEmpty);
}

/// byte[], int8_t[], uint8_t[]...
template <typename T, size_t arysz>
inline enable_if_t<IsTrivialByte<T>::value, size_t>
ToBitv(RevBuffer& rbuf, const T (&ary)[arysz]) {
   return ByteArrayToBitv(rbuf, ary, arysz);
}
template <size_t arysz>
inline size_t ToBitv(RevBuffer& rbuf, const char (&ary)[arysz]) {
   return ByteArrayToBitv(rbuf, ary, arysz - (ary[arysz - 1] == 0));
}

template <typename T, size_t arysz>
inline enable_if_t<IsTrivialByte<T>::value, size_t>
ToBitv(RevBuffer& rbuf, const std::array<T,arysz>& ary) {
   return ByteArrayToBitv(rbuf, ary.data(), arysz);
}

inline size_t ToBitv(RevBuffer& rbuf, const StrView& str) {
   return ByteArrayToBitv(rbuf, str.begin(), str.size());
}

inline void ToBitv(RevBuffer& rbuf, const ByteVector& bvec) {
   ToBitv(rbuf, CastToStrView(bvec));
}

/// 若有提供 ToStrView(str) 則透過這裡處理.
/// std::string, CharAry, CharVector...
template <class StrT>
inline auto ToBitv(RevBuffer& rbuf, StrT&& str) -> decltype(ToStrView(str), size_t()) {
   return ToBitv(rbuf, ToStrView(str));
}

//--------------------------------------------------------------------------//
// Container to Bitv.
// enable_if: sizeof(Container::value_type) != 1

template <class Container>
inline auto ElementCountToBitv(RevBuffer& rbuf, const Container& c)
-> decltype(const_cast<Container*>(&c)->erase(c.begin()), void()) {
   if(size_t sz = c.size())
      IntToBitv_NoZero(rbuf, sz, fon9_BitvT_Container);
   else
      RevPutBitv(rbuf, fon9_BitvV_ContainerEmpty);
}
inline void ElementCountToBitv(RevBuffer&, ...) {
   // 固定大小的陣列, 不用紀錄 element count.
}

template <class Container>
auto ToBitv(RevBuffer& rbuf, const Container& c) -> enable_if_t<sizeof(*std::begin(c)) != 1> {
   const auto ibeg = std::begin(c);
   for (auto i = std::end(c); i != ibeg;)
      ToBitv(rbuf, *(--i));
   ElementCountToBitv(rbuf, c);
}

} // namespace
#endif//__fon9_BitvEncode_hpp__
