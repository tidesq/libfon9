/// \file fon9/Unaligned.hpp
///
/// 存取沒有[記憶體對齊]的資料.
///
/// - 直接存取沒有[記憶體對齊]的資料, 某些CPU可能會 Bus error,
///   所以使用 fon9::GetUnaligned(); fon9::PutUnaligned(); 支援這類情形.
/// - 直接使用 memcpy()? 因為較新的 compiler 都能最佳化 memcpy(dst,src,固定大小);
///   但是如果是 non-POD? 用 memcpy() 是否會有副作用?
///
/// \author fonwinz@gmail.com
#ifndef __fon9_Unaligned_hpp__
#define __fon9_Unaligned_hpp__
#include <string.h>

namespace fon9 {

#if defined(_MSC_VER) && defined(_WIN64)
template <typename T>
static inline T GetUnaligned(const T __unaligned* address) {
   return *address;
}
template <typename T>
static inline void PutUnaligned(T __unaligned* address, T value) {
   *address = value;
}
#else
template <typename T>
static inline T GetUnaligned(const T* address) {
   T v;
   memcpy(&v, address, sizeof(T));
   return v;
}
template <typename T>
static inline void PutUnaligned(T* address, T value) {
   memcpy(address, &value, sizeof(value));
}
#endif

} // namespace
#endif//__fon9_Unaligned_hpp__
