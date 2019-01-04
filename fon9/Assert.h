/// \file fon9/Assert.h
/// \author fonwinz@gmail.com
#ifndef __fon9_Assert_h__
#define __fon9_Assert_h__
#include "fon9/sys/Config.h"
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(fon9_WINDOWS) && !defined(NDEBUG)
   #undef assert
   #define assert(expression) (void)(                                                       \
            (!!(expression)) ||                                                              \
            (fon9_wassert(_CRT_WIDE(#expression), _CRT_WIDE(__FILE__), (unsigned)(__LINE__)), 0) \
        )

   // 為了 debug 時, 可以在這裡可以設定中斷, 所以改寫了 assert().
   fon9_API void fon9_wassert(
      _In_z_ wchar_t const* _Message,
      _In_z_ wchar_t const* _File,
      _In_   unsigned       _Line
   );
#endif

#ifdef __cplusplus
}// extern "C"
#endif
#endif//__fon9_Assert_h__
