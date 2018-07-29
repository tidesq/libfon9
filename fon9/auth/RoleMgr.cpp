/// \file fon9/auth/RoleMgr.cpp
/// \author fonwinz@gmail.com
#include "fon9/auth/RoleMgr.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/BitvArchive.hpp"

namespace fon9 { namespace auth {

RoleTree::RoleConfigTree::RoleConfigTree(RoleItem& owner)
   : base{owner.OwnerTree_->LayoutSP_->GetTab(0)->SaplingLayout_}
   , OwnerRole_(&owner) {
}

void RoleTree::RoleConfigTree::WriteUpdated(PolicyConfigs::Locker& lockedMap) {
   lockedMap.unlock();
   Maps::Locker  maps{this->OwnerRole_->OwnerTree_->Maps_};
   maps->WriteUpdated(*this->OwnerRole_);
}

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4265 /* class has virtual functions, but destructor is not virtual. */);
struct RoleTree::RoleConfigTree::PodOp : public seed::PodOpDefault {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = PodOpDefault;

   SeedValue&  Seed_;
   bool        IsModified_{false};
   PodOp(SeedValue& seed, Tree& sender, seed::OpResult res, const StrView& keyText)
      : base{sender, res, keyText}
      , Seed_(seed) {
   }

   void BeginRead(seed::Tab& tab, seed::FnReadOp fnCallback) override {
      this->BeginRW(tab, std::move(fnCallback), seed::SimpleRawRd{this->Seed_});
   }
   void BeginWrite(seed::Tab& tab, seed::FnWriteOp fnCallback) override {
      this->BeginRW(tab, std::move(fnCallback), seed::SimpleRawWr{this->Seed_});
      this->IsModified_ = true;
   }
};

struct RoleTree::RoleConfigTree::TreeOp : public seed::TreeOp {
   fon9_NON_COPY_NON_MOVE(TreeOp);
   using base = seed::TreeOp;
   TreeOp(RoleConfigTree& tree) : base(tree) {
   }
   static PolicyKeys::iterator GetStartIterator(PolicyKeys& policies, StrView strKeyText) {
      return base::GetStartIterator(policies, strKeyText, [](const StrView& strKey) { return SeedKey::MakeRef(strKey); });
   }
   static void MakePolicyConfigView(PolicyKeys::iterator ivalue, seed::Tab* tab, RevBuffer& rbuf) {
      if (tab)
         seed::FieldsCellRevPrint(tab->Fields_, seed::SimpleRawRd{ivalue->second}, rbuf, seed::GridViewResult::kCellSplitter);
      RevPrint(rbuf, ivalue->first);
   }
   virtual void GridView(const seed::GridViewRequest& req, seed::FnGridViewOp fnCallback) {
      seed::GridViewResult res{this->Tree_};
      {
         PolicyConfigs::Locker map{static_cast<RoleConfigTree*>(&this->Tree_)->PolicyConfigs_};
         seed::MakeGridView(*map, this->GetStartIterator(*map, req.OrigKey_),
                            req, res, &MakePolicyConfigView);
      } // unlock.
      fnCallback(res);
   }

