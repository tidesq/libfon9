// \file fon9/fmkt/SymbTree.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbTree.hpp"
#include "fon9/seed/PodOp.hpp"
#include "fon9/seed/RawWr.hpp"

namespace fon9 { namespace fmkt {

fon9_MSC_WARN_DISABLE(4265 /* class has virtual functions, but destructor is not virtual. */);
class SymbTree::PodOp : public seed::PodOpDefault {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = seed::PodOpDefault;
public:
   const SymbSP   Symb_;
   PodOp(Tree& sender, const StrView& keyText, SymbSP&& symb)
      : base(sender, seed::OpResult::no_error, keyText)
      , Symb_{std::move(symb)} {
   }
   void BeginRead(seed::Tab& tab, seed::FnReadOp fnCallback) override {
      if (auto dat = this->Symb_->GetSymbData(tab.GetIndex()))
         this->BeginRW(tab, std::move(fnCallback), seed::SimpleRawRd{*dat});
      else {
         this->OpResult_ = seed::OpResult::not_found_seed;
         fnCallback(*this, nullptr);
      }
   }
   void BeginWrite(seed::Tab& tab, seed::FnWriteOp fnCallback) override {
      if (auto dat = this->Symb_->FetchSymbData(tab.GetIndex()))
         this->BeginRW(tab, std::move(fnCallback), seed::SimpleRawWr{*dat});
      else {
         this->OpResult_ = seed::OpResult::not_found_seed;
         fnCallback(*this, nullptr);
      }
   }
};

class SymbTree::TreeOp : public seed::TreeOp {
   fon9_NON_COPY_NON_MOVE(TreeOp);
   using base = seed::TreeOp;
public:
   TreeOp(SymbTree& tree) : base(tree) {
   }
#if 0 // 若使用 unordered_map 則不支援 GridView;
   void GridView(const seed::GridViewRequest& req, seed::FnGridViewOp fnCallback) override {
      seed::GridViewResult res{this->Tree_, req.Tab_};
      {
         SymbMap::Locker lockedMap{static_cast<SymbTree*>(&this->Tree_)->SymbMap_};
         seed::MakeGridView(*lockedMap, seed::GetIteratorForGv(*lockedMap, req.OrigKey_),
                            req, res, &MakeRowView);
      } // unlock.
      fnCallback(res);
   }
   static void MakeRowView(iterator ivalue, seed::Tab* tab, RevBuffer& rbuf) {
      if (tab)
         FieldsCellRevPrint(tab->Fields_, seed::SimpleRawRd{GetSymbValue(*ivalue)}, rbuf, seed::GridViewResult::kCellSplitter);
      RevPrint(rbuf, GetSymbId(*ivalue));
   }
#endif

   void OnPodOp(const StrView& strKeyText, SymbSP&& symb, seed::FnPodOp&& fnCallback) {
      if (symb) {
         PodOp op(this->Tree_, strKeyText, std::move(symb));
         fnCallback(op, &op);
      }
      else
         fnCallback(seed::PodOpResult{this->Tree_, seed::OpResult::not_found_key, strKeyText}, nullptr);
   }
   void Get(StrView strKeyText, seed::FnPodOp fnCallback) override {
      SymbSP symb;
      {
         SymbMap::Locker lockedMap{static_cast<SymbTree*>(&this->Tree_)->SymbMap_};
         auto            ifind = lockedMap->find(strKeyText);
         if (ifind != lockedMap->end())
            symb.reset(&GetSymbValue(*ifind));
      } // unlock.
      this->OnPodOp(strKeyText, std::move(symb), std::move(fnCallback));
   }
   void Add(StrView strKeyText, seed::FnPodOp fnCallback) override {
      if (seed::IsTextBeginOrEnd(strKeyText)) {
         fnCallback(seed::PodOpResult{this->Tree_, seed::OpResult::not_found_key, strKeyText}, nullptr);
         return;
      }
      SymbSP symb;
      {
         SymbMap::Locker lockedMap{static_cast<SymbTree*>(&this->Tree_)->SymbMap_};
         auto            ifind = lockedMap->find(strKeyText);
         if (ifind == lockedMap->end()) {
            symb = static_cast<SymbTree*>(&this->Tree_)->MakeSymb(strKeyText);
            ifind = lockedMap->emplace(ToStrView(symb->SymbId_), symb).first;
         }
         else
            symb.reset(&GetSymbValue(*ifind));
      } // unlock.
      this->OnPodOp(strKeyText, std::move(symb), std::move(fnCallback));
   }
   void Remove(StrView strKeyText, seed::Tab* tab, seed::FnPodRemoved fnCallback) override {
      seed::PodRemoveResult res{this->Tree_, seed::OpResult::not_found_key, strKeyText, tab};
      {
         SymbMap::Locker lockedMap{static_cast<SymbTree*>(&this->Tree_)->SymbMap_};
         auto            ifind = lockedMap->find(strKeyText);
         if (ifind != lockedMap->end()) {
            lockedMap->erase(ifind);
            res.OpResult_ = seed::OpResult::removed_pod;
         }
      }
      fnCallback(res);
   }
};
fon9_MSC_WARN_POP;

void SymbTree::OnTreeOp(seed::FnTreeOp fnCallback) {
   TreeOp op{*this};
   fnCallback(seed::TreeOpResult{this, seed::OpResult::no_error}, &op);
}
void SymbTree::OnParentSeedClear() {
   SymbMapImpl symbs{std::move(*this->SymbMap_.Lock())};
   // unlock 後, symbs 解構時, 自動清除.
}

} } // namespaces
