// \file fon9/fmkt/Symb.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/Symb.hpp"
#include "fon9/seed/Plugins.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

SymbData::~SymbData() {
}

//--------------------------------------------------------------------------//

class SymbTree : public seed::Tree {
   fon9_NON_COPY_NON_MOVE(SymbTree);
   using base = seed::Tree;
   using SymbMapImpl = fmkt::SymbMap;
   using iterator = SymbMapImpl::iterator;
   using SymbMap = MustLock<SymbMapImpl>;
   SymbMap  SymbMap_;

   static seed::LayoutSP MakeLayout() {
      // 是否可以在 exe 啟動時:
      // 1. 決定需要那些 tab, 然後就固定下來, 這樣就不用每個 symb 都需要一個 locker.
      // 2. 是否可以在 exe 啟動時建立好 table(map), 這樣 table 也不用 locker.
      seed::Fields flds;
      flds.Add(fon9_MakeField(Named{"Market"}, Symb, Market_));
      return new seed::LayoutDy(fon9_MakeField(Named{"Id"}, Symb, SymbId_),
                                seed::TabSP{new seed::Tab{Named{"SymbBase"}, std::move(flds), seed::TabFlag::NoSapling | seed::TabFlag::NoSeedCommand | seed::TabFlag::Writable}},
                                seed::TreeFlag::AddableRemovable | seed::TreeFlag::Unordered);
   }

   fon9_MSC_WARN_DISABLE(4265 /* class has virtual functions, but destructor is not virtual. */);
   class PodOp : public seed::PodOpDefault {
      fon9_NON_COPY_NON_MOVE(PodOp);
      using base = seed::PodOpDefault;
   public:
      const SymbSP   Symb_;
      PodOp(Tree& sender, const StrView& keyText, SymbSP&& symb)
         : base(sender, seed::OpResult::no_error, keyText)
         , Symb_{std::move(symb)} {
      }
      void BeginRead(seed::Tab& tab, seed::FnReadOp fnCallback) override {
         this->BeginRW(tab, std::move(fnCallback), seed::SimpleRawRd{*this->Symb_});
      }
      void BeginWrite(seed::Tab& tab, seed::FnWriteOp fnCallback) override {
         this->BeginRW(tab, std::move(fnCallback), seed::SimpleRawWr{*this->Symb_});
      }
   };
   class TreeOp : public seed::TreeOp {
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
            FieldsCellRevPrint(tab->Fields_, seed::SimpleRawRd{GetSymbRef(*ivalue)}, rbuf, seed::GridViewResult::kCellSplitter);
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
               symb.reset(&GetSymbRef(*ifind));
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
               symb.reset(new Symb{strKeyText});
               ifind = lockedMap->emplace(ToStrView(symb->SymbId_), symb).first;
            }
            else
               symb.reset(&GetSymbRef(*ifind));
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

public:
   SymbTree() : base{MakeLayout()} {
   }
   void OnTreeOp(seed::FnTreeOp fnCallback) override {
      TreeOp op{*this};
      fnCallback(seed::TreeOpResult{this, seed::OpResult::no_error}, &op);
   }
};

static bool SymbMap_Start(seed::PluginsHolder& holder, StrView args) {
   holder.Root_->Add(new fon9::seed::NamedSapling(new SymbTree, "Symbs"));
   return true;
}

} } // namespaces

extern "C" fon9_API fon9::seed::PluginsDesc f9p_SymbMap;
static fon9::seed::PluginsPark f9pRegister{"Symbs", &f9p_SymbMap};

fon9::seed::PluginsDesc f9p_SymbMap{
   "",
   &fon9::fmkt::SymbMap_Start,
   nullptr,
   nullptr,
};
