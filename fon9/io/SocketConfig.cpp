/// \file fon9/io/SocketConfig.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/SocketConfig.hpp"
#include "fon9/StrTo.hpp"

namespace fon9 { namespace io {

inline static void SetOptionsDefaultAfterZero(SocketOptions& opts) {
   opts.SO_SNDBUF_ = opts.SO_RCVBUF_ = -1;
   opts.TCP_NODELAY_ = 1;
}

void SocketOptions::SetDefaults() {
   ZeroStruct(this);
   SetOptionsDefaultAfterZero(*this);
}

void SocketConfig::SetDefaults() {
   ZeroStruct(this);
   SetOptionsDefaultAfterZero(this->Options_);
   this->AddrBind_.Addr_.sa_family = this->AddrRemote_.Addr_.sa_family = AF_UNSPEC;
}

void SocketConfig::SetAddrFamily() {
   if (this->AddrBind_.Addr_.sa_family == AF_UNSPEC)
      this->AddrBind_.Addr_.sa_family = this->AddrRemote_.Addr_.sa_family;
   else if (this->AddrRemote_.Addr_.sa_family == AF_UNSPEC)
      this->AddrRemote_.Addr_.sa_family = this->AddrBind_.Addr_.sa_family;
}

//--------------------------------------------------------------------------//

ConfigParser::Result SocketOptions::OnTagValue(StrView tag, StrView& value) {
   if (tag == "TcpNoDelay")
      this->TCP_NODELAY_ = (toupper(static_cast<unsigned char>(value.Get1st())) != 'N');
   else if (tag == "SNDBUF")
      this->SO_SNDBUF_ = StrTo(value, -1);
   else if (tag == "RCVBUF")
      this->SO_RCVBUF_ = StrTo(value, -1);
   else if (tag == "ReuseAddr")
      this->SO_REUSEADDR_ = (toupper(static_cast<unsigned char>(value.Get1st())) == 'Y');
   else if (tag == "ReusePort")
      this->SO_REUSEPORT_ = (toupper(static_cast<unsigned char>(value.Get1st())) == 'Y');
   else if (tag == "Linger") {
      if (toupper(static_cast<unsigned char>(value.Get1st())) == 'N') { // 開啟linger設定,但等候0秒.
         this->Linger_.l_onoff = 1;
         this->Linger_.l_linger = 0;
      }
   }
   else if (tag == "KeepAlive")
      this->KeepAliveInterval_ = StrTo(value, int{});
   else
      return ConfigParser::Result::EUnknownTag;
   return ConfigParser::Result::Success;
}

ConfigParser::Result SocketConfig::OnTagValue(StrView tag, StrView& value) {
   if (tag == "Bind")
      this->AddrBind_.FromStr(value);
   else if (tag == "Remote")
      this->AddrRemote_.FromStr(value);
   else
      return this->Options_.OnTagValue(tag, value);
   return ConfigParser::Result::Success;
}

ConfigParser::Result SocketConfig::Parser::OnTagValue(StrView tag, StrView& value) {
   if (value.begin() == nullptr && this->AddrDefault_ && !this->AddrDefault_->GetPort()) {
      this->AddrDefault_->FromStr(tag);
      this->AddrDefault_ = nullptr;
   }
   else
      return this->Owner_.OnTagValue(tag, value);
   return ConfigParser::Result::Success;
}

SocketConfig::Parser::~Parser() {
   this->Owner_.SetAddrFamily();
}

} } // namespaces
