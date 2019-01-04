/// \file fon9/sys/Config.hpp
///
/// libfon9 的一些基本設定.
///
/// \author fonwinz@gmail.com

/// \defgroup AlNum       文數字
/// \defgroup Thrs        Thread
/// \defgroup Buffer      緩衝區機制
/// \defgroup Misc        其他工具
/// \defgroup fon9_MACRO  巨集定義
#ifndef __fon9_sys_Config_hpp__
#define __fon9_sys_Config_hpp__
#include "fon9/sys/Config.h"

fon9_BEFORE_INCLUDE_STD;
   // 預先 include 一些必要的 header
   #if defined(fon9_WINDOWS)
      #define _WINSOCKAPI_ // stops Windows.h including winsock.h
      #define NOMINMAX     // stops Windows.h #define max()
      #include <Windows.h>
   #else//#if fon9_WINDOWS #else...
      #include <unistd.h>
   #endif//#if WIN #else #endif

   // 若為 MSVC & _DEBUG 則開啟 memory leak 偵測.
   #if defined(_MSC_VER) && defined(_DEBUG)
      #define _CRTDBG_MAP_ALLOC
      #include <stdlib.h>
      #include <crtdbg.h>
      #ifndef DBG_NEW
         #define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
         #define new DBG_NEW
      #endif
   #else
      #include <stdlib.h>
   #endif

   #include <type_traits>
   #include "fon9/Assert.h"
fon9_AFTER_INCLUDE_STD;

/// \ingroup fon9_MACRO
/// 定義 template class, 並放入 libfon9.dll
#define fon9_API_TEMPLATE_CLASS(nameOfTypedef, classOfTemplate, ...) \
template class classOfTemplate<__VA_ARGS__> fon9_API;                \
typedef  class classOfTemplate<__VA_ARGS__> nameOfTypedef;

/// \ingroup fon9_MACRO
/// 使用巨集方式設定「禁止複製」.
/// 如果用繼承 boost::noncopyable 的作法, 可能會有不必要的警告, 例如:
///   - warning C4625: 'class name' : copy constructor could not be generated because a base class copy constructor is inaccessible or deleted
///   - warning C4626: 'class name' : assignment operator could not be generated because a base class assignment operator is inaccessible or deleted
#define fon9_NON_COPYABLE(className)        \
   className(const className& r) = delete; \
   className& operator=(const className& r) = delete;

/// \ingroup fon9_MACRO
/// 使用巨集方式設定「禁止move」.
#define fon9_NON_MOVEABLE(className)         \
   className(const className&& r) = delete; \
   className& operator=(const className&& r) = delete;

/// \ingroup fon9_MACRO
/// 使用巨集方式設定「禁止複製及move」.
#define fon9_NON_COPY_NON_MOVE(className) \
   fon9_NON_COPYABLE(className)           \
   fon9_NON_MOVEABLE(className)

namespace fon9 {
using byte = unsigned char;
} // namespace

#endif//__fon9_sys_Config_hpp__
