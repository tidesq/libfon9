/// \file fon9/io/SocketClient.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/SocketClient.hpp"
#include "fon9/StrTo.hpp"

namespace fon9 { namespace io {

void SocketClientConfig::SetDefaults() {
   base::SetDefaults();
   this->DomainNames_.clear();
   this->TimeoutSecs_ = 20;
}

StrView SocketClientConfig::ParseConfig(StrView cfgstr, FnOnTagValue fnUnknownField) {
   return base::ParseConfigClient(cfgstr, [this, &fnUnknownField](StrView tag, StrView value) {
      if (tag.size() == 2 && toupper(static_cast<unsigned char>(tag.Get1st())) == 'D' && toupper(static_cast<unsigned char>(tag.begin()[1])) == 'N')
         // "dn" or "DN"
         this->DomainNames_ = value.ToString();
      else if (tag == "Timeout")
         this->TimeoutSecs_ = StrTo(value, 0u);
      else
         return (fnUnknownField && fnUnknownField(tag, value));
      return true;
   });
}

} } // namespaces
