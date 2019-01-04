// \file fon9/fmkt/SymbDy.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbDy_hpp__
#define __fon9_fmkt_SymbDy_hpp__
#include "fon9/fmkt/SymbTree.hpp"

namespace fon9 { namespace fmkt {

class fon9_API SymbDyTree;
using SymbDyTreeSP = intrusive_ptr<SymbDyTree>;

/// \ingroup fmkt
/// 使用 LayoutDy 動態建立所需的資料.
/// 對於一個追求速度(而非彈性)的系統,
/// 可以將商品所需用到的 Layout, 在設計階段使用 LayoutN 確定好,
/// 所以 LayoutDy(動態建立 SymbData) 並非絕對必要.
/// 若使用 LayoutDy 則每次要用到 SymbData 時, 需要額外的 lock.
class fon9_API SymbDy : public Symb {
   fon9_NON_COPY_NON_MOVE(SymbDy);
   using base = Symb;
   using SymbDataSP = std::unique_ptr<SymbData>;
   using ExDatasImpl = std::vector<SymbDataSP>;
   using ExDatas = MustLock<ExDatasImpl>;
   ExDatas  ExDatas_;

public:
   const SymbDyTreeSP  SymbTreeSP_;
   SymbDy(SymbDyTreeSP symbTreeSP, const StrView& symbid);

   SymbData* GetSymbData(int tabid) override;
   SymbData* FetchSymbData(int tabid) override;
};

class fon9_API SymbDataTab : public seed::Tab {
   fon9_NON_COPY_NON_MOVE(SymbDataTab);
   using base = seed::Tab;
public:
   using base::base;

   using SymbDataSP = std::unique_ptr<SymbData>;
   virtual SymbDataSP FetchSymbData(Symb& symb) = 0;
};
using SymbDataTabSP = intrusive_ptr<SymbDataTab>;

class fon9_API SymbDyTree : public SymbTree {
   fon9_NON_COPY_NON_MOVE(SymbDyTree);
   using base = SymbTree;

public:
   using base::base;

   bool AddSymbDataTab(SymbDataTabSP tab) {
      auto layout = static_cast<seed::LayoutDy*>(this->LayoutSP_.get())->Lock();
      return layout->Add(std::move(tab));
   }

   SymbSP MakeSymb(const StrView& symbid) override;
};

} } // namespaces
#endif//__fon9_fmkt_SymbDy_hpp__
