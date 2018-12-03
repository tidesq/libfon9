// \file fon9/fix/FixParser.cpp
// \author fonwinz@gmail.com
#include "fon9/fix/FixParser.hpp"
#include "fon9/StrTo.hpp"

namespace fon9 { namespace fix {

FixParser::FixParser() {
   // 預先分配必用(常用)的欄位: 1..511.
   // 0..0xff(255)
   this->FieldArray_[8];// "8=FIX.4.2"
   // 0x100..0x1ff(511)
   this->FieldArray_[0x100];
}
void FixParser::Clear() {
   this->MsgSeqNum_ = 0;
   this->ExpectSize_ = 0;
   this->MIndexNext_ = 0;
   if (this->FieldList_.empty())
      return;
   for (FixField* fld : this->FieldList_)
      fld->ValueCount_ = 0;
   this->FieldList_.clear();
}

FixParser::Result FixParser::Verify(StrView& fixmsg, VerifyItem vitem) {
   const size_t msgsz = fixmsg.size();
   if (this->ExpectSize_ > 0 && msgsz < this->ExpectSize_)
      return NeedsMore;

   uint32_t    expHeaderWidth = static_cast<uint32_t>(this->ExpectHeader_.size());
   const char* pbeg = fixmsg.begin();
   const char* msgbeg = pbeg;
   if (fon9_LIKELY(expHeaderWidth > 0)) {
      if (fon9_UNLIKELY(msgsz <= expHeaderWidth + kFixTailWidth)) {
         this->ExpectSize_ = static_cast<ExpectSize>(expHeaderWidth + kFixTailWidth + 1);
         return NeedsMore;
      }
      if (IsEnumContains(vitem, VerifyHeader)
          && fon9_CompareBytes(this->ExpectHeader_.begin(), expHeaderWidth, msgbeg, expHeaderWidth) != 0)
         return EInvalidHeader;
      msgbeg += expHeaderWidth;
   }
   else {
      // 尋找 "8=x|9=n|"
      if (msgsz <= kFixMinHeaderWidth + kFixTailWidth) {
         this->ExpectSize_ = kFixMinHeaderWidth + kFixTailWidth + 1;
         return NeedsMore;
      }
      if (msgbeg[0] != '8' || msgbeg[1] != '=')
         return EInvalidHeader;
      msgbeg = reinterpret_cast<const char*>(memchr(msgbeg + 2, f9fix_kCHAR_SPL, msgsz - 2));
      if (!msgbeg)
         return EInvalidHeader;
      if (msgbeg[1] != '9' || msgbeg[2] != '=')
         return EInvalidHeader;
      this->ExpectHeader_.assign(fixmsg.begin(), msgbeg += 3);
      expHeaderWidth = static_cast<uint32_t>(this->ExpectHeader_.size());
   }
   fixmsg.SetBegin(msgbeg);
   const size_t   bodyLength = StrTo(&fixmsg, 0u);
   // 尋找 header 時, 已經有預留足夠長度,
   // 所以: 如果沒有 "9=" bodyLength "|" 的結束字元, 就視為錯誤!
   if (fixmsg.Get1st() != f9fix_kCHAR_SPL)
      return EInvalidHeader;
   // 檢查 CheckSum.
   uint32_t    expsz = static_cast<uint32_t>((fixmsg.begin() - pbeg) + bodyLength);
   const char* const pend = pbeg + expsz;
   expsz += kFixTailWidth;
   if (msgsz < expsz) {
      fixmsg.SetBegin(pbeg);
      this->ExpectSize_ = static_cast<ExpectSize>(expsz);
      return NeedsMore;
   }
   if (IsEnumContains(vitem, VerifyCheckSum)) {
      //  |10=xxx
      // [0123456]
      if (pend[0] != f9fix_kCHAR_SPL
          || pend[1] != '1'
          || pend[2] != '0'
          || pend[3] != '=')
         return EFormat;
      byte cks = Pic9StrTo<3, byte>(pend + 4);// static_cast<byte>(((pend[4] - '0') * 10 + (pend[5] - '0')) * 10 + (pend[6] - '0'));
      while (pbeg < pend)
         cks = static_cast<byte>(cks - static_cast<byte>(*pbeg++));
      if (cks != f9fix_kCHAR_SPL) {
         this->Clear();
         this->ExpectSize_ = static_cast<ExpectSize>(expsz);
         return ECheckSum;
      }
   }
   fixmsg.Reset(fixmsg.begin() + 1, pend); //+1 = 移除 f9fix_kCHAR_SPL.
   return static_cast<Result>(expsz);
}
FixParser::Result FixParser::Parse(StrView& fixmsg, Until until) {
   Result rcode = this->Verify(fixmsg, until== Until::FullMessage ? VerifyAll : VerifyLengthOnly);
   if (rcode == NeedsMore || rcode == ECheckSum)
      return rcode;
   this->Clear();
   if (rcode < NeedsMore)
      return rcode;
   Result pcode = this->ParseFields(fixmsg, until);
   if (pcode < 0) // 發生錯誤, 傳回錯誤代碼.
      return pcode;
   return rcode;
}
FixParser::Result FixParser::ParseFields(StrView& fixmsg, Until until) {
   const char* msgend = fixmsg.end();
   while (fixmsg.begin() < msgend) {
      const FixTag tag = StrTo(&fixmsg, 0u);
      if (fon9_UNLIKELY(tag == 0 || fixmsg.Get1st() != '='))
         return EFormat;
      fixmsg.SetBegin(fixmsg.begin() + 1); //移除 '='

      FixField& fld = this->FieldArray_[tag];
      if (fon9_UNLIKELY(fld.ValueCount_ >= kMaxDupFieldCount + 1))
         return EDupField;
      fld.Tag_ = tag;
      StrView* pValue;
      if (fon9_LIKELY(fld.ValueCount_ == 0))
         pValue = &fld.Value_;
      else {
         if (fld.ValueCount_ == 1) {
            fld.MIndex_ = this->MIndexNext_;
            fon9_GCC_WARN_DISABLE("-Wconversion"); // warning: conversion to ‘uint16_t’ from ‘int’ may alter its value[-Wconversion]
            this->MIndexNext_ += kMaxDupFieldCount;
            fon9_GCC_WARN_POP;
            if (this->MFields_.size() < this->MIndexNext_)
               this->MFields_.resize(this->MIndexNext_ + kMaxDupFieldCount * 4u);
         }
         pValue = &this->MFields_[fld.MIndex_ * static_cast<size_t>(kMaxDupFieldCount) + (fld.ValueCount_ - 1)];
      }

      if (fon9_LIKELY(tag != f9fix_kTAG_RawData))
         *pValue = StrFetchNoTrim(fixmsg, f9fix_kCHAR_SPL);
      else { // RawData 可包含任意字元, 所以要用 RawDataLength 來判斷長度.
         const FixField* fldRawDataLength = GetField(f9fix_kTAG_RawDataLength);
         if (fon9_UNLIKELY(!fldRawDataLength)) {
            // 如果沒有 RawDataLength 則使用分隔字元.
            *pValue = StrFetchNoTrim(fixmsg, f9fix_kCHAR_SPL);
            // return ERawData;
         }
         else {
            uint32_t rawDataLength = StrTo(fldRawDataLength->Value_, static_cast<uint32_t>(-1));
            if (fon9_UNLIKELY(rawDataLength > fixmsg.size()))
               return ERawData;
            if (fon9_UNLIKELY(fixmsg.begin()[rawDataLength] != f9fix_kCHAR_SPL))
               return ERawData;
            pValue->Reset(fixmsg.begin(), fixmsg.begin() + rawDataLength);
            fixmsg.SetBegin(pValue->end() + 1);// +1 移除 f9fix_kCHAR_SPL
         }
      }
      ++fld.ValueCount_;
      this->FieldList_.push_back(&fld);
      if (fon9_LIKELY(until == Until::FullMessage))
         continue;
      switch (tag) {
      default: continue;
      #define CASE_TAG_UNTIL(tagName)  case f9fix_kTAG_##tagName: until -= Until::tagName;  break
         CASE_TAG_UNTIL(MsgSeqNum);
         CASE_TAG_UNTIL(MsgType);
         CASE_TAG_UNTIL(SendingTime);
      #undef CASE_TAG_UNTIL
      }
      if (until == Until::FullMessage)
         break;
   }
   const FixField* fldMsgSeqNum = this->GetField(f9fix_kTAG_MsgSeqNum);
   this->MsgSeqNum_ = (fldMsgSeqNum ? StrTo(fldMsgSeqNum->Value_, 0u) : 0);
   return ParseEnd;
}
StrView FixParser::GetValue(const FixField& fld, unsigned index) const {
   return index >= fld.ValueCount_ ? StrView{nullptr}
      : index == 0 ? fld.Value_
      : this->MFields_[fld.MIndex_ * kMaxDupFieldCount + (index - 1)];
}

} } // namespaces
