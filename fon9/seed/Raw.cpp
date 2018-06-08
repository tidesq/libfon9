/// \file fon9/seed/Raw.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/Raw.hpp"
#include "fon9/Blob.h"

namespace fon9 { namespace seed {

Raw::~Raw() {
   if (uint32_t L = this->GetDyBlobCount()) {
      byte*  blobPtr = reinterpret_cast<byte*>(this) + this->DyMemPos_;
      do {
         // 為了避免 blobPtr 記憶體沒有對齊, 所以複製一份處理.
         fon9_Blob blob = GetUnaligned(reinterpret_cast<fon9_Blob*>(blobPtr));
         fon9_Blob_Free(&blob);
         blobPtr += sizeof(fon9_Blob);
      } while (--L > 0);
   }
}

} } // namespaces
