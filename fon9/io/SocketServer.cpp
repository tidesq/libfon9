/// \file fon9/io/SocketServer.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/SocketServer.hpp"
#include "fon9/StrTo.hpp"

namespace fon9 { namespace io {

inline static unsigned GetDefaultServerThreadCount() {
   unsigned c = std::thread::hardware_concurrency() / 2;
   return c ? c : 2;
}

void SocketServerConfig::SetDefaults() {
   this->ListenConfig_.SetDefaults();
   this->AcceptedClientOptions_.SetDefaults();
   this->ListenConfig_.Options_.SO_REUSEADDR_ = 1;
   this->ListenConfig_.Options_.SO_REUSEPORT_ = 1;
   this->ListenBacklog_ = 5;
   this->ServiceArgs_.ThreadCount_ = GetDefaultServerThreadCount();
   this->ServiceArgs_.Capacity_ = 0;
   this->ServiceArgs_.CpuAffinity_.clear();
   this->ServiceArgs_.HowWait_ = HowWait::Block;
}

StrView SocketServerConfig::ParseConfig(StrView cfgstr, FnOnTagValue fnUnknownField, FnOnTagValue fnClientOptionsUnknownField) {
   StrView clires;
   StrView res = this->ListenConfig_.ParseConfigListen(cfgstr,
                                                       [this, &fnUnknownField, &fnClientOptionsUnknownField, &clires](StrView tag, StrView value) {
      if (tag == "ListenBacklog") {
         if ((this->ListenBacklog_ = StrTo(value, int{5})) <= 0)
            this->ListenBacklog_ = 5;
      }
      else if (tag == "ClientOptions") {
         StrView cres = this->AcceptedClientOptions_.ParseConfig(FetchFirstBr(value), fnClientOptionsUnknownField);
         if (clires.empty())
            clires = cres;
      }
      else if (this->ServiceArgs_.FromTagValue(tag, value) == nullptr)
         return true;
      else
         return (fnUnknownField && fnUnknownField(tag, value));
      return true;
   });
   if (this->ServiceArgs_.ThreadCount_ <= 0)
      this->ServiceArgs_.ThreadCount_ = GetDefaultServerThreadCount();
   if (!res.empty())
      return res;
   return clires;
}

SocketResult SocketServerConfig::CreateListenSocket(Socket& soListen) const {
   SocketResult res = soListen.CreateDeviceSocket(this->ListenConfig_.GetAF(), SocketType::Stream);
   if (res) {
      if ((res = soListen.SetSocketOptions(this->ListenConfig_.Options_)).IsSuccess())
         if ((res = soListen.Bind(this->ListenConfig_.AddrBind_)).IsSuccess())
            if (::listen(soListen.GetSocketHandle(), this->ListenBacklog_))
               return SocketResult{"listen"};
   }
   return res;
}

} } // namespaces
