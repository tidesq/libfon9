/// \file fon9/seed/FieldSchCfgStr.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_FieldSchCfgStr_hpp__
#define __fon9_seed_FieldSchCfgStr_hpp__
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/SchTask.hpp"

namespace fon9 { namespace seed {

struct SchCfgStr : public CharVector {
   using base = CharVector;
   using base::base;
   SchCfgStr() = default;
};

template <class BaseField>
class FieldSchCfgStrT : public BaseField {
   fon9_NON_COPY_NON_MOVE(FieldSchCfgStrT);
   using base = BaseField;
public:
   using base::base;

   #define fon9_kCSTR_UDFieldMaker_SchCfgStr "Sch"
   StrView GetTypeId(NumOutBuf&) const override {
      return StrView{fon9_kCSTR_UDFieldMaker_Head fon9_kCSTR_UDFieldMaker_SchCfgStr};
   }
   OpResult StrToCell(const RawWr& wr, StrView value) const {
      if (StrTrim(&value).empty())
         base::StrToCell(wr, value);
      else {
         SchConfig cfg{value};
         base::StrToCell(wr, ToStrView(RevPrintTo<CharVector>(cfg)));
      }
      return OpResult::no_error;
   }
};


/// \ingroup seed
/// 存取 SchConfig 設定字串.
class fon9_API FieldSchCfgStr : public FieldSchCfgStrT<FieldString<SchCfgStr::base> > {
   fon9_NON_COPY_NON_MOVE(FieldSchCfgStr);
   using base = FieldSchCfgStrT<FieldString<SchCfgStr::base> >;
public:
   using base::base;
};

inline FieldSPT<FieldSchCfgStr> MakeField(Named&& named, int32_t ofs, SchCfgStr&) {
   return FieldSPT<FieldSchCfgStr>{new FieldSchCfgStr{std::move(named), ofs}};
}
inline FieldSPT<FieldSchCfgStr> MakeField(Named&& named, int32_t ofs, const SchCfgStr&) {
   return FieldSPT<FieldSchCfgStr>{new FieldConst<FieldSchCfgStr>{std::move(named), ofs}};
}

} } // namespaces
#endif//__fon9_seed_FieldSchCfgStr_hpp__
