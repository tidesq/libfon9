// \file fon9/seed/TabTreeOp.cpp
// \author fonwinz@gmail.com
#include "fon9/seed/TabTreeOp.hpp"
#include "fon9/seed/FieldChars.hpp"
#include "fon9/seed/CloneTree.hpp"
#include "fon9/StrTo.hpp"

namespace fon9 { namespace seed {

static std::vector<TabSP> MakeTabTreeOpLayoutTabs(const Layout& ref) {
   std::vector<TabSP> res{ref.GetTabCount()};
   size_t idx = 0;
   for (TabSP& tab : res)
      tab.reset(new Tab{*ref.GetTab(idx++), Fields{}, TabFlag::HasSapling});
   return res;
}
TabTreeOp::TabTreeOp(const Layout& ref)
   : base{new LayoutN(FieldSP{new FieldChar1(Named{"type"})}, MakeTabTreeOpLayoutTabs(ref))} {
}
TabTreeOp::TabTreeOp(const Tab& ref)
   : base{new Layout1{FieldSP{new FieldChar1(Named{"type"})},
                      TabSP{new Tab{ref, Fields{}, TabFlag::HasSapling}}}} {
}
//--------------------------------------------------------------------------//
fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4265 /* class has virtual functions, but destructor is not virtual. */
                              4355 /* 'this' : used in base member initializer list*/);
struct TabTreeOp::PodOp : public PodOpDefault {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = PodOpDefault;
public:
   seed::TreeOp& OrigTreeOp_;
   PodOp(TabTreeOp& sender, const StrView& strKeyText, seed::TreeOp& origTreeOp)
      : base(sender, OpResult::no_error, strKeyText)
      , OrigTreeOp_(origTreeOp) {
   }
   TreeSP GetSapling(Tab& tab) override {
      this->Tab_ = &tab;
      return static_cast<TabTreeOp*>(this->Sender_)->GetSapling(this->OrigTreeOp_, *this);
   }
   void OnSeedCommand(Tab* tab, StrView cmdln, FnCommandResultHandler resHandler) override {
      this->Tab_ = tab;
      static_cast<TabTreeOp*>(this->Sender_)->OnTabEditCommand(this->OrigTreeOp_, *this, cmdln, std::move(resHandler));
   }
};
struct TabTreeOp::TreeOp : public seed::TreeOp {
   fon9_NON_COPY_NON_MOVE(TreeOp);
   using base = seed::TreeOp;
   seed::TreeOp& OrigTreeOp_;
   TreeOp(TabTreeOp& tree, seed::TreeOp& origTreeOp) : base(tree), OrigTreeOp_(origTreeOp) {
   }
   void Get(StrView strKeyText, FnPodOp fnCallback) override {
      PodOp op{*static_cast<TabTreeOp*>(&this->Tree_), strKeyText, this->OrigTreeOp_};
      fnCallback(op, &op);
   }
};
void TabTreeOp::HandleTabTreeOp(seed::TreeOp& origTreeOp, FnTreeOp&& fnCallback) {
   TreeOp op{*this, origTreeOp};
   fnCallback(TreeOpResult{this, OpResult::no_error}, &op);
}
//--------------------------------------------------------------------------//
struct TabTreeOp::EditTree : public CloneTree {
   fon9_NON_COPY_NON_MOVE(EditTree);
   using base = CloneTree;
   using base::base;
   GridView MakeApplyCheckGridView(Tab& tab, std::string gvOrig) {
      assert(&tab == this->LayoutSP_->GetTab(0));
      Container::Locker    container{this->Container_};
      CloneTree::GridView  mygv{container, GridViewRequestFull{tab}};
      if (this->GvEditingSize_ == mygv.GridView_.size()
      && this->GvApply_.size() - this->GvOrigPos_ == gvOrig.size()) {
         const char* pcurr = this->GvApply_.c_str();
         if (memcmp(pcurr, mygv.GridView_.c_str(), this->GvEditingSize_) == 0
          && memcmp(pcurr + this->GvOrigPos_, gvOrig.c_str(), gvOrig.size()) == 0) {
            mygv.GridView_ = this->GvApply_;
            return mygv;
         }
      }
      this->GvEditingSize_ = static_cast<uint32_t>(mygv.GridView_.size());
      if (mygv.GridView_ == gvOrig)
         this->GvOrigPos_ = this->GvEditingSize_;
      else {
         mygv.GridView_.reserve(mygv.GridView_.size() + gvOrig.size() + sizeof(NumOutBuf));
         NumOutBuf nbuf;
         char*     pnum = nbuf.end();
         *--pnum = GridViewResult::kRowSplitter;
         pnum = ToStrRev(pnum, ++this->SubmitId_);
         #define kCSTR_SubmitId_RowHead   "\n\f" "SubmitId="
         static_assert(kCSTR_SubmitId_RowHead[0] == GridViewResult::kRowSplitter
                       && kCSTR_SubmitId_RowHead[1] == GridViewResult::kApplySplitter,
                       "kCSTR_SubmitId_RowHead define error.");
         mygv.GridView_.append(kCSTR_SubmitId_RowHead);
         mygv.GridView_.append(pnum, nbuf.end());
         this->GvOrigPos_ = static_cast<uint32_t>(mygv.GridView_.size());
         mygv.GridView_.append(gvOrig);
      }
      this->GvApply_ = mygv.GridView_;
      return mygv;
   }
   void ApplySubmit(seed::TreeOp& origTreeOp, Tab& tab, StrView submitId, FnCommandResultHandler&& resHandler) {
      assert(&tab == this->LayoutSP_->GetTab(0));
      GridApplySubmitRequest reqSubmit;
      if ((reqSubmit.Tab_ = origTreeOp.Tree_.LayoutSP_->GetTab(&tab.Name_)) == nullptr) {
         SeedOpResult res{*this, OpResult::not_supported_grid_apply, kTabTree_KeyApply, &tab};
         resHandler(res, StrView{});
         return;
      }
      auto    sid = StrTo(submitId, uint64_t{0});
      StrView errmsg{"Bad SubmitId"};
      if (sid == this->SubmitId_) {
         Container::Locker container{this->Container_};
         if (sid == this->SubmitId_) {
            ++this->SubmitId_;
            CloneTree::GridView  mygv{container, GridViewRequestFull{tab}};
            const char*          cstrApply = this->GvApply_.c_str();
            if (this->GvEditingSize_ == mygv.GridView_.size()
            && memcmp(cstrApply, mygv.GridView_.c_str(), this->GvEditingSize_) == 0) {
               reqSubmit.BeforeGrid_.Reset(cstrApply + this->GvOrigPos_, cstrApply + this->GvApply_.size());
               reqSubmit.EditingGrid_ = ToStrView(mygv.GridView_);
               origTreeOp.GridApplySubmit(reqSubmit, std::move(resHandler));
               return;
            }
            errmsg = "Editing changed";
         }
      }
      SeedOpResult res{*this, OpResult::bad_apply_submit, kTabTree_KeyApply, &tab};
      resHandler(res, errmsg);
   }
private:
   uint64_t    SubmitId_{0};
   uint32_t    GvEditingSize_{0};
   uint32_t    GvOrigPos_{0};
   std::string GvApply_;
};
TabTreeOp::EditTreeSP TabTreeOp::EditSaplingManager(TabTreeOp& owner, Tab* tab, seed::TreeOp* origTreeOp) {
   uintptr_t key = reinterpret_cast<uintptr_t>(&owner);
   if (tab) {
      assert(tab == owner.LayoutSP_->GetTab(static_cast<size_t>(tab->GetIndex())));
      key += tab->GetIndex();
   }
   using SaplingsImpl = SortedVector<uintptr_t, EditTreeSP>;
   using Saplings = MustLock<SaplingsImpl>;
   static Saplings Saplings_;
   Saplings::Locker saplings{Saplings_};
   auto ifind = saplings->find(key);
   if (ifind != saplings->end()) {
      if (origTreeOp == nullptr) {
         for (size_t L = owner.LayoutSP_->GetTabCount(); L > 0; --L) {
            ifind = saplings->erase(ifind);
            if (ifind == saplings->end() || ifind->first != ++key)
               break;
         }
         return EditTreeSP{};
      }
      return ifind->second;
   }
   if (origTreeOp == nullptr || tab == nullptr)
      return EditTreeSP{};
   if ((tab = origTreeOp->Tree_.LayoutSP_->GetTab(&tab->Name_)) == nullptr)
      return EditTreeSP{};
   EditTreeSP tree{new EditTree{*origTreeOp->Tree_.LayoutSP_->KeyField_, *tab,
                                TabFlag::Writable | TabFlag::NoSapling | TabFlag::NoSeedCommand,
                                TreeFlag::AddableRemovable}};
   saplings->kfetch(key).second = tree;
   saplings.unlock();
   tree->CopyTable(*origTreeOp);
   return tree;
}
//--------------------------------------------------------------------------//
struct TabTreeOp::ApplyTree : public Tree {
   fon9_NON_COPY_NON_MOVE(ApplyTree);
   using base = Tree;
   const TreeSP      Orig_;
   const EditTreeSP  Editing_;
public:
   /// 直接使用 editing 的 layout, 段 ApplyTree 本身並沒有 container.
   ApplyTree(TreeSP orig, EditTreeSP editing)
      : base{editing->LayoutSP_}
      , Orig_{std::move(orig)}
      , Editing_{std::move(editing)} {
   }
   struct TreeOp : public seed::TreeOp {
      fon9_NON_COPY_NON_MOVE(TreeOp);
      using base = seed::TreeOp;
      seed::TreeOp& OrigTreeOp_;
      TreeOp(ApplyTree& tree, seed::TreeOp& origTreeOp) : base(tree), OrigTreeOp_(origTreeOp) {
      }
      void GridView(const GridViewRequest& myreq, FnGridViewOp fnCallback) override {
         if (Tab* mytab = myreq.Tab_) {
            if (Tab* otab = this->OrigTreeOp_.Tree_.LayoutSP_->GetTab(&mytab->Name_)) {
               EditTreeSP editing = static_cast<ApplyTree*>(&this->Tree_)->Editing_;
               this->OrigTreeOp_.GridView(GridViewRequestFull{*otab},
                                          [mytab, editing, fnCallback](GridViewResult& res) {
                  if (res.OpResult_ != OpResult::no_error)
                     fnCallback(res);
                  else {
                     res = editing->MakeApplyCheckGridView(*mytab, res.GridView_);
                     fnCallback(res);
                  }
               });
               return;
            }
         }
         return base::GridView(myreq, std::move(fnCallback));
      }
   };
   void OnTreeOp(FnTreeOp fnCallback) override {
      intrusive_ptr<ApplyTree> pthis{this};
      this->Orig_->OnTreeOp([pthis, fnCallback](const TreeOpResult& resOrig, seed::TreeOp* opOrig) {
         if(opOrig == nullptr)
            fnCallback(TreeOpResult{pthis.get(), resOrig.OpResult_}, nullptr);
         else {
            TreeOp myop{*pthis, *opOrig};
            fnCallback(TreeOpResult{pthis.get(), OpResult::no_error}, &myop);
         }
      });
   }
};
//--------------------------------------------------------------------------//
TabTreeOp::~TabTreeOp() {
   EditSaplingManager(*this, nullptr, nullptr); // 移除 Sapling.
}
TreeSP TabTreeOp::GetSapling(seed::TreeOp& origTreeOp, SeedOpResult& req) {
   if (req.KeyText_ == kTabTree_KeyEdit)
      return EditSaplingManager(*this, req.Tab_, &origTreeOp);
   if (req.KeyText_ == kTabTree_KeyApply)
      return TreeSP{new ApplyTree(&origTreeOp.Tree_, EditSaplingManager(*this, req.Tab_, &origTreeOp))};
   return TreeSP{};
}
void TabTreeOp::OnTabEditCommand(seed::TreeOp& origTreeOp, SeedOpResult& rr, StrView cmdln, FnCommandResultHandler&& resHandler) {
   rr.OpResult_ = OpResult::not_supported_cmd;
   StrTrim(&cmdln);
   if (rr.KeyText_ == kTabTree_KeyEdit) {
      if (cmdln == "restore") {
         if (EditTreeSP tree = EditSaplingManager(*this, rr.Tab_, &origTreeOp)) {
            rr.OpResult_ = OpResult::no_error;
            static_cast<CloneTree*>(tree.get())->CopyTable(origTreeOp);
         }
      }
   }
   else if (rr.KeyText_ == kTabTree_KeyApply) {
      StrView cmd = StrFetchTrim(cmdln, &isspace);
      if (cmd == "submit") {
         if (EditTreeSP tree = EditSaplingManager(*this, rr.Tab_, &origTreeOp)) {
            if (Tab* etab = tree->LayoutSP_->GetTab(&rr.Tab_->Name_)) {
               tree->ApplySubmit(origTreeOp, *etab, cmdln, std::move(resHandler));
               return;
            }
         }
      }
   }
   resHandler(rr, StrView{});
}

fon9_WARN_POP;
} } // namespaces
