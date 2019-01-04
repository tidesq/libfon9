// \file fon9/fmkt/SymbTree.hpp
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

   SymbSP FetchSymb(const SymbMap::Locker& symbs, const StrView& symbid);
   SymbSP FetchSymb(const StrView& symbid) {
      return this->FetchSymb(this->SymbMap_.Lock(), symbid);
   }

   static SymbSP GetSymb(const SymbMap::Locker& symbs, const StrView& symbid) {
      auto ifind = symbs->find(symbid);
      return ifind == symbs->end() ? SymbSP{nullptr} : SymbSP{&GetSymbValue(*ifind)};
   }
   SymbSP GetSymb(const StrView& symbid) {
      return this->GetSymb(this->SymbMap_.Lock(), symbid);
   }
};

} } // namespaces
#endif//__fon9_fmkt_SymbTree_hpp__
