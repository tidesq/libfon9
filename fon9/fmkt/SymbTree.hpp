// \file fon9/SymbTree.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_SymbTree_hpp__
#define __fon9_fmkt_SymbTree_hpp__
#include "fon9/fmkt/Symb.hpp"
#include "fon9/seed/Tree.hpp"

namespace fon9 { namespace fmkt {

class fon9_API SymbTree : public seed::Tree {
   fon9_NON_COPY_NON_MOVE(SymbTree);
   using base = seed::Tree;

   class PodOp;
   class TreeOp;

public:
   using SymbMapImpl = fmkt::SymbMap;
   using iterator = SymbMapImpl::iterator;
   using SymbMap = MustLock<SymbMapImpl>;
   SymbMap  SymbMap_;

   using base::base;
   void OnTreeOp(seed::FnTreeOp fnCallback) override;
   void OnParentSeedClear() override;

   /// 衍生者必須傳回有效的 SymbSP;
   virtual SymbSP MakeSymb(const StrView& symbid) = 0;
};

} } // namespaces
#endif//__fon9_fmkt_SymbTree_hpp__
