// \file fon9/fix/FixBuilder.cpp
// \author fonwinz@gmail.com
#include "fon9/fix/FixBuilder.hpp"

namespace fon9 { namespace fix {

void FixBuilder::PutUtcNow() {
   if (this->TimeFIXMS_)
      RevPutMem(this->Buffer_, this->TimeFIXMS_, kDateTimeStrWidth_FIXMS);
   else {
      RevPut_TimeFIXMS(this->Buffer_, this->Time_ = UtcNow());
      this->TimeFIXMS_ = this->Buffer_.GetCurrent();
   }
}

BufferList FixBuilder::Final(const StrView& beginHeader) {
   assert(this->CheckSumPos_ != nullptr);
   const size_t bodyLength = CalcDataSize(this->Buffer_.cfront()) - kFixTailWidth;
   RevPrint(this->Buffer_, beginHeader, bodyLength);

   char* const psum = this->CheckSumPos_;
   this->CheckSumPos_ = nullptr;
   this->TimeFIXMS_ = nullptr;

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
