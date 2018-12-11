// \file fon9/fix/FixConfig.cpp
// \author fonwinz@gmail.com
#include "fon9/fix/FixConfig.hpp"

namespace fon9 { namespace fix {

const FixMsgTypeConfig* FixConfig::Get(StrView msgType) const {
   if (fon9_LIKELY(msgType.size() == 1)) {
      int idx = Alpha2Seq(*msgType.begin());
      if (0 <= idx && idx < static_cast<int>(numofele(this->Configs_)))
         return &this->Configs_[idx];
   }
   ConfigMap::const_iterator ifind = this->ConfigMap_.find(CharVector::MakeRef(msgType));
   return(ifind == this->ConfigMap_.end() ? nullptr : &ifind->second);
}
FixMsgTypeConfig& FixConfig::Fetch(StrView msgType) {
   if (fon9_LIKELY(msgType.size() == 1)) {
      int idx = Alpha2Seq(*msgType.begin());
      if (0 <= idx && idx < static_cast<int>(numofele(this->Configs_)))
         return this->Configs_[idx];
   }
   return this->ConfigMap_.kfetch(CharVector{msgType}).second;
}

} } // namespaces
