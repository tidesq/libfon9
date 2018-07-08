// \file fon9/BitvEncode.cpp
// \author fonwinz@gmail.com
#include "fon9/BitvEncode.hpp"

namespace fon9 {

fon9_API size_t ByteArrayToBitv_NoEmpty(RevBuffer& rbuf, const void* byteArray, size_t byteCount) {
   assert(byteCount > 0);
   if (fon9_UNLIKELY(byteCount > kBitvMaxByteArraySize))
      Raise<std::length_error>("ByteArrayToBitv: byteCount too big.");
   // 動態長度的資料(e.g. std::string, ByteVector, CharVector...)
   // 如果尾端有 '\0', 如果使用底下作法, 則 BitvTo() 還原時, 長度會縮減, 這樣正確嗎?
   // while (reinterpret_cast<const byte*>(byteArray)[byteCount - 1] == 0) {
   //    if (--byteCount == 0)
   //       return RevPutBitv(rbuf, fon9_BitvV_ByteArrayEmpty);
   // }
   char*  pout = rbuf.AllocPrefix(sizeof(uint32_t) + byteCount + 1);
   memcpy(pout -= byteCount, byteArray, byteCount);
   return byteCount + ByteArraySizeToBitvT_NoEmpty(rbuf, pout, static_cast<uint32_t>(byteCount));
}

//--------------------------------------------------------------------------//

template <typename T>
inline enable_if_t<sizeof(T) != 1, DecScaleT> BitvAdjustScale(T& value, DecScaleT scale) {
   if (fon9_UNLIKELY(scale <= 0))
      return 0;
   // fon9_BitvT_Decimal 格式:
   // `1011 0xxx` + `sssss S mm` + vvvv...
   //                        \__ value __/
   for (;;) {
      if ((value & ~static_cast<T>(0x3ff)) == 0)
         return scale;
      if (scale == 1)
         break;
      if (value % 100 != 0)
         break;
      value /= 100;
      --scale;
      if (--scale == 0)
         return 0;
   }
   if (value % 10 == 0) {
      value /= 10;
      --scale;
   }
   return scale;
}

template <typename T>
inline enable_if_t<sizeof(T) == 1, DecScaleT> BitvAdjustScale(T&, DecScaleT scale) {
   return scale;
}

fon9_API size_t DecToBitv(RevBuffer& rbuf, uintmax_t value, DecScaleT scale, bool isNeg) {
   char* pout = rbuf.AllocPrefix(sizeof(value) + 2);
   scale = static_cast<DecScaleT>(scale << 3);
   if (isNeg)
      scale |= 0x04;

   byte vByteCount = PackBigEndianRev(pout, value);
   pout -= vByteCount;
   if (vByteCount > 1 && (*pout & 0xfc) == 0) {
      *pout |= static_cast<char>(scale);
      --vByteCount;
   }
   else {
      *--pout = static_cast<char>(scale);
   }
   byte vSize = static_cast<byte>(vByteCount + 2u);
   *--pout = static_cast<char>(fon9_BitvT_Decimal | (vByteCount - 1));
   rbuf.SetPrefixUsed(pout);
   return vSize;
}

fon9_API size_t DecToBitv(RevBuffer& rbuf, uintmax_t value, DecScaleT scale) {
   if (value == 0)
      return RevPutBitv(rbuf, fon9_BitvV_Number0);
   if ((scale = BitvAdjustScale(value, scale)) == 0)
      return IntToBitv_NoZero(rbuf, value, fon9_BitvT_IntegerPos);
   return DecToBitv(rbuf, value, scale, false);
}

fon9_API size_t DecToBitv(RevBuffer& rbuf, intmax_t value, DecScaleT scale) {
   if (value == 0)
      return RevPutBitv(rbuf, fon9_BitvV_Number0);
   if ((scale = BitvAdjustScale(value, scale)) == 0) {
      return value < 0
         ? IntToBitv_NoZero(rbuf, static_cast<uintmax_t>(~value), fon9_BitvT_IntegerNeg)
         : IntToBitv_NoZero(rbuf, static_cast<uintmax_t>(value), fon9_BitvT_IntegerPos);
   }
   if (value < 0)
      return DecToBitv(rbuf, static_cast<uintmax_t>(~value), scale, true);
   return DecToBitv(rbuf, static_cast<uintmax_t>(value), scale, false);
}

} // namespace
