// \file fon9/fix/FixCompID.cpp
// \author fonwinz@gmail.com
#include "fon9/fix/FixCompID.hpp"
#include "fon9/fix/FixParser.hpp"

namespace fon9 { namespace fix {

FixTag CompID::Check(const FixParser& fixmsg, FixTag tagCompID, FixTag tagSubID) const {
   if (!this->SubID_.empty()) {
      if (const FixParser::FixField* fldSubID = fixmsg.GetField(tagSubID)) {
         if (fldSubID->Value_ != ToStrView(this->SubID_))
            return tagSubID;
      }
      else
         return tagSubID;
   }
   if (const FixParser::FixField* fldCompID = fixmsg.GetField(tagCompID))
      if (fldCompID->Value_ == ToStrView(this->CompID_))
         return 0;
   return tagCompID;
}
FixTag CompIDs::Check(const FixParser& fixmsg) const {
   FixTag errTag = this->Sender_.Check(fixmsg, f9fix_kTAG_TargetCompID, f9fix_kTAG_TargetSubID);
   if (fon9_LIKELY(errTag == 0))
      errTag = this->Target_.Check(fixmsg, f9fix_kTAG_SenderCompID, f9fix_kTAG_SenderSubID);
   return errTag;
}

void CompIDs::Initialize(const StrView& senderCompID, const StrView& senderSubID,
                         const StrView& targetCompID, const StrView& targetSubID) {
   this->Sender_.Initialize(senderCompID, senderSubID);
   this->Target_.Initialize(targetCompID, targetSubID);
   size_t rsz = this->Sender_.CalcHeaderSize(f9fix_STRTAG(SenderCompID), f9fix_STRTAG(SenderSubID))
              + this->Target_.CalcHeaderSize(f9fix_STRTAG(TargetCompID), f9fix_STRTAG(TargetSubID));
   this->Header_.clear();
   if (byte* hdr = reinterpret_cast<byte*>(this->Header_.alloc(rsz))) {
      hdr = this->Sender_.PutHeader(hdr, f9fix_STRTAG(SenderCompID), f9fix_STRTAG(SenderSubID));
      this->Target_.PutHeader(hdr, f9fix_STRTAG(TargetCompID), f9fix_STRTAG(TargetSubID));
   }
}

} } // namespaces
