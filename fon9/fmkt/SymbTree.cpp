// \file fon9/fmkt/SymbTree.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbTree.hpp"
#include "fon9/seed/PodOp.hpp"
#include "fon9/seed/RawWr.hpp"
#include "fon9/RevPrint.hpp"

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
   void GridView(const seed::GridViewRequest& req, seed::FnGridViewOp fnCallback) override {
      seed::GridViewResult res{this->Tree_, req.Tab_};
      {
         SymbMap::Locker lockedMap{static_cast<SymbTree*>(&this->Tree_)->SymbMap_};
      #if 0 // 不是 unordered_map 則可以使用一般的 GridView.
         seed::MakeGridView(*lockedMap, seed::GetIteratorForGv(*lockedMap, req.OrigKey_),
                               req, res, &MakeRowView);
      #else // 若使用 unordered_map 則 req.OrigKey_ = "key list";
         if (req.OrigKey_.Get1st() != seed::GridViewResult::kCellSplitter)
            res.OpResult_ = seed::OpResult::not_supported_grid_view;
         else {
            res.ContainerSize_ = lockedMap->size();
            StrView        keys{req.OrigKey_.begin() + 1, req.OrigKey_.end()};
            RevBufferList  rbuf{256};
            for (;;) {
               const char* pkey = static_cast<const char*>(memrchr(keys.begin(), seed::GridViewResult::kCellSplitter, keys.size()));
               StrView     key{pkey ? (pkey + 1) : keys.begin(), keys.end()};
               auto        ifind = seed::GetIteratorForPod(*lockedMap, key);
               if (ifind == lockedMap->end())
                  RevPrint(rbuf, key);
               else
                  MakeRowView(ifind,  res.Tab_, rbuf);
               if (pkey == nullptr)
                  break;
               keys.SetEnd(pkey);
               RevPrint(rbuf, seed::GridViewResult::kRowSplitter);
            }
            res.GridView_ = BufferTo<std::string>(rbuf.MoveOut());
         }
      #endif
      } // unlock map.
      fnCallback(res);
   }
   static void MakeRowView(iterator ivalue, seed::Tab* tab, RevBuffer& rbuf) {
      if (tab) {
         if (auto dat = GetSymbValue(*ivalue).GetSymbData(tab->GetIndex()))
            FieldsCellRevPrint(tab->Fields_, seed::SimpleRawRd{*dat}, rbuf, seed::GridViewResult::kCellSplitter);
      }
      RevPrint(rbuf, GetSymbId(*ivalue));
   }

   void OnPodOp(const StrView& strKeyText, SymbSP&& symb, seed::FnPodOp&& fnCallback) {
      if (symb) {
         PodOp op(this->Tree_, strKeyText, std::move(symb));
         fnCallback(op, &op);
      }
      else
         fnCallback(seed::PodOpResult{this->Tree_, seed::OpResult::not_found_key, strKeyText}, nullptr);
   }
   void Get(StrView strKeyText, seed::FnPodOp fnCallback) override {
      SymbSP symb = static_cast<SymbTree*>(&this->Tree_)->GetSymb(strKeyText);
      this->OnPodOp(strKeyText, std::move(symb), std::move(fnCallback));
   }
   void Add(StrView strKeyText, seed::FnPodOp fnCallback) override {
      if (seed::IsTextBeginOrEnd(strKeyText))
         fnCallback(seed::PodOpResult{this->Tree_, seed::OpResult::not_found_key, strKeyText}, nullptr);
      else {
         SymbSP symb = static_cast<SymbTree*>(&this->Tree_)->FetchSymb(strKeyText);
         this->OnPodOp(strKeyText, std::move(symb), std::move(fnCallback));
      }
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
SymbSP SymbTree::FetchSymb(const SymbMap::Locker& symbs, const StrView& symbid) {
   auto ifind = symbs->find(symbid);
   if (ifind != symbs->end())
      return SymbSP{&GetSymbValue(*ifind)};
   auto symb = this->MakeSymb(symbid);
   symbs->insert(SymbMapImpl::value_type(ToStrView(symb->SymbId_), symb));
   return symb;
}

} } // namespaces
