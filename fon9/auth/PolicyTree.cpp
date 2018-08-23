// \file fon9/PolicyTree.cpp
// \author fonwinz@gmail.com
#include "fon9/auth/PolicyTree.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/TreeLockContainerT.hpp"

namespace fon9 { namespace auth {

PolicyTree::PolicyTree(std::string tabName, std::string keyName, seed::Fields&& fields)
   : base{new seed::Layout1(fon9_MakeField(Named{std::move(keyName)}, PolicyItem, PolicyId_),
                            new seed::Tab{Named{std::move(tabName)}, std::move(fields)})} {
}

void PolicyTree::OnParentSeedClear() {
   PolicyItemMap  temp;
   {
      PolicyMaps::Locker maps{this->PolicyMaps_};
      maps->DeletedMap_.clear();
      maps->ItemMap_.swap(temp);
   }
   for (auto& seed : temp)
      seed->OnParentTreeClear(*this);
}

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4265 /* class has virtual functions, but destructor is not virtual. */
                              4355 /* 'this' : used in base member initializer list*/);
struct PolicyTree::PodOp : public seed::PodOpLocker<PodOp, PolicyMaps::Locker> {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = seed::PodOpLocker<PodOp, PolicyMaps::Locker>;
   PolicyItemSP Seed_;
   bool         IsModified_{false};
   PodOp(PolicyItem& seed, Tree& sender, seed::OpResult res, PolicyMaps::Locker& locker, bool isForceWrite)
      : base{*this, sender, res, ToStrView(seed.PolicyId_), locker}
      , Seed_(&seed)
      , IsModified_{isForceWrite} {
   }
   PolicyItem& GetSeedRW(seed::Tab&) {
      return *this->Seed_;
   }
   void BeginWrite(seed::Tab& tab, seed::FnWriteOp fnCallback) override {
      base::BeginWrite(tab, std::move(fnCallback));
      this->IsModified_ = true;
   }

   seed::TreeSP HandleGetSapling(seed::Tab&) {
      return this->Seed_->GetSapling();
   }
   void HandleSeedCommand(PolicyMaps::Locker& locker, SeedOpResult& res, StrView cmdln, seed::FnCommandResultHandler&& resHandler) {
      this->Seed_->OnSeedCommand(locker, res, cmdln, std::move(resHandler));
   }
};

struct PolicyTree::TreeOp : public seed::TreeOp {
   fon9_NON_COPY_NON_MOVE(TreeOp);
   using base = seed::TreeOp;
   TreeOp(PolicyTree& tree) : base(tree) {
   }

   static void MakePolicyRecordView(PolicyItemMap::iterator ivalue, seed::Tab* tab, RevBuffer& rbuf) {
      if (tab)
         FieldsCellRevPrint(tab->Fields_, seed::SimpleRawRd{**ivalue}, rbuf, seed::GridViewResult::kCellSplitter);
      RevPrint(rbuf, (*ivalue)->PolicyId_);
   }
   void GridView(const seed::GridViewRequest& req, seed::FnGridViewOp fnCallback) override {
      seed::GridViewResult res{this->Tree_, req.Tab_};
      {
         PolicyMaps::Locker maps{static_cast<PolicyTree*>(&this->Tree_)->PolicyMaps_};
         seed::MakeGridView(maps->ItemMap_, this->GetIteratorForGv(maps->ItemMap_, req.OrigKey_),
                            req, res, &MakePolicyRecordView);
      } // unlock.
      fnCallback(res);
   }

   void OnPodOp(PolicyMaps::Locker& maps, PolicyItem& rec, seed::FnPodOp&& fnCallback, bool isForceWrite = false) {
      PodOp op{rec, this->Tree_, seed::OpResult::no_error, maps, isForceWrite};
      fnCallback(op, &op);
      if (op.IsModified_) {
         op.Lock();
         maps->WriteUpdated(rec);
      }
   }
   void Get(StrView strKeyText, seed::FnPodOp fnCallback) override {
      {
         PolicyMaps::Locker maps{static_cast<PolicyTree*>(&this->Tree_)->PolicyMaps_};
         auto               ifind = this->GetIteratorForPod(maps->ItemMap_, strKeyText);
         if (ifind != maps->ItemMap_.end()) {
            this->OnPodOp(maps, **ifind, std::move(fnCallback));
            return;
         }
      } // unlock.
      fnCallback(seed::PodOpResult{this->Tree_, seed::OpResult::not_found_key, strKeyText}, nullptr);
   }
   void Add(StrView strKeyText, seed::FnPodOp fnCallback) override {
      if (this->IsTextBegin(strKeyText) || this->IsTextEnd(strKeyText)) {
         fnCallback(seed::PodOpResult{this->Tree_, seed::OpResult::not_found_key, strKeyText}, nullptr);
         return;
      }
      PolicyMaps::Locker maps{static_cast<PolicyTree*>(&this->Tree_)->PolicyMaps_};
      auto               ifind = maps->ItemMap_.find(strKeyText);
      bool               isForceWrite = false;
      if (ifind == maps->ItemMap_.end()) {
         isForceWrite = true;
         PolicyItemSP rec = static_cast<PolicyTree*>(&this->Tree_)->MakePolicy(strKeyText);
         ifind = maps->ItemMap_.insert(std::move(rec)).first;
      }
      this->OnPodOp(maps, **ifind, std::move(fnCallback), isForceWrite);
   }
   void Remove(StrView strKeyText, seed::Tab* tab, seed::FnPodRemoved fnCallback) override {
      seed::PodRemoveResult res{this->Tree_, seed::OpResult::not_found_key, strKeyText, tab};
      if (static_cast<PolicyTree*>(&this->Tree_)->Delete(strKeyText))
         res.OpResult_ = seed::OpResult::removed_pod;
      fnCallback(res);
   }
};
fon9_WARN_POP;

void PolicyTree::OnTreeOp(seed::FnTreeOp fnCallback) {
   TreeOp op{*this};
   fnCallback(seed::TreeOpResult{this, seed::OpResult::no_error}, &op);
}

} } // namespaces
