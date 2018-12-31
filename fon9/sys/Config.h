/// \file fon9/sys/Config.h
///
/// libfon9 C API 的一些基本設定.
///
/// \author fonwinz@gmail.com
#ifndef __fon9_sys_Config_h__
#define __fon9_sys_Config_h__
#ifdef __cplusplus
extern "C" {
#endif

   #if defined(_WIN32) || defined(_WIN64)
      /// Windows環境的定義.
      #define fon9_WINDOWS
      /// C API 的呼叫方式, Windows: __stdcall
      #define fon9_CAPI_CALL   __stdcall
   #else//#if WIN #else...
      /// 非Windows環境, 一般而言是 POSIX.
      #define fon9_POSIX
      /// C API 的呼叫方式, Windows: __stdcall, unix: 空白(不須額外定義).
      #define fon9_CAPI_CALL
   #endif//#if WIN #else #endif

   /// \ingroup fon9_MACRO
   /// 設定 fon9_API: MSVC 的 DLL import/export.
   #if defined(fon9_NODLL)
      #define fon9_API
   #elif defined(fon9_EXPORT)
      #define fon9_API    __declspec(dllexport)
   #elif defined(fon9_WINDOWS)
      #define fon9_API    __declspec(dllimport)
   #else//not defined: fon9_WINDOWS
      #define fon9_API
   #endif

   #if defined(_MSC_VER)
      /// 關閉 MSVC 的警告, 關閉前沒有先push現在 warn 設定.
      /// warnlist 使用空白分隔, 例如: fon9_MSC_WARN_DISABLE_NO_PUSH(4777 4888)
      #define fon9_MSC_WARN_DISABLE_NO_PUSH(warnlist)   __pragma(warning(disable: warnlist))
      /// 關閉 MSVC 的警告, 關閉前會先push現在 warn 設定.
      #define fon9_MSC_WARN_DISABLE(warnlist)           __pragma(warning(push)) fon9_MSC_WARN_DISABLE_NO_PUSH(warnlist)
      /// 還原 MSVC 的警告設定.
      #define fon9_MSC_WARN_POP                         __pragma(warning(pop))

      #define fon9_GCC_WARN_DISABLE_NO_PUSH(warn)
      #define fon9_GCC_WARN_DISABLE(warn)
      #define fon9_GCC_WARN_POP

      #define fon9_BEFORE_INCLUDE_STD  \
               fon9_MSC_WARN_DISABLE(\
                  4820 /* 'name' : '4' bytes padding added after data member 'name' */ \
                  4668 /* 'NTDDI_WIN7SP1' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif' */ \
                  4571 /* Informational : catch (...) semantics changed since Visual C++ 7.1; structured exceptions(SEH) are no longer caught */ \
                  4625 /* 'name' : copy constructor could not be generated because a base class copy constructor is inaccessible or deleted */ \
                  4626 /* 'name' : assignment operator could not be generated because a base class assignment operator is inaccessible or deleted */ \
                  4265 /* 'std::_Pad' : class has virtual functions, but destructor is not virtual */ \
                  4350 /* behavior change: '...' */ \
                  4711 /* function 'name' selected for automatic inline expansion */ \
                  4710 /* 'name' : function not inlined */ \
                  4371 /* 'std::_Pmf_wrap<...>' : layout of class may have changed from a previous version of the compiler due to better packing of member 'std::_Pmf_wrap<...>::_Mypmf' */ \
                  4365 /* 'return' : conversion from 'bool' to 'BOOLEAN', signed / unsigned mismatch */ \
                  4574 /* 'INCL_WINSOCK_API_TYPEDEFS' is defined to be '0': did you mean to use '#if INCL_WINSOCK_API_TYPEDEFS'? */ \
                  4355 /* 'this': used in base member initializer list */ \
                  5026 /* move constructor was implicitly defined as deleted */ \
                  5027 /* move assignment operator was implicitly defined as deleted */ \
               )
      #define fon9_AFTER_INCLUDE_STD      fon9_MSC_WARN_POP
      #define fon9_WARN_DISABLE_PADDING   fon9_MSC_WARN_DISABLE(4820)
      #define fon9_WARN_POP               fon9_MSC_WARN_POP

      fon9_MSC_WARN_DISABLE_NO_PUSH(
         4514 /* 'name' : unreferenced inline function has been removed */ \
         4710 /* 'name' : function not inlined */ \
         4711 /* function 'name' selected for automatic inline expansion */ \
         4251 /* needs to have dll-interface to be used by clients */ \
         4275 /* non dll-interface class '...' used as base for dll-interface class '...' */ \
      )
      /// 強迫 MSVC 輸出使用 utf8 編碼.
      #pragma execution_character_set("UTF-8")
   #else
      #define fon9_MSC_WARN_DISABLE_NO_PUSH(warnlist)
      #define fon9_MSC_WARN_DISABLE(warnlist)
      #define fon9_MSC_WARN_POP

      #define fon9_GCC_MAKE_DIAG(x)                       _Pragma(#x)
      /// warn 例: "-Wmissing-field-initializers" 須加上引號!
      #define fon9_GCC_WARN_DISABLE_NO_PUSH(warn)         fon9_GCC_MAKE_DIAG(GCC diagnostic ignored warn)
      /// 例: fon9_GCC_WARN_DISABLE("-Wmissing-field-initializers") 須加上引號!
      #define fon9_GCC_WARN_DISABLE(warn)                 fon9_GCC_MAKE_DIAG(GCC diagnostic push)   fon9_GCC_WARN_DISABLE_NO_PUSH(warn)
      #define fon9_GCC_WARN_POP                           fon9_GCC_MAKE_DIAG(GCC diagnostic pop)

      #define fon9_BEFORE_INCLUDE_STD
      #define fon9_AFTER_INCLUDE_STD
      #define fon9_WARN_DISABLE_PADDING                  fon9_GCC_MAKE_DIAG(GCC diagnostic push)
      #define fon9_WARN_POP                              fon9_GCC_WARN_POP
   #endif
   #define fon9_WARN_DISABLE_SWITCH                      fon9_MSC_WARN_DISABLE(4061 4062); fon9_GCC_WARN_DISABLE("-Wswitch")

   #ifdef _MSC_VER
      /// 讓 compiler 可以最佳化分支路徑.
      #define fon9_UNLIKELY(x)  x
      /// 讓 compiler 可以最佳化分支路徑.
      #define fon9_LIKELY(x)    x
   #else
      #define fon9_LIKELY(x)    __builtin_expect(!!(x), 1)
      #define fon9_UNLIKELY(x)  __builtin_expect(!!(x), 0)
   #endif

   #ifdef __cplusplus
      #define fon9_ENUM_underlying(type)   :type
   #else
      #define fon9_ENUM_underlying(type)
   #endif

   /// \ingroup AlNum
   /// 強迫把 x 變成字串, 例如: fon9_TOCSTR(xyz) 可得到 "xyz"; 不論 xyz 的內容為何!
   /// fon9_TOCSTR("ABC") 則會得到 "\"ABC\\"" 這樣的字串!
   // 上面寫成 "\"ABC\\"" 是因為配合 doxygen 的關係, 實際 C 的表示法是: "\"ABC\"".
   #define fon9_TOCSTR(x)       #x

   /// \ingroup AlNum
   /// `fon9_CTXTOCSTR(123)` 可得到 "123";
   /// 若有 \#`define xxx CTX` 則: `fon9_CTXTOCSTR(xxx)` 可得到 "CTX".
   /// 其餘則不論 x 為 enum 或 const; 都不會取出內容, 只會將 x 變成字串,
   /// 例如: `fon9_CTXTOCSTR(EnumValue1)` 只會得到 "EnumValue1".
   #define fon9_CTXTOCSTR(x)    fon9_TOCSTR(x)

   #if defined(_MSC_VER)
      #define fon9_PACK(n)      __pragma(pack(push,1))
      #define fon9_PACK_POP     __pragma(pack(pop))
   #else
      #define fon9_PACK(n)      _Pragma("pack(push,1)")
      #define fon9_PACK_POP     _Pragma("pack(pop)")
   #endif

#ifdef __cplusplus
}//extern "C"
#endif
#endif//__fon9_sys_Config_h__
