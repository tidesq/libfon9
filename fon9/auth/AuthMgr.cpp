/// \file fon9/auth/AuthMgr.cpp
/// \author fonwinz@gmail.com
#include "fon9/auth/AuthMgr.hpp"
#include "fon9/RevPrint.hpp"
#include <algorithm> // std::replace()

namespace fon9 { namespace auth {

void AuthResult::UpdateRoleConfig() {
   this->AuthMgr_->RoleMgr_->GetRole(ToStrView(this->RoleId_), *this);
}
StrView AuthResult::GetPolicyId(StrView policyName) const {
   auto ifind = this->PolicyKeys_.find(PolicyName::MakeRef(policyName));
   if (ifind == this->PolicyKeys_.end())
      return ToStrView(this->RoleId_);
   return ToStrView(ifind->second);
}
void AuthResult::RevPrintUser(RevBuffer& rbuf) const {
   RevPrint(rbuf, this->AuthcId_);
   if (!this->AuthzId_.empty())
      RevPrint(rbuf, this->AuthzId_, '/');
}
void AuthResult::RevPrintUFrom(RevBuffer& rbuf, StrView devid) const {
   RevPrint(rbuf, '|', devid);
   this->RevPrintUser(rbuf);
   RevPrint(rbuf, "U=");
}
std::string AuthResult::MakeUFrom(StrView devid) const {
   RevBufferList rbuf{128};
   RevPrintUFrom(rbuf, devid);
   return BufferTo<std::string>(rbuf.MoveOut());
}

AuthSession::~AuthSession() {
}

//--------------------------------------------------------------------------//

AuthMgr::AuthMgr(seed::MaTreeSP ma, std::string name, InnDbfSP storage)
   : base{name}
   , MaRoot_{std::move(ma)}
   , Agents_{new seed::MaTree{"Agents"}}
   , Storage_{std::move(storage)}
   , RoleMgr_{new RoleMgr{"RoleMgr"}} {
   this->RoleMgr_->LinkStorage(this->Storage_);
   this->Agents_->Add(this->RoleMgr_);
}

seed::TreeSP AuthMgr::GetSapling() {
   return this->Agents_;
}

AuthSessionSP AuthMgr::CreateAuthSession(StrView mechanismName, FnOnAuthVerifyCB cb) {
   AuthAgentSP aa = this->Agents_->Get<AuthAgent>(mechanismName);
   if (!aa) {
      CharVector name{StrView{fon9_kCSTR_AuthAgent_Prefix}};
      name.append(mechanismName);
      std::replace(name.begin(), name.end(), '-', '_');
      aa = this->Agents_->Get<AuthAgent>(ToStrView(name));
      if(!aa)
         return nullptr;
   }
   return aa->CreateAuthSession(this, std::move(cb));
}

std::string AuthMgr::GetSaslMechList(char chSpl) const {
   const auto  kCSTR_AuthAgent_Prefix_length = sizeof(fon9_kCSTR_AuthAgent_Prefix) - 1;
   auto        mechList{this->Agents_->GetList(fon9_kCSTR_AuthAgent_Prefix)};
   std::string res;
   for (auto& i : mechList) {
      if (!res.empty())
         res.push_back(chSpl);
      res.append(i->Name_.c_str() + kCSTR_AuthAgent_Prefix_length, i->Name_.size() - kCSTR_AuthAgent_Prefix_length);
   }
   std::replace(res.begin(), res.end(), '_', '-');
   return res;
}

} } // namespaces
