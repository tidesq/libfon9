/// \file fon9/seed/CloneTree.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_CloneTree_hpp__
#define __fon9_seed_CloneTree_hpp__
#include "fon9/seed/Tree.hpp"
#include "fon9/SortedVector.hpp"

namespace fon9 { namespace seed {

/// \ingroup seed
/// 從指定的 tree 其中一個 tab, 複製 layout 及 container 內容.
/// - 所有的欄位使用 DyMem, 透過 MakeDyMemRaw() 建立一筆與來源 tree 相同的資料.
/// - Container 使用 SortedVector;
class fon9_API CloneTree : public Tree {
   fon9_NON_COPY_NON_MOVE(CloneTree);
   using base = Tree;
   struct PodOp;
   struct TreeOp;
protected:
   struct Element : public Raw {
      fon9_NON_COPY_NON_MOVE(Element);
      Element() = default;
   };
   using ElementSP = std::unique_ptr<Element>;
   struct CmpElementSP {
      const Field* KeyField_;
      bool operator()(const ElementSP& lhs, const ElementSP& rhs) const {
         return this->KeyField_->Compare(SimpleRawRd{*lhs}, SimpleRawRd{*rhs}) < 0;
      }
   };
   struct ContainerImpl : public SortedVectorSet<ElementSP, CmpElementSP> {
      fon9_NON_COPY_NON_MOVE(ContainerImpl);
      using base = SortedVectorSet<ElementSP, CmpElementSP>;
      using base::base;
      ElementSP KeySP_;
   };
   using Container = MustLock<ContainerImpl>;
   Container   Container_;
   friend ContainerImpl::iterator ContainerLowerBound(ContainerImpl& container, StrView strKeyText);
   friend ContainerImpl::iterator ContainerFind(ContainerImpl& container, StrView strKeyText);

public:
   CloneTree(const Field& keyField, Tab& srcTab, TabFlag newTabFlag, TreeFlag newTreeFlag);
   void CopyTable(seed::TreeOp& srcTreeOp);

   virtual void OnTreeOp(FnTreeOp fnCallback) override;
   virtual void OnParentSeedClear() override;

   struct GridView : public GridViewResult {
      GridView(CloneTree& tree, const GridViewRequest& req)
         : GridView{tree.Container_.Lock(), req} {
      }
      GridView(const Container::Locker& container, const GridViewRequest& req);
   };
};

} } // namespaces
#endif//__fon9_seed_CloneTree_hpp__
