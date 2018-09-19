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

   /// 預設使用底下 Layout1 建構:
   /// - KeyField = seed::FieldCharVector(keyName)
   /// - Tab = {tabName, fields}
   /// - 若上述 Layout 不符, 您也可以直接使用 seed::Tree 的建構參數.
   PolicyTree(std::string tabName, std::string keyName, seed::Fields&& fields,
              seed::TabFlag tabFlags = seed::TabFlag::Writable | seed::TabFlag::NoSapling);

   virtual void OnTreeOp(seed::FnTreeOp fnCallback) override;
   virtual void OnParentSeedClear() override;
};
using PolicyTreeSP = seed::TreeSPT<PolicyTree>;

} } // namespaces
#endif//__fon9_auth_PolicyTree_hpp__
