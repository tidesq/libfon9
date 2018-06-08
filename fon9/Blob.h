/// \file fon9/Blob.h
/// \author fonwinz@gmail.com
#ifndef __fon9_Blob_h__
#define __fon9_Blob_h__
#include "fon9/sys/Config.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// \ingroup Misc
/// fon9_Blob 的大小型別.
typedef uint32_t  fon9_Blob_Size_t;

/// \ingroup Misc
/// 儲存一塊動態長度資料的結構.
typedef struct fon9_Blob {
   /// 動態長度資料的位置.
   uint8_t*          MemPtr_;
   /// 動態長度資料區塊的大小.
   /// 不包含尾端 EOS '\0', 所以實際分配的記憶體大小是 (MemSize_ + 1)
   fon9_Blob_Size_t MemSize_;
   /// 動態長度資料的使用量.
   /// 不包含尾端 EOS '\0'
   fon9_Blob_Size_t MemUsed_;
} fon9_Blob;

/// \ingroup Misc
/// 釋放 fon9_Blob 的記憶體, 釋放後清除 blob 的內容.
extern fon9_API void fon9_CAPI_CALL fon9_Blob_Free(fon9_Blob* blob);

/// \ingroup Misc
/// 將 app 資料加入尾端, 如果原本記憶體不足, 則會重新分配記憶體.
/// 固定會在加入 app 之後增加一個 EOS '\0'(但不列入 MemUsed_), 所以可以安心地加入字串.
/// 如果 app == nullptr: 則在尾端保留至少 appsz bytes 尚未初始化的空間, 且 blob->MemUsed_ 不會變動!
/// \retval 1 成功加入 app.
/// \retval 0 記憶體不足: 原本資料不變.
extern fon9_API int fon9_CAPI_CALL fon9_Blob_Append(fon9_Blob* blob, const void* app, fon9_Blob_Size_t appsz);

/// \ingroup Misc
/// 將 mem 資料填入 blob, 若 mem==NULL, 則表示僅重新設定 MemUsed_.
/// - 固定在尾端增加 EOS '\0': MemPtr_[memsz] = '\0'
/// - 一般而言使用 fon9_Blob_Set() 只會分配 [剛好] 的用量.
///
/// \retval 1 成功將 mem 填入 blob.
/// \retval 0 記憶體不足: 清除原本資料.
extern fon9_API int fon9_CAPI_CALL fon9_Blob_Set(fon9_Blob* blob, const void* mem, fon9_Blob_Size_t memsz);

/// \ingroup Misc
/// 比較2塊 mem 的內容.
/// \retval 1  lhs >  rhs
/// \retval 0  lhs == rhs
/// \retval -1 lhs <  rhs
extern fon9_API int fon9_CAPI_CALL fon9_CompareBytes(const void* lhs, size_t lhsSize, const void* rhs, size_t rhsSize);

#ifdef __cplusplus
}// extern "C"
#endif
#endif//__fon9_Blob_h__
