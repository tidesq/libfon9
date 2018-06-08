/// \file fon9/Base64.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_Base64_hpp__
#define __fon9_Base64_hpp__
#include "fon9/Outcome.hpp"

namespace fon9 {

using Base64Result = Outcome<size_t, std::errc>;

/// \ingroup AlNum
/// \return Base64編碼後所需的長度(不含EOS)
constexpr size_t Base64EncodeLengthNoEOS(size_t srcSize) {
   return (srcSize / 3 + ((srcSize % 3) != 0)) * 4;
}

/// \ingroup AlNum
/// \param dst     存放Base64編碼後的資料, 不含 CR LF, 不可為 nullptr.
/// \param dstSize dst的大小
/// \param src     要編碼的來源資料.
/// \param srcSize 來源的資料量
///
/// \retval 成功   編碼後的長度(不含EOS), 如果剛好等於dstSize, 則此時沒有EOS.
/// \retval std::errc::no_buffer_space    dst, dstSize 容量不足.
fon9_API Base64Result Base64Encode(char* dst, size_t dstSize, const void* src, size_t srcSize);

/// \ingroup AlNum
/// 取得 srcB64 解碼後所需的資料大小.
/// \retval 成功  存放解碼結果所需的資料大小.
/// \retval std::errc::bad_message  srcB64有不正確的字元.
///         僅允許 'A'..'Z', 'a'..'z', '0'..'9', '+', '/', 及尾端最多2個 '=';
fon9_API Base64Result Base64DecodeLength(const char* srcB64, size_t srcSize);

/// \ingroup AlNum
/// 計算長度為 srcSize 的字串, 解碼後所需的資料最大可能大小.
constexpr size_t Base64DecodeLength(size_t srcSize) {
   // ((srcSize * 6) + 7) / 8; 避免 overflow 的算法:
   return (6 * (srcSize >> 3)) + ((6 * (srcSize & 0x7) + 7) >> 3);
}

/// \ingroup AlNum
/// \param dst     存放Base64解碼後的資料, 不可為 nullptr.
/// \param dstSize dst的大小.
/// \param srcB64  Base64的字串.
/// \param srcSize src的大小(不含EOS).
///
/// \retval >0 解碼後的資料長度.
/// \retval std::errc::no_buffer_space  dst, dstSize 容量不足.
/// \retval std::errc::bad_message      srcB64有不正確的字元.
fon9_API Base64Result Base64Decode(void* dst, size_t dstSize, const char* srcB64, size_t srcSize);

/// \ingroup AlNum
/// 把 binary 資料用 Base64 填入 rbuf.
template <class RevBuffer>
inline void RevPutB64(RevBuffer& rbuf, const void* src, size_t srcSize) {
   if (size_t b64Size = Base64EncodeLengthNoEOS(srcSize)) {
      char* pout = rbuf.AllocPrefix(b64Size);
      pout -= b64Size;
      Base64Encode(pout, b64Size, src, srcSize);
      rbuf.SetPrefixUsed(pout);
   }
}

} // namespaces
#endif//__fon9_Base64_hpp__
