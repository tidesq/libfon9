// \file f9tws/Config.h
// \author fonwinz@gmail.com
#ifndef __f9tws_Config_h__
#define __f9tws_Config_h__
#include "fon9/sys/Config.h"
#ifdef __cplusplus
extern "C" {
#endif

#if defined(fon9_WINDOWS)
   /// C API 的呼叫方式, Windows: __stdcall
   #define f9tws_CAPI_CALL   __stdcall
#else//#if WIN #else...
   /// C API 的呼叫方式, Windows: __stdcall, unix: 空白(不須額外定義).
   #define f9tws_CAPI_CALL
#endif//#if WIN #else #endif

/// 設定 f9tws_API: MSVC 的 DLL import/export.
#if defined(f9tws_NODLL)
   #define f9tws_API
#elif defined(f9tws_EXPORT)
   #define f9tws_API    __declspec(dllexport)
#elif defined(f9tws_WINDOWS)
   #define f9tws_API    __declspec(dllimport)
#else//not defined: fon9_WINDOWS
   #define f9tws_API
#endif

#ifdef __cplusplus
}//extern "C"
#endif
#endif//__f9tws_Config_h__