   void OnPodOp(PolicyConfigs::Locker& lockedMap, const SeedKey& key, SeedValue& value, seed::FnPodOp&& fnCallback, bool isForceWrite = false) {
      PodOp op{value, this->Tree_, seed::OpResult::no_error, ToStrView(key)};
      fnCallback(op, &op);
      if (op.IsModified_ || isForceWrite)
         static_cast<RoleConfigTree*>(&this->Tree_)->WriteUpdated(lockedMap);
   }
   virtual void Get(StrView strKeyText, seed::FnPodOp fnCallback) override {
      {
         PolicyConfigs::Locker lockedMap{static_cast<RoleConfigTree*>(&this->Tree_)->PolicyConfigs_};
         auto                  ifind = this->GetFindIterator(*lockedMap, strKeyText, [](const StrView& strKey) { return SeedKey::MakeRef(strKey); });
         if (ifind != lockedMap->end()) {
            this->OnPodOp(lockedMap, ifind->first, ifind->second, std::move(fnCallback));
            return;
         }
      } // unlock.
      fnCallback(seed::PodOpResult{this->Tree_, seed::OpResult::not_found_key, strKeyText}, nullptr);
   }
   virtual void Add(StrView strKeyText, seed::FnPodOp fnCallback) override {
      if (this->IsTextBegin(strKeyText) || this->IsTextEnd(strKeyText)) {
         fnCallback(seed::PodOpResult{this->Tree_, seed::OpResult::not_found_key, strKeyText}, nullptr);
         return;
      }
      SeedKey               key{SeedKey::MakeRef(strKeyText)};
      PolicyConfigs::Locker lockedMap{static_cast<RoleConfigTree*>(&this->Tree_)->PolicyConfigs_};
      auto                  ifind = lockedMap->find(key);
      if (ifind != lockedMap->end())
         this->OnPodOp(lockedMap, ifind->first, ifind->second, std::move(fnCallback));
      else
         this->OnPodOp(lockedMap, key, (*lockedMap)[key], std::move(fnCallback), true);
   }
   virtual void Remove(StrView strKeyText, seed::Tab* tab, seed::FnPodRemoved fnCallback) override {
      seed::PodRemoveResult res{this->Tree_, seed::OpResult::not_found_key, strKeyText, tab};
      SeedKey               key{SeedKey::MakeRef(strKeyText)};
      {
         PolicyConfigs::Locker lockedMap{static_cast<RoleConfigTree*>(&this->Tree_)->PolicyConfigs_};
         auto                  ifind = lockedMap->find(key);
         if (ifind != lockedMap->end()) {
            lockedMap->erase(ifind);
            res.OpResult_ = seed::OpResult::removed_pod;
            static_cast<RoleConfigTree*>(&this->Tree_)->WriteUpdated(lockedMap);
         }
      }
      fnCallback(res);
   }
};
fon9_WARN_POP;

void RoleTree::RoleConfigTree::OnTreeOp(seed::FnTreeOp fnCallback) {
   if (fnCallback) {
      TreeOp op{*this};
      fnCallback(seed::TreeOpResult{this, seed::OpResult::no_error}, &op);
   }
}

//--------------------------------------------------------------------------//

fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list
RoleTree::RoleItem::RoleItem(const StrView& roleId, RoleTreeSP owner)
   : PolicyItem{roleId}
   , OwnerTree_{std::move(owner)}
   , RoleConfig_{new RoleConfigTree{*this}} {
}
fon9_MSC_WARN_POP;

seed::TreeSP RoleTree::RoleItem::GetSapling() {
   return this->RoleConfig_;
}
void RoleTree::RoleItem::LoadPolicy(DcQueue& buf) {
   unsigned ver = 0;
   PolicyConfigs::Locker pmap{this->RoleConfig_->PolicyConfigs_};
   BitvInArchive{buf}(ver, this->Description_, *pmap);
}
void RoleTree::RoleItem::SavePolicy(RevBuffer& rbuf) {
   const unsigned ver = 0;
   PolicyConfigs::ConstLocker pmap{this->RoleConfig_->PolicyConfigs_};
   BitvOutArchive{rbuf}(ver, this->Description_, *pmap);
}
void RoleTree::RoleItem::SetRemoved(PolicyTable&) {
   this->RoleConfig_.reset();
}

//--------------------------------------------------------------------------//

seed::LayoutSP RoleTree::MakeLayout() {
   seed::Fields fields;
   fields.Add(seed::MakeField(seed::Named{"PolicyId"}, 0, *static_cast<RoleConfigTree::SeedValue*>(nullptr)));
   seed::LayoutSP saplingLayout{
      new seed::Layout1(fon9_MakeField(seed::Named{"PolicyName"}, PolicyKeys::value_type, first),
                        new seed::Tab{seed::Named{"RoleConfig"}, std::move(fields)})};

   fields.Add(fon9_MakeField(seed::Named{"Description"}, RoleItem, Description_));
   seed::TabSP tab{new seed::Tab(seed::Named{"RoleCfg"}, std::move(fields), std::move(saplingLayout))};
   return new seed::Layout1(seed::FieldSP{new seed::FieldCharVector(seed::Named{"RoleId"}, 0)},
                            std::move(tab));
}

AuthR RoleTree::GetRole(StrView roleId, RoleConfig& res) const {
   res.RoleId_.assign(roleId);
   Maps::ConstLocker  maps{this->Maps_};
   auto               ifind = maps->ItemMap_.find(roleId);
   if (ifind == maps->ItemMap_.end())
      return AuthR{fon9_Auth_ERoleId};
   RoleCfgSP roleConfig = static_cast<RoleItem*>(ifind->second.get())->RoleConfig_;
   maps.unlock();

   PolicyConfigs::Locker pmap{roleConfig->PolicyConfigs_};
   res.PolicyKeys_ = *pmap;
   return AuthR{fon9_Auth_Success};
}

PolicyItemSP RoleTree::MakePolicy(const StrView& roleId) {
   return PolicyItemSP{new RoleItem(roleId, this)};
}

//--------------------------------------------------------------------------//

RoleMgr::RoleMgr(std::string name)
   : base(new RoleTree{}, std::move(name)) {
}

void RoleMgr::GetRole(StrView roleId, FnOnGetRoleCB cb) {
   RoleConfig  res;
   AuthR       rcode = static_cast<RoleTree*>(this->Sapling_.get())->GetRole(roleId, res);
   cb(std::move(rcode), std::move(res));
}

} } // namespaces
