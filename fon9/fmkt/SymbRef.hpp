// \file fon9/SymbRef.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbRef_hpp__
#define __fon9_fmkt_SymbRef_hpp__
#include "fon9/fmkt/SymbDy.hpp"
#include "fon9/fmkt/FmktTypes.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 商品資料的擴充: 參考價.
class fon9_API SymbRef : public SymbData {
   fon9_NON_COPY_NON_MOVE(SymbRef);
public:
   SymbRef() = default;

   Pri   PriRef_;
   Pri   PriUpLmt_;
   Pri   PriDnLmt_;

   static seed::Fields MakeFields();
};

class SymbRefTabDy : public SymbDataTab {
   fon9_NON_COPY_NON_MOVE(SymbRefTabDy);
   using base = SymbDataTab;
public:
   SymbRefTabDy(Named&& named)
      : base{std::move(named), SymbRef::MakeFields(), seed::TabFlag::NoSapling_NoSeedCommand_Writable} {
   }

   SymbDataSP FetchSymbData(Symb&) override;
};

} } // namespaces
#endif//__fon9_fmkt_SymbRef_hpp__
