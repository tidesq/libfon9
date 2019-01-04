// \file fon9/fmkt/SymbDeal.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbDeal_hpp__
#define __fon9_fmkt_SymbDeal_hpp__
#include "fon9/fmkt/SymbDy.hpp"
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/TimeStamp.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 商品資料的擴充: 行情的一筆成交資料.
class fon9_API SymbDeal : public SymbData {
   fon9_NON_COPY_NON_MOVE(SymbDeal);
public:
   struct Data {
      /// 成交時間.
      TimeInterval   Time_{TimeInterval::Null()};
      /// 單筆成交價量.
      /// this->Deal_.Qty_ 不一定等於 this->TotalQty_ - prev->TotalQty_;
      /// 因為行情資料可能有漏.
      PriQty         Deal_{};
      /// 累計成交量.
      Qty            TotalQty_{};
   };
   Data  Data_;

   SymbDeal(const Data& rhs) : Data_{rhs} {
   }
   SymbDeal() = default;

   static seed::Fields MakeFields();
};

class SymbDealTabDy : public SymbDataTab {
   fon9_NON_COPY_NON_MOVE(SymbDealTabDy);
   using base = SymbDataTab;
public:
   SymbDealTabDy(Named&& named)
      : base{std::move(named), SymbDeal::MakeFields(), seed::TabFlag::NoSapling_NoSeedCommand_Writable} {
   }

   SymbDataSP FetchSymbData(Symb&) override;
};

} } // namespaces
#endif//__fon9_fmkt_SymbDeal_hpp__
