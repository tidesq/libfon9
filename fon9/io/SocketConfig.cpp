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

//--------------------------------------------------------------------------//

StrView SocketOptions::ParseConfig(StrView cfgstr, FnOnTagValue fnUnknownField) {
   StrView res;
   while (!cfgstr.empty()) {
      StrView value = SbrFetchField(cfgstr, '|');
      StrView tag = StrSplitTrim(value, '=');
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
      else if (!fnUnknownField || !fnUnknownField(tag, value)) {
         if (res.empty())
            res.Reset(tag.begin(), value.end() ? value.end() : tag.end());
      }
   }
   return res;
}

StrView SocketConfig::ParseConfig(StrView cfgstr, SocketAddress* addrDefault, FnOnTagValue fnUnknownField) {
   StrView  res = this->Options_.ParseConfig(cfgstr, [this, &addrDefault, &fnUnknownField](StrView tag, StrView value) {
      if (tag == "Bind")
         this->AddrBind_.FromStr(value);
      else if (tag == "Remote")
         this->AddrRemote_.FromStr(value);
      else if (value.begin() == nullptr && addrDefault && !addrDefault->GetPort()) {
         addrDefault->FromStr(tag);
         addrDefault = nullptr;
      }
      else
         return (fnUnknownField && fnUnknownField(tag, value));
      return true;
   });
   if (this->AddrBind_.Addr_.sa_family == AF_UNSPEC)
      this->AddrBind_.Addr_.sa_family = this->AddrRemote_.Addr_.sa_family;
   else if (this->AddrRemote_.Addr_.sa_family == AF_UNSPEC)
      this->AddrRemote_.Addr_.sa_family = this->AddrBind_.Addr_.sa_family;
   return res;
}

} } // namespaces
