/// \file fon9/auth/PolicyAcl.cpp
/// \author fonwinz@gmail.com
#include "fon9/auth/PolicyAcl.hpp"
#include "fon9/auth/PolicyMaster.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/BitvArchive.hpp"

namespace fon9 { namespace auth {

class PolicyAclTree : public MasterPolicyTree {
   fon9_NON_COPY_NON_MOVE(PolicyAclTree);
   using base = MasterPolicyTree;

   using DetailTableImpl = seed::AccessList;
   using DetailTree = DetailPolicyTreeTable<DetailTableImpl, KeyMakerCharVector>;
   using DetailTable = DetailTree::DetailTable;

   struct MasterItem : public MasterPolicyItem {
      fon9_NON_COPY_NON_MOVE(MasterItem);
      using base = MasterPolicyItem;
      seed::AclPath  Home_;
      MasterItem(const StrView& policyId, MasterPolicyTreeSP owner)
         : base(policyId, std::move(owner)) {
         this->DetailPolicyTree_.reset(new DetailTree{*this});
      }
      void LoadPolicy(DcQueue& buf) override {
         unsigned ver = 0;
         DetailTable::Locker pmap{static_cast<DetailTree*>(this->DetailPolicyTree_.get())->DetailTable_};
         BitvInArchive{buf}(ver, this->Home_, *pmap);
      }
      void SavePolicy(RevBuffer& rbuf) override {
         const unsigned ver = 0;
         DetailTable::ConstLocker pmap{static_cast<DetailTree*>(this->DetailPolicyTree_.get())->DetailTable_};
         BitvOutArchive{rbuf}(ver, this->Home_, *pmap);
      }
   };
   using MasterItemSP = intrusive_ptr<MasterItem>;

   PolicyItemSP MakePolicy(const StrView& policyId) override {
      return PolicyItemSP{new MasterItem(policyId, this)};
   }

   static seed::LayoutSP MakeLayout() {
      seed::Fields fields;
      fields.Add(fon9_MakeField(Named{"HomePath"}, MasterItem, Home_));
      seed::TabSP tab{new seed::Tab(Named{"Acl"}, std::move(fields), seed::MakeAclTreeLayout())};
      return new seed::Layout1(fon9_MakeField(Named{"PolicyId"}, PolicyItem, PolicyId_),
                               std::move(tab));
   }

public:
   PolicyAclTree() : base{MakeLayout()} {
   }

   bool GetPolicy(const StrView& policyId, seed::AclConfig& res) const {
      struct ResultHandler {
         seed::AclConfig* Result_;
         void InLocking(const PolicyItem& master) {
            this->Result_->Home_ = static_cast<const MasterItem*>(&master)->Home_;
         }
         void OnUnlocked(DetailPolicyTree& detailTree) {
            DetailTable::Locker pmap{static_cast<DetailTree*>(&detailTree)->DetailTable_};
            this->Result_->Acl_ = *pmap;
         }
      };
      return base::GetPolicy(policyId, ResultHandler{&res});
   }
};

//--------------------------------------------------------------------------//

PolicyAclAgent::PolicyAclAgent(seed::MaTree* authMgrAgents, std::string name)
   : base(new PolicyAclTree{}, std::move(name)) {
   (void)authMgrAgents;
}

bool PolicyAclAgent::GetPolicy(const AuthResult& authr, PolicyConfig& res) {
   if (!static_cast<PolicyAclTree*>(this->Sapling_.get())->GetPolicy(authr.GetPolicyId(ToStrView(this->Name_)), res))
      return false;
   static const char kUserId[] = "{UserId}";
   StrView userId = authr.GetUserId();
   res.Home_ = CharVectorReplace(ToStrView(res.Home_), kUserId, userId);
   seed::AccessList acl{std::move(res.Acl_)};
   for (auto& v : acl)
      res.Acl_.kfetch(CharVectorReplace(ToStrView(v.first), kUserId, userId)).second = v.second;
   return true;
}

} } // namespaces
