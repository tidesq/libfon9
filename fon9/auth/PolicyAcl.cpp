/// \file fon9/auth/PolicyAcl.cpp
/// \author fonwinz@gmail.com
#include "fon9/auth/PolicyAcl.hpp"
#include "fon9/auth/PolicyMaster.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/BitvArchive.hpp"
#include "fon9/FilePath.hpp"

namespace fon9 { namespace auth {

bool AclPathParser::NormalizePath(StrView& path) {
   FilePath::StrList plist = FilePath::SplitPathList(path);
   if (!path.empty())
      return false;
   this->Path_.assign(ToStrView(FilePath::MergePathList(plist)));
   this->EndNames_.resize(plist.size());
   size_t index = 0;
   size_t pos = 0;
   for (auto& s : plist) {
      pos += s.size();
      this->EndNames_[index++] = pos;
      ++pos;
   }
   return true;
}

const char* AclPathParser::NormalizePathStr(StrView path) {
   this->Path_.assign(path);
   if (this->NormalizePath(path))
      return nullptr;
   this->EndNames_.clear();
   return path.begin();
}

AccessRight AclPathParser::GetAccess(const AccessList& acl) const {
   AclPath  path{this->Path_};
   size_t   L = this->EndNames_.size();
   for (;;) {
      auto ifind = acl.find(path);
      if (ifind != acl.end())
         return ifind->second;
      if (L == 0)
         return AccessRight::None;
      if (--L > 0)
         path.resize(this->EndNames_[L - 1]);
      else {
         size_t psz = path.size();
         char*  pbeg = path.begin();
         if (psz == 3 && pbeg[0] == '.' && pbeg[1] == '.' && pbeg[2] == '/') //path == "../"
            path.resize(2);
         else
            path.clear();
      }
   }
}

AccessRight AclPathParser::CheckAccess(const AccessList& acl, AccessRight needsAccess) const {
   AccessRight ac = this->GetAccess(acl);
   if (ac == AccessRight::None || (needsAccess != AccessRight::None && !IsEnumContains(ac, needsAccess)))
      return AccessRight::None;
   return ac;
}

void AclPathParser::DelPrefix() {
   if (fon9_LIKELY(this->EndNames_.size() > 1)) {
      size_t prefixSz = this->EndNames_[0] + 1;
      this->Path_.erase(0, prefixSz);
      this->EndNames_.erase(this->EndNames_.begin());
      for (size_t& psz : this->EndNames_)
         psz -= prefixSz;
   }
   else {
      this->Path_.clear();
      this->EndNames_.clear();
   }
}

void AclPathParser::AddPrefix(StrView prefix) {
   size_t  prefixSz = prefix.size();
   AclPath path;
   path.reserve(prefixSz + 1 + this->Path_.size());
   path.assign(prefix);
   path.push_back('/');
   path.append(ToStrView(this->Path_));
   this->Path_ = std::move(path);
   this->EndNames_.insert(this->EndNames_.begin(), prefixSz);
   ++prefixSz;
   size_t count = this->EndNames_.size();
   for (size_t L = 1; L < count; ++L)
      this->EndNames_[L] += prefixSz;
}

StrView AclPathParser::GetIndexPath(size_t index) const {
   if (index >= this->EndNames_.size())
      return StrView{nullptr};
   const char* pstr = this->Path_.begin();
   size_t plen = this->EndNames_[index];
   if (index > 0) {
      size_t sz = this->EndNames_[index - 1] + 1;
      pstr += sz;
      plen -= sz;
   }
   return StrView{pstr, plen};
}

   
//--------------------------------------------------------------------------//

class PolicyAclTree : public MasterPolicyTree {
   fon9_NON_COPY_NON_MOVE(PolicyAclTree);
   using base = MasterPolicyTree;

   using DetailTableImpl = AccessList;
   using DetailTree = DetailPolicyTreeTable<DetailTableImpl, KeyMakerCharVector>;
   using DetailTable = DetailTree::DetailTable;

   struct MasterItem : public MasterPolicyItem {
      fon9_NON_COPY_NON_MOVE(MasterItem);
      using base = MasterPolicyItem;
      AclPath  Home_;
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

   PolicyItemSP MakePolicy(const StrView& roleId) override {
      return PolicyItemSP{new MasterItem(roleId, this)};
   }

   static seed::LayoutSP MakeLayout() {
      seed::Fields fields;
      fields.Add(fon9_MakeField(seed::Named{"Rights"}, DetailTableImpl::value_type, second));
      seed::LayoutSP saplingLayout{
         new seed::Layout1(fon9_MakeField(seed::Named{"Path"}, DetailTableImpl::value_type, first),
                           new seed::Tab{seed::Named{"AclRights"}, std::move(fields)})};

      fields.Add(fon9_MakeField(seed::Named{"HomePath"}, MasterItem, Home_));
      seed::TabSP tab{new seed::Tab(seed::Named{"Acl"}, std::move(fields), std::move(saplingLayout))};
      return new seed::Layout1(seed::FieldSP{new seed::FieldCharVector(seed::Named{"PolicyId"}, 0)},
                               std::move(tab));
   }

public:
   PolicyAclTree() : base{MakeLayout()} {
   }

   bool GetPolicy(const StrView& policyId, AclConfig& res) const {
      struct ResultHandler {
         AclConfig* Result_;
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

bool PolicyAclAgent::GetPolicy(const AuthResult& authr, AclConfig& res) {
   if (!static_cast<PolicyAclTree*>(this->Sapling_.get())->GetPolicy(authr.GetPolicyId(ToStrView(this->Name_)), res))
      return false;
   static const char kUserId[] = "{UserId}";
   StrView userId = authr.GetUserId();
   res.Home_ = CharVectorReplace(ToStrView(res.Home_), kUserId, userId);
   AccessList acl{std::move(res.Acl_)};
   for (auto& v : acl)
      res.Acl_[CharVectorReplace(ToStrView(v.first), kUserId, userId)] = v.second;
   return true;
}

} } // namespaces
