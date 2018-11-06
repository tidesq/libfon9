/// \file fon9/io/SocketClientConfig.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/SocketClientConfig.hpp"
#include "fon9/StrTo.hpp"

namespace fon9 { namespace io {

void SocketClientConfig::SetDefaults() {
   base::SetDefaults();
   this->DomainNames_.clear();
   this->TimeoutSecs_ = 20;
}

ConfigParser::Result SocketClientConfig::Parser::OnTagValue(StrView tag, StrView& value) {
   if (tag.size() == 2
       && toupper(static_cast<unsigned char>(tag.Get1st())) == 'D'
       && toupper(static_cast<unsigned char>(tag.begin()[1])) == 'N')
      // "dn" or "DN"
      static_cast<SocketClientConfig*>(&this->Owner_)->DomainNames_ = value.ToString();
   else if (tag == "Timeout")
      static_cast<SocketClientConfig*>(&this->Owner_)->TimeoutSecs_ = StrTo(value, 0u);
   else
      return BaseParser::OnTagValue(tag, value);
   return ConfigParser::Result::Success;
}

} } // namespaces
