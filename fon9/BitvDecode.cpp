// \file fon9/BitvDecode.cpp
// \author fonwinz@gmail.com
#include "fon9/BitvDecode.hpp"
#include "fon9/CharVector.hpp"
#include "fon9/Blob.h"

namespace fon9 {

fon9_API bool PopBitvByteArraySize(DcQueue& buf, size_t& barySize) {
   const byte* const ptype = buf.Peek1();
   if (ptype == nullptr) {
      barySize = 0;
      return false;
   }

   if (fon9_LIKELY((*ptype & 0x80) == 0x00)) {
      barySize = *ptype + 1u;
      if (fon9_UNLIKELY(!buf.IsSizeEnough(barySize + 1u)))
         return false;
      buf.PopConsumed(1);
      return true;
   }

   if (fon9_UNLIKELY(*ptype == fon9_BitvV_ByteArrayEmpty)) {
      barySize = 0;
      buf.PopConsumed(1);
      return true;
   }

   if (fon9_UNLIKELY((*ptype & fon9_BitvT_Mask) != fon9_BitvT_ByteArray))
      Raise<BitvTypeNotMatch>("PopBitvByteArraySize: type not match");

   byte        bCount = static_cast<byte>(((*ptype >> 1) & 0x03) + 1); // = 用了幾個 byte 儲存 barySize?
   uintmax_t   numbuf[2];
   const byte* pnum = static_cast<const byte*>(buf.Peek(numbuf, bCount + 1u)); // +1 for skip *ptype.
   if (fon9_UNLIKELY(pnum == nullptr)) {
      barySize = 0;
      return false;
   }

   barySize = GetPackedBigEndian<uint32_t>(pnum + 1, bCount);
   if (*pnum & 0x01)
      barySize |= (static_cast<size_t>(1) << (bCount * 8));
   barySize += 0x80 + 1;
   ++bCount; // = fon9_BitvT[1] + used_of(barySize)
   if (!buf.IsSizeEnough(barySize + bCount))
      return false;
   buf.PopConsumed(bCount);
   return true;
}

//--------------------------------------------------------------------------//

fon9_API void BitvToStrAppend(DcQueue& buf, std::string& dst) {
   BitvToStrAppend<std::string>(buf, dst);
}
fon9_API void BitvToStrAppend(DcQueue& buf, CharVector& dst) {
   BitvToStrAppend<CharVector>(buf, dst);
}

//--------------------------------------------------------------------------//

fon9_API void BitvToBlob(DcQueue& buf, fon9_Blob& dst) {
   size_t   byteCount;
   if (!PopBitvByteArraySize(buf, byteCount))
      Raise<BitvNeedsMore>("BitvToBlob: needs more");
   dst.MemUsed_ = 0;
   if (byteCount > 0) {
      if (!fon9_Blob_Set(&dst, nullptr, static_cast<fon9_Blob_Size_t>(byteCount)))
         Raise<std::bad_alloc>();
      if (buf.Read(dst.MemPtr_, byteCount) != byteCount) {
         assert(!"BitvToBlob: Unexpected read size!");
      }
   }
}

fon9_API size_t BitvToByteArray(DcQueue& buf, void* bary, size_t barySize) {
   size_t   byteCount;
   if (!PopBitvByteArraySize(buf, byteCount))
      Raise<BitvNeedsMore>("BitvToByteArray: needs more");
   if (barySize < byteCount)
      Raise<std::out_of_range>("BitvToByteArray: bary buffer too small.");
   if (buf.Read(bary, byteCount) != byteCount) {
      assert(!"BitvToByteArray: Unexpected read size!");
   }
   if (size_t szTail = barySize - byteCount)
      memset(reinterpret_cast<byte*>(bary) + byteCount, 0, szTail);
   return byteCount;
}

//--------------------------------------------------------------------------//

fon9_API void BitvToChar(DcQueue& buf, char& out) {
   if (const fon9_BitvT* ptype = reinterpret_cast<const fon9_BitvT*>(buf.Peek1())) {
      if (static_cast<fon9_BitvV>(*ptype) == fon9_BitvV_Char0) {
         out = '\0';
         buf.PopConsumed(1u);
         return;
      }
      if (fon9_UNLIKELY(*ptype != 0))
         Raise<BitvTypeNotMatch>("BitvToChar: type not match");
      // ByteArray,size=1
      fon9_BitvT tvalue[2];
      if (const char* pbuf = static_cast<const char*>(buf.Peek(&tvalue, 2))) {
         out = pbuf[1];
         buf.PopConsumed(2u);
         return;
      }
      // exception: needs more.
   }
   Raise<BitvNeedsMore>("BitvToChar: needs more");
}

fon9_API void BitvToBool(DcQueue& buf, bool& out) {
   if (const fon9_BitvV* pval = reinterpret_cast<const fon9_BitvV*>(buf.Peek1())) {
      if (*pval == fon9_BitvV_BoolTrue)
         out = true;
      else if (*pval == fon9_BitvV_BoolFalse)
         out = false;
      else
         Raise<BitvTypeNotMatch>("BitvToBool: type not match");
      buf.PopConsumed(1u);
      return;
   }
   Raise<BitvNeedsMore>("BitvToBool: needs more");
}

//--------------------------------------------------------------------------//

fon9_API int8_t BitvToInt(DcQueue& buf, int8_t nullValue) { return BitvToIntAux(buf, nullValue); }
fon9_API uint8_t BitvToInt(DcQueue& buf, uint8_t nullValue) { return BitvToIntAux(buf, nullValue); }
fon9_API int16_t BitvToInt(DcQueue& buf, int16_t nullValue) { return BitvToIntAux(buf, nullValue); }
fon9_API uint16_t BitvToInt(DcQueue& buf, uint16_t nullValue) { return BitvToIntAux(buf, nullValue); }
fon9_API int32_t BitvToInt(DcQueue& buf, int32_t nullValue) { return BitvToIntAux(buf, nullValue); }
fon9_API uint32_t BitvToInt(DcQueue& buf, uint32_t nullValue) { return BitvToIntAux(buf, nullValue); }
fon9_API int64_t BitvToInt(DcQueue& buf, int64_t nullValue) { return BitvToIntAux(buf, nullValue); }
fon9_API uint64_t BitvToInt(DcQueue& buf, uint64_t nullValue) { return BitvToIntAux(buf, nullValue); }

fon9_API size_t BitvToContainerElementCount(DcQueue& buf) {
   return BitvToIntAux<size_t,
      fon9_BitvT_Container,
      fon9_BitvV_ContainerEmpty,
      fon9_BitvV_ContainerEmpty>(buf, static_cast<size_t>(0));
}

//--------------------------------------------------------------------------//

fon9_API void BitvToNumber(DcQueue& buf, fon9_BitvNumR& numr) {
   const byte* const ptype = buf.Peek1();
   if (fon9_UNLIKELY(ptype == nullptr))
      Raise<BitvNeedsMore>("BitvToNumber: needs more");

   fon9_BitvT  type = static_cast<fon9_BitvT>(*ptype & fon9_BitvT_Mask);
   uintmax_t   numbuf[2];
   byte        byteCount;
   const byte* pnum;
   if (type == fon9_BitvT_IntegerPos) {
      numr.Type_ = fon9_BitvNumT_Pos;
   __GET_INT:
      numr.Scale_ = 0;
      byteCount = static_cast<byte>((*ptype & 0x07) + 1);
      if ((pnum = static_cast<const byte*>(buf.Peek(numbuf, byteCount + 1u))) == nullptr)
         Raise<BitvNeedsMore>("BitvToInteger: needs more");
      numr.Num_ = GetPackedBigEndian<uintmax_t>(pnum + 1, byteCount);
      buf.PopConsumed(byteCount + 1u);
      if (numr.Type_ == fon9_BitvNumT_Neg)
         numr.Num_ = ~numr.Num_;
      return;
   }
   if (type == fon9_BitvT_IntegerNeg) {
      numr.Type_ = fon9_BitvNumT_Neg;
      goto __GET_INT;
   }

   /// - v = `1011 0xxx` + `sssss N bb`;  2 bytes.
   if (type == fon9_BitvT_Decimal) {
      byteCount = static_cast<byte>((*ptype & 0x07) + 1);
      if ((pnum = static_cast<const byte*>(buf.Peek(numbuf, byteCount + 2u))) == nullptr)
         Raise<BitvNeedsMore>("BitvToDecimal: needs more");
      ++pnum;
      numr.Scale_ = static_cast<byte>(*pnum >> 3);
      numr.Type_ = (*pnum & 0x04) ? fon9_BitvNumT_Neg : fon9_BitvNumT_Pos;
      numr.Num_ = ((*const_cast<byte*>(pnum) &= 0x03) != 0) // 因為等一下就要從buffer移除此資料了, 所以使用 const_cast<> 沒關係.
         ? GetPackedBigEndian<uintmax_t>(pnum, static_cast<byte>(byteCount + 1))
         : GetPackedBigEndian<uintmax_t>(pnum + 1, byteCount);
      buf.PopConsumed(byteCount + 2u);
      if (numr.Type_ == fon9_BitvNumT_Neg)
         numr.Num_ = ~numr.Num_;
      return;
   }

   numr.Scale_ = 0;
   numr.Num_ = 0;
   if (static_cast<fon9_BitvV>(*ptype) == fon9_BitvV_Number0)
      numr.Type_ = fon9_BitvNumT_Zero;
   else if (static_cast<fon9_BitvV>(*ptype) == fon9_BitvV_NumberNull)
      numr.Type_ = fon9_BitvNumT_Null;
   else
      Raise<BitvTypeNotMatch>("BitvToNumber: type not match");
   buf.PopConsumed(1u);
}

} // namespace
