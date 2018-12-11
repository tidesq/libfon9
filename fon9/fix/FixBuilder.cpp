// \file fon9/fix/FixBuilder.cpp
// \author fonwinz@gmail.com
#include "fon9/fix/FixBuilder.hpp"

namespace fon9 { namespace fix {

BufferList FixBuilder::Final(const StrView& beginHeader) {
   assert(this->CheckSumPos_ != nullptr);
   const size_t bodyLength = CalcDataSize(this->Buffer_.cfront()) - kFixTailWidth;
   RevPrint(this->Buffer_, beginHeader, bodyLength);

   char* const psum = this->CheckSumPos_;
   this->CheckSumPos_ = nullptr;

   const BufferNode* cfront = this->Buffer_.cfront();
   byte  cks = f9fix_kCHAR_SPL;
   while (cfront) {
      const byte* pend = cfront->GetDataEnd();
      const byte* pbeg = cfront->GetDataBegin();
      if ((cfront = cfront->GetNext()) == nullptr)
         pend = reinterpret_cast<byte*>(psum);
      for (; pbeg != pend; ++pbeg)
         cks = static_cast<byte>(cks + *pbeg);
   }

   this->PutCheckSumField(psum, cks);
   return this->Buffer_.MoveOut();
}

} } // namespaces
