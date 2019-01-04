// \file fon9/fmkt/SymbBS.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbBS_hpp__
#define __fon9_fmkt_SymbBS_hpp__
#include "fon9/fmkt/SymbDy.hpp"
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/TimeStamp.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 商品資料的擴充: 行情的買賣報價.
class fon9_API SymbBS : public SymbData {
   fon9_NON_COPY_NON_MOVE(SymbBS);
public:
   enum {
      /// 買賣價量列表數量.
      kBSCount = 5,
   };
   struct Data {
      /// 報價時間.
      TimeInterval   Time_{TimeInterval::Null()};
      /// 賣出價量列表, [0]=最佳賣出價量.
      PriQty         Sells_[kBSCount];
      /// 買進價量列表, [0]=最佳買進價量.
      PriQty         Buys_[kBSCount];
   };
   Data  Data_;
   
   SymbBS(const Data& rhs) : Data_{rhs} {
   }
   SymbBS() = default;

   static seed::Fields MakeFields();
};

class SymbBSTabDy : public SymbDataTab {
   fon9_NON_COPY_NON_MOVE(SymbBSTabDy);
   using base = SymbDataTab;
public:
   SymbBSTabDy(Named&& named)
      : base{std::move(named), SymbBS::MakeFields(), seed::TabFlag::NoSapling_NoSeedCommand_Writable} {
   }

   SymbDataSP FetchSymbData(Symb&) override;
};

} } // namespaces
#endif//__fon9_fmkt_SymbBS_hpp__
