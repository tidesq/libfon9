// \file fon9/SymbIn.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbIn_hpp__
#define __fon9_fmkt_SymbIn_hpp__
#include "fon9/fmkt/SymbTree.hpp"
#include "fon9/fmkt/SymbRef.hpp"
#include "fon9/fmkt/SymbBS.hpp"
#include "fon9/fmkt/SymbDeal.hpp"

namespace fon9 { namespace fmkt {

class fon9_API SymbInTree;
using SymbInTreeSP = intrusive_ptr<SymbInTree>;

/// \ingroup fmkt
/// 所有商品資料直接機中在此:
/// - 適用於: 追求速度(而非彈性)的系統.
/// - 這裡僅是範例: 只包含 SymbRef
class SymbIn : public Symb {
   fon9_NON_COPY_NON_MOVE(SymbIn);
   using base = Symb;
public:
   SymbRef  Ref_;
   SymbBS   BS_;
   SymbDeal Deal_;

   using base::base;

   SymbData* GetSymbData(int tabid) override;
   SymbData* FetchSymbData(int tabid) override;
};

class SymbInTree : public SymbTree {
   fon9_NON_COPY_NON_MOVE(SymbInTree);
   using base = SymbTree;

public:
   using base::base;

   SymbSP MakeSymb(const StrView& symbid) override;
};

} } // namespaces
#endif//__fon9_fmkt_SymbIn_hpp__
