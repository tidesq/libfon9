/// \file fon9/auth/PolicyMaster.cpp
/// \author fonwinz@gmail.com
#include "fon9/auth/PolicyMaster.hpp"

namespace fon9 { namespace auth {

MasterPolicyItem::MasterPolicyItem(const StrView& policyId, MasterPolicyTreeSP owner)
   : base{policyId}
   , OwnerMasterTree_{std::move(owner)} {
}
seed::TreeSP MasterPolicyItem::GetSapling() {
   return this->DetailPolicyTree_;
}
void MasterPolicyItem::SetRemoved(PolicyTable&) {
   this->DetailPolicyTree_.reset();
}

} } // namespaces
