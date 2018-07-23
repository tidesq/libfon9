/// \file fon9/Random.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_Random_hpp__
#define __fon9_Random_hpp__
#include "fon9/sys/Config.hpp"

fon9_BEFORE_INCLUDE_STD;
#include <random>
fon9_AFTER_INCLUDE_STD;

namespace fon9 {

/// \ingroup Misc
/// 在 dst[0..dstSize-1] 填入亂數.
fon9_API void RandomBytes(void* dst, size_t dstSize);

/// \ingroup Misc
/// 在 dst[0..dstSize) 任意填入以下 ASCII 字元, 不包含 EOS.
/// - '0'..'9', 'A'..'Z', 'a'..'z'
/// - '+', '-', '*', '/', '%', '@'
/// - '!', '$', '&', '_', '?', '^'
fon9_API void RandomChars(char* dst, size_t dstSize);

/// \ingroup Misc
/// = RandomChars(dst, dstSize-1); dst[dstSize-1] = 0;
inline void RandomString(char* dst, size_t dstSize) {
   if (fon9_LIKELY(dstSize > 0)) {
      dst[dstSize - 1] = 0;
      if (fon9_LIKELY(dstSize > 1))
         RandomChars(dst, dstSize - 1);
   }
}

/// \ingroup Misc
/// 取得一個 thrdad_local 的 std::default_random_engine
fon9_API std::default_random_engine& GetRandomEngine();

} // namespaces
#endif//__fon9_Random_hpp__
