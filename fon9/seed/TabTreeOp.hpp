/// \file fon9/seed/TabTreeOp.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_TabTreeOp_hpp__
#define __fon9_seed_TabTreeOp_hpp__
#include "fon9/seed/Tree.hpp"
#include "fon9/seed/PodOp.hpp"

namespace fon9 { namespace seed {

constexpr StrView kTabTree_KeyEdit{"edit"};
constexpr StrView kTabTree_KeyApply{"apply"};

/// \ingroup seed
/// 協助 Tree::OnTabTreeOp()
/// 當收到 "^edit:Config" 之類的要求, ref: SeedSearcher.cpp: ContinueSeedSearch();
/// 會將要求拆解為 key="edit", tab="Config", 然後透過 Tree::OnTabTreeOp() 處理,
/// YourTree::OnTabTreeOp() 通常會建立一個 TabTreeOp 處理這種要求.
class fon9_API TabTreeOp : public Tree {
   fon9_NON_COPY_NON_MOVE(TabTreeOp);
   using base = Tree;
   struct PodOp;
   struct TreeOp;
   struct ApplyTree;
   struct EditTree;
   using EditTreeSP = intrusive_ptr<EditTree>;
   static EditTreeSP EditSaplingManager(TabTreeOp& owner, Tab* tab, seed::TreeOp* origTreeOp);
protected:
   /// 根據 req.KeyText_ 返回不同的 tree:
   /// - "edit":  會透過 OrigTree 對應的 tab 建立 CloneTree 作為編輯用.
   /// - "apply": 建立一個新的 tree, 此 tree 僅提供: 比對差異結果的 GridView.
   virtual TreeSP GetSapling(seed::TreeOp& origTreeOp, SeedOpResult& req);
   /// 根據 rr.KeyText_ 處理不同的指令:
   /// - "edit",  cmdln=="restore": 重新從 OrigTree 複製完整的資料.
   /// - "apply", cmdln=="submit":  將編輯用的 tree 套用至 OrigTree.
   virtual void OnSeedCommand(seed::TreeOp& origTreeOp, SeedOpResult& rr, StrView cmdln, FnCommandResultHandler&& resHandler);
public:
   TabTreeOp(const Tab& ref);
   TabTreeOp(const Layout& ref);
   ~TabTreeOp();
   void HandleTabTreeOp(seed::TreeOp& origTreeOp, FnTreeOp&& fnCallback);
};
using TabTreeOpSP = intrusive_ptr<TabTreeOp>;

} } // namespaces
#endif//__fon9_seed_TabTreeOp_hpp__
