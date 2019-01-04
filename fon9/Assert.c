/// \file fon9/Assert.c
/// \author fonwinz@gmail.com
#include "fon9/Assert.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(fon9_WINDOWS) && !defined(NDEBUG)
fon9_API void fon9_wassert(
   _In_z_ wchar_t const* _Message,
   _In_z_ wchar_t const* _File,
   _In_   unsigned       _Line
) {
   _wassert(_Message, _File, _Line);
}
#endif

#ifdef __cplusplus
}// extern "C"
#endif
