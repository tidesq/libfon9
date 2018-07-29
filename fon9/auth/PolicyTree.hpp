/// \file fon9/PolicyTree.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_PolicyTree_hpp__
#define __fon9_auth_PolicyTree_hpp__
#include "fon9/auth/PolicyTable.hpp"
#include "fon9/seed/Tree.hpp"

namespace fon9 { namespace auth {

class fon9_API PolicyTree : public seed::Tree, public PolicyTable {
   fon9_NON_COPY_NON_MOVE(PolicyTree);
   using base = seed::Tree;
   struct PodOp;
   struct TreeOp;

public:
   using base::base;
   PolicyTree(std::string tabName, std::string keyName, seed::Fields&& fields);

   virtual void OnTreeOp(seed::FnTreeOp fnCallback) override;
   virtual void OnParentSeedClear() override;
};
using PolicyTreeSP = seed::TreeSPT<PolicyTree>;

} } // namespaces
#endif//__fon9_auth_PolicyTree_hpp__
