// \file fon9/seed/CloneTree.cpp
// \author fonwinz@gmail.com
#include "fon9/seed/CloneTree.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/TreeLockContainerT.hpp"

namespace fon9 { namespace seed {

inline CloneTree::ContainerImpl::iterator ContainerLowerBound(CloneTree::ContainerImpl& container, StrView strKeyText) {
   container.key_comp().KeyField_->StrToCell(SimpleRawWr{*container.KeySP_}, strKeyText);
   return container.lower_bound(container.KeySP_);
}
inline CloneTree::ContainerImpl::iterator ContainerFind(CloneTree::ContainerImpl& container, StrView strKeyText) {
   container.key_comp().KeyField_->StrToCell(SimpleRawWr{*container.KeySP_}, strKeyText);
   return container.find(container.KeySP_);
}

static LayoutSP CloneLayout(const Field& keyField, Tab& srcTab, TabFlag newTabFlag, TreeFlag newTreeFlag) {
   auto    cfgs = MakeFieldsConfig(srcTab.Fields_, *fon9_kCSTR_CELLSPL, *fon9_kCSTR_ROWSPL);
   Fields  flds;
   StrView cfgv{&cfgs};
   MakeFields(cfgv, *fon9_kCSTR_CELLSPL, *fon9_kCSTR_ROWSPL, flds);

   cfgs.clear();
   AppendFieldConfig(cfgs, keyField, *fon9_kCSTR_CELLSPL, -1);
   cfgv = &cfgs;
   FieldSP  keyfld = MakeField(cfgv, *fon9_kCSTR_CELLSPL, *fon9_kCSTR_ROWSPL);
   TabSP    ktab{new Tab{srcTab, std::move(flds), newTabFlag, keyfld.get()}};
   return LayoutSP{new Layout1(std::move(keyfld), std::move(ktab), newTreeFlag)};
}

CloneTree::CloneTree(const Field& keyField, Tab& srcTab, TabFlag newTabFlag, TreeFlag newTreeFlag)
   : base{CloneLayout(keyField, srcTab, newTabFlag, newTreeFlag)}
   , Container_{CmpElementSP{LayoutSP_->KeyField_.get()}} {
}
void CloneTree::OnParentSeedClear() {
   ContainerImpl::base  temp;
   {
      Container::Locker container{this->Container_};
      container->swap(temp);
   }
   // for (auto& seed : temp)
   //    seed->OnParentTreeClear(*this);
}
void CloneTree::CopyTable(seed::TreeOp& srcTreeOp) {
   GridViewRequest req{TextBegin()};
   req.Tab_ = srcTreeOp.Tree_.LayoutSP_->GetTab(&this->LayoutSP_->GetTab(0)->Name_);
   req.MaxBufferSize_ = 0;
   srcTreeOp.GridView(req, [this](GridViewResult& res) {
      Container::Locker container{this->Container_};
      Tab*              tab = this->LayoutSP_->GetTab(0);
      StrView           gv{&res.GridView_};
      container->clear();
      if (!container->KeySP_)
         container->KeySP_.reset(MakeDyMemRaw<Element>(*tab));
      while (!gv.empty()) {
         StrView     ln = StrFetchNoTrim(gv, static_cast<char>(res.kRowSplitter));
         StrView     fstr = StrFetchNoTrim(ln, static_cast<char>(res.kCellSplitter));
         ElementSP   ele{MakeDyMemRaw<Element>(*tab)};
         SimpleRawWr wr{*ele};
         size_t      fidx = 0;
         this->LayoutSP_->KeyField_->StrToCell(wr, fstr);
         while (!ln.empty()) {
            fstr = StrFetchNoTrim(ln, static_cast<char>(res.kCellSplitter));
            if (const Field* fld = tab->Fields_.Get(fidx++))
               fld->StrToCell(wr, fstr);
         }
         container->insert(std::move(ele));
      }
   });
}

CloneTree::GridView::GridView(const Container::Locker& container, const GridViewRequest& req)
   : GridViewResult{ContainerOf(Container::StaticCast(*container), &CloneTree::Container_), req.Tab_} {
   seed::MakeGridView(*container, GetIteratorForGv(*container, req.OrigKey_),
                      req, *this, [this](ContainerImpl::iterator ivalue, Tab* tab, RevBuffer& rbuf) {
      SimpleRawRd rd{**ivalue};
      if (tab)
         FieldsCellRevPrint(tab->Fields_, rd, rbuf, GridViewResult::kCellSplitter);
      this->Sender_->LayoutSP_->KeyField_->CellRevPrint(rd, nullptr, rbuf);
   });
}

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4265 /* class has virtual functions, but destructor is not virtual. */
                              4355 /* 'this' : used in base member initializer list*/);
struct CloneTree::PodOp : public PodOpDefault {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = PodOpDefault;
   Element* Seed_;
   PodOp(Element& seed, Tree& sender, OpResult res, const StrView& keyText)
      : base{sender, res, keyText}
      , Seed_(&seed) {
   }
   void BeginRead(Tab& tab, FnReadOp fnCallback) override {
      this->BeginRW(tab, std::move(fnCallback), SimpleRawRd{this->Seed_});
   }
   void BeginWrite(Tab& tab, FnWriteOp fnCallback) override {
      this->BeginRW(tab, std::move(fnCallback), SimpleRawWr{this->Seed_});
   }
};
struct CloneTree::TreeOp : public seed::TreeOp {
   fon9_NON_COPY_NON_MOVE(TreeOp);
   using base = seed::TreeOp;
   TreeOp(CloneTree& tree) : base(tree) {
   }
   void GridView(const GridViewRequest& req, FnGridViewOp fnCallback) override {
      CloneTree::GridView res{*static_cast<CloneTree*>(&this->Tree_), req};
      fnCallback(res);
   }
   void Get(StrView strKeyText, FnPodOp fnCallback) override {
      {
         Container::Locker container{static_cast<CloneTree*>(&this->Tree_)->Container_};
         auto              ifind = GetIteratorForPod(*container, strKeyText);
         if (ifind != container->end()) {
            PodOp op{**ifind, this->Tree_, OpResult::no_error, strKeyText};
            fnCallback(op, &op);
            return;
         }
      } // unlock.
      fnCallback(PodOpResult{this->Tree_, OpResult::not_found_key, strKeyText}, nullptr);
   }
   void Add(StrView strKeyText, FnPodOp fnCallback) override {
      if (IsTextBeginOrEnd(strKeyText)) {
         fnCallback(PodOpResult{this->Tree_, OpResult::not_found_key, strKeyText}, nullptr);
         return;
      }
      Container::Locker container{static_cast<CloneTree*>(&this->Tree_)->Container_};
      auto              ifind = ContainerFind(*container, strKeyText);
      if (ifind == container->end()) {
         ElementSP rec{MakeDyMemRaw<Element>(*static_cast<CloneTree*>(&this->Tree_)->LayoutSP_->GetTab(0))};
         rec.swap(container->KeySP_);
         ifind = container->insert(std::move(rec)).first;
      }
      PodOp op{**ifind, this->Tree_, OpResult::no_error, strKeyText};
      fnCallback(op, &op);
   }
   void Remove(StrView strKeyText, Tab* tab, FnPodRemoved fnCallback) override {
      PodRemoveResult res{this->Tree_, OpResult::not_found_key, strKeyText, tab};
      {
         Container::Locker container{static_cast<CloneTree*>(&this->Tree_)->Container_};
         auto              ifind = ContainerFind(*container, strKeyText);
         if (ifind != container->end()) {
            container->erase(ifind);
            res.OpResult_ = OpResult::removed_pod;
         }
      }
      fnCallback(res);
   }
};
void CloneTree::OnTreeOp(FnTreeOp fnCallback) {
   TreeOp op{*this};
   fnCallback(TreeOpResult{this, OpResult::no_error}, &op);
}
fon9_WARN_POP;

} } // namespaces
