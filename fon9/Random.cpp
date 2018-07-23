// \file fon9/Random.cpp
// \author fonwinz@gmail.com
#include "fon9/Random.hpp"
#include "fon9/Utility.hpp"
namespace fon9 {

/// \ingroup Misc
/// 在 function 裡面建立一個 static thread_local 物件.
/// 第一次呼叫時會自動建構, 但是此物件不會自動解構!
/// 所以僅適用於不須解構的物件.
#define fon9_THREAD_LOCAL_OBJ(T, name, ...)        \
static thread_local char   name##Mem_[sizeof(T)];  \
static thread_local T*     name{nullptr};          \
if (!name)                                         \
   name = fon9::InplaceNew<T>(name##Mem_, __VA_ARGS__);

fon9_API std::default_random_engine& GetRandomEngine() {
   fon9_THREAD_LOCAL_OBJ(std::default_random_engine, rng, std::random_device{}());
   return *rng;
}

fon9_API void RandomBytes(void* dst, size_t dstSize) {
   fon9_THREAD_LOCAL_OBJ(std::uniform_int_distribution<>, dist, 0, 0xff);
   std::default_random_engine& rng = GetRandomEngine();
   for (size_t L = 0; L < dstSize; ++L) {
      *reinterpret_cast<byte*>(dst) = static_cast<byte>((*dist)(rng));
      dst = reinterpret_cast<byte*>(dst) + 1;
   }
}

fon9_API void RandomChars(char* dst, size_t dstSize) {
   if (dstSize <= 0)
      return;
   static const char chs[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "+-*/%@"
      "!$&_?^";
   fon9_THREAD_LOCAL_OBJ(std::uniform_int_distribution<>, dist, 0, static_cast<int>(sizeof(chs) - 2));
   std::default_random_engine& rng = GetRandomEngine();
   for (size_t L = 0; L < dstSize; ++L)
      *dst++ = chs[static_cast<byte>((*dist)(rng))];
}

} // namespaces
