/// \file fon9/seed/Tree.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_Tree_hpp__
#define __fon9_seed_Tree_hpp__
#include "fon9/seed/TreeOp.hpp"

namespace fon9 { namespace seed {

/// \ingroup seed
/// 透過 Tree::OnTreeOp(); 取得.
/// 然後才能操作 container.
class fon9_API TreeOp;

fon9_WARN_DISABLE_PADDING;
struct TreeOpResult {
   /// 此值必定有效, 不會是 nullptr.
   Tree*    Sender_;
   OpResult OpResult_;
};
fon9_WARN_POP;

using FnTreeOp = std::function<void(const TreeOpResult& res, TreeOp* op)>;

fon9_WARN_DISABLE_PADDING;
/// \ingroup seed
/// - 這裡提供的是 **管理用** 介面.
/// - 實際應用時, 您應該會先找到需要的 Tree or Seed, 然後直接操作 (例: MaAuth, DllMgr)
class fon9_API Tree : public intrusive_ref_counter<Tree> {
   fon9_NON_COPY_NON_MOVE(Tree);
protected:
   /// 在最後一個 TreeSP 死亡時會呼叫此處, 所以此時已沒有任何人擁有 this!
   /// 請參考 DllTree::OnTreeReleased() 結合 TaskThr_ 的應用實例.
   /// 預設: if (this->use_count() == 0) delete this;
   virtual void OnTreeReleased();

   friend inline void intrusive_ptr_deleter(const Tree* tree) {
      const_cast<Tree*>(tree)->OnTreeReleased();
   }

public:
   /// 建構時會確保 LayoutSP_ 有效, 必定不會為 nullptr.
   const LayoutSP LayoutSP_;

   /// layout 不可是 nullptr.
   /// 允許多個 Tree 共用同一個 Layout.
   Tree(LayoutSP layout) : LayoutSP_{std::move(layout)} {
   }

   virtual ~Tree();

   /// 預設: fnCallback(TreeOpResult{this, OpResult::not_supported_tree_op}, nullptr);
   virtual void OnTreeOp(FnTreeOp fnCallback);

   /// 用在 NeedsApply 建立另一個編輯用的 tree.
   /// 當 SeedSearcher.cpp: ContinueSeedSearch() 收到 "^edit:Config", "^apply:Config" 之類的要求,
   /// 就會呼叫此處, 可參考或直接使用 TabTreeOp.cpp; 
   /// 預設: fnCallback(TreeOpResult{this, OpResult::not_supported_tree_op}, nullptr);
   virtual void OnTabTreeOp(FnTreeOp fnCallback);

   /// 清除全部的 seeds. 通常用在 parent 死亡時.
   /// 為了避免 seed 擁有 Parent tree SP, 因循環參考造成 memory leak.
   /// 預設 do nothing.
   /// \code
   ///   for (auto& pod : this->Container_) {
   ///      for (auto& seed : pod)
   ///         if (auto sapling = seed.GetSapling())
   ///            sapling->OnParentSeedClear();
   ///   }
   /// \endcode
   virtual void OnParentSeedClear();
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_seed_Tree_hpp__
