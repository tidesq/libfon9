/// \file fon9/fix/FixCompID.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_FixCompID_hpp__
#define __fon9_fix_FixCompID_hpp__
#include "fon9/fix/FixBase.hpp"
#include "fon9/Utility.hpp"
#include "fon9/CharVector.hpp"

namespace fon9 { namespace fix {

struct fon9_API CompID {
   CharVector  CompID_;
   CharVector  SubID_;

   void Initialize(const StrView& compID, const StrView& subID) {
      this->CompID_.assign(compID);
      this->SubID_.assign(subID);
   }
   size_t CalcHeaderSize(const StrView& ftagCompID, const StrView& ftagSubID) const {
      size_t   sz = ftagCompID.size() + this->CompID_.size();
      if (size_t subsz = this->SubID_.size())
         sz += subsz + ftagSubID.size();
      return sz;
   }
   byte* PutHeader(byte* hdr, const StrView& ftagCompID, const StrView& ftagSubID) const {
      hdr = PutFwd(hdr, ftagCompID);
      hdr = PutFwd(hdr, this->CompID_);
      if (size_t subsz = this->SubID_.size()) {
         hdr = PutFwd(hdr, ftagSubID);
         hdr = PutFwd(hdr, this->SubID_.begin(), subsz);
      }
      return hdr;
   }
   /// \retval tagCompID  CompID_ 不正確.
   /// \retval tagSubID   如果 !this->SubID_.empty() 且 fixmsg.GetField(tagSubID) 與 this->SubID_ 不同.
   /// \retval 0          CompID, SubID 正確無誤.
   FixTag Check(const FixParser& fixmsg, FixTag tagCompID, FixTag tagSubID) const;
};

/// \ingroup fix
/// 從本機的觀點看的 CompID.
/// - Sender: 本機端的 CompID.
/// - Target: 遠端的 CompID.
struct fon9_API CompIDs {
   CompID      Sender_;
   CompID      Target_;
   /// "|49=SenderCompID|50=SenderSubID|56=TargetCompID|57=TargetSubID"
   CharVector  Header_;

   CompIDs(const StrView& senderCompID, const StrView& senderSubID,
           const StrView& targetCompID, const StrView& targetSubID) {
      this->Initialize(senderCompID, senderSubID, targetCompID, targetSubID);
   }
   void Initialize(const StrView& senderCompID, const StrView& senderSubID,
                   const StrView& targetCompID, const StrView& targetSubID);

   /// 檢查收到的訊息 CompID 是否正確.
   /// this->Sender_.Check(fixmsg, f9fix_kTAG_TargetCompID, f9fix_kTAG_TargetSubID);
   /// this->Target_.Check(fixmsg, f9fix_kTAG_SenderCompID, f9fix_kTAG_SenderSubID);
   /// \retval f9fix_kTAG_SenderCompID
   /// \retval f9fix_kTAG_SenderSubID
   /// \retval f9fix_kTAG_TargetCompID
   /// \retval f9fix_kTAG_TargetSubID
   /// \retval 0  CompIDs的檢查符合期望.
   FixTag Check(const FixParser& fixmsg) const;
};

} } // namespaces
#endif//__fon9_fix_FixCompID_hpp__
