// \file fon9/Blob.c
// \author fonwinz@gmail.com
#include "fon9/Blob.h"

fon9_MSC_WARN_DISABLE(4820);
#include <malloc.h>
#include <string.h>
fon9_MSC_WARN_POP;
typedef uint8_t   byte;

void fon9_CAPI_CALL fon9_Blob_Free(fon9_Blob* blob) {
   if (blob->MemPtr_)
      free(blob->MemPtr_);
   memset(blob, 0, sizeof(*blob));
}
int fon9_CAPI_CALL fon9_Blob_Append(fon9_Blob* blob, const void* app, fon9_Blob_Size_t appsz) {
   size_t newUsed = blob->MemUsed_ + appsz;
   if (newUsed > blob->MemSize_) {
      size_t newsz = newUsed + 128;
      if ((newsz & 0xffffffff) != newsz)//overflow!
         return 0;
      byte*  newptr = realloc(blob->MemPtr_, newsz + 1);
      if (newptr == NULL)
         return 0;
      if ((void*)(blob->MemPtr_) <= app && app < (void*)(blob->MemPtr_ + blob->MemSize_))
         app = newptr + ((byte*)app - blob->MemPtr_);
      blob->MemPtr_ = newptr;
      blob->MemSize_ = (uint32_t)newsz;
   }
   if (app) {
      memmove(blob->MemPtr_ + blob->MemUsed_, app, appsz);
      blob->MemUsed_ = (uint32_t)newUsed;
      *(blob->MemPtr_ + newUsed) = '\0';
   }
   return 1;
}
int fon9_CAPI_CALL fon9_Blob_Set(fon9_Blob* blob, const void* mem, fon9_Blob_Size_t memsz) {
   if (blob->MemSize_ < memsz) {
      // 一般而言使用 fon9_Blob_Set() 分配 [剛好] 的用量即可.
      //  - 若 mem != null, 則表示填入一塊固定資料, 不大會再追加, 即使再有追加, 則在 fon9_Blob_Append() 調整即可.
      //  - 若 mem == null, 則表示呼叫時 memsz 已有自己的預留量.
      size_t newsz = memsz;
      if ((newsz & 0xffffffff) != newsz) {//overflow!
         blob->MemUsed_ = 0;
         return 0;
      }
      if (blob->MemPtr_)
         free(blob->MemPtr_);
      if ((blob->MemPtr_ = malloc(newsz + 1)) == NULL) {
         blob->MemSize_ = blob->MemUsed_ = 0;
         return 0;
      }
      blob->MemSize_ = (uint32_t)newsz;
   }
   blob->MemUsed_ = (uint32_t)memsz;
   if (mem)
      memmove(blob->MemPtr_, mem, memsz);
   if (blob->MemPtr_) // if (blob is empty && memsz==0) 就不需要填 '\0'
      *((char*)(blob->MemPtr_) + memsz) = '\0';
   return 1;
}
int fon9_CAPI_CALL fon9_CompareBytes(const void* lhs, size_t lhsSize, const void* rhs, size_t rhsSize) {
   if (lhsSize == rhsSize)
      return lhsSize <= 0 ? 0 : memcmp(lhs, rhs, lhsSize);
   int cmpr;
   if (lhsSize < rhsSize) {
      if (lhsSize <= 0)
         return -1;
      cmpr = memcmp(lhs, rhs, lhsSize);
      return(cmpr == 0 ? -1 : cmpr);
   }
   if (rhsSize <= 0)
      return 1;
   cmpr = memcmp(lhs, rhs, rhsSize);
   return(cmpr == 0 ? 1 : cmpr);
}
