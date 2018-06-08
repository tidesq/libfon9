/// \file fon9/seed/FieldTimeStamp.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_FieldTimeStamp_hpp__
#define __fon9_seed_FieldTimeStamp_hpp__
#include "fon9/seed/FieldValueFmt.hpp"
#include "fon9/TimeStamp.hpp"

namespace fon9 { namespace seed {

/// \ingroup seed
/// 存取 TimeStamp 的欄位.
class fon9_API FieldTimeStamp : public FieldValueFmt<TimeStamp, FmtTS> {
   fon9_NON_COPY_NON_MOVE(FieldTimeStamp);
   using base = FieldValueFmt<TimeStamp, FmtTS>;
public:
   /// 建構:
   /// - 固定為 FieldType::TimeStamp; FieldSource::DataMember;
   FieldTimeStamp(Named&& named, int32_t ofs) : base{std::move(named), FieldType::TimeStamp, ofs, TimeStamp::Scale} {
   }
   /// 建構:
   /// - 固定為 FieldType::TimeStamp; FieldSource::DyMem;
   FieldTimeStamp(Named&& named) : base(std::move(named), FieldType::TimeStamp, TimeStamp::Scale) {
   }

   /// 傳回: "Ts";
   virtual StrView GetTypeId(NumOutBuf&) const override;
};

inline FieldSPT<FieldTimeStamp> MakeField(Named&& named, int32_t ofs, TimeStamp&) {
   return FieldSPT<FieldTimeStamp>{new FieldTimeStamp{std::move(named), ofs}};
}
inline FieldSPT<FieldTimeStamp> MakeField(Named&& named, int32_t ofs, const TimeStamp&) {
   return FieldSPT<FieldTimeStamp>{new FieldConst<FieldTimeStamp>{std::move(named), ofs}};
}

//--------------------------------------------------------------------------//

class fon9_API FieldTimeInterval : public FieldValueFmt<TimeInterval> {
   fon9_NON_COPY_NON_MOVE(FieldTimeInterval);
   using base = FieldValueFmt<TimeInterval>;
public:
   /// 建構:
   /// - 固定為 FieldType::TimeInterval; FieldSource::DataMember;
   FieldTimeInterval(Named&& named, int32_t ofs) : base{std::move(named), FieldType::TimeInterval, ofs, TimeInterval::Scale} {
   }
   /// 建構:
   /// - 固定為 FieldType::TimeInterval; FieldSource::DyMem;
   FieldTimeInterval(Named&& named) : base(std::move(named), FieldType::TimeInterval, TimeInterval::Scale) {
   }

   /// 傳回: "Ti";
   virtual StrView GetTypeId(NumOutBuf&) const override;
};

inline FieldSPT<FieldTimeInterval> MakeField(Named&& named, int32_t ofs, TimeInterval&) {
   return FieldSPT<FieldTimeInterval>{new FieldTimeInterval{std::move(named), ofs}};
}
inline FieldSPT<FieldTimeInterval> MakeField(Named&& named, int32_t ofs, const TimeInterval&) {
   return FieldSPT<FieldTimeInterval>{new FieldConst<FieldTimeInterval>{std::move(named), ofs}};
}

} } // namespaces
#endif//__fon9_seed_FieldTimeStamp_hpp__
