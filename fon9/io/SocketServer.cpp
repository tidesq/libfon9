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
   this->AcceptedSocketOptions_.SetDefaults();
   this->AcceptedClientOptions_ = DeviceOptions{};
   this->ListenConfig_.Options_.SO_REUSEADDR_ = 1;
   this->ListenConfig_.Options_.SO_REUSEPORT_ = 1;
   this->ListenBacklog_ = 5;
   this->ServiceArgs_.ThreadCount_ = GetDefaultServerThreadCount();
   this->ServiceArgs_.Capacity_ = 0;
   this->ServiceArgs_.CpuAffinity_.clear();
   this->ServiceArgs_.HowWait_ = HowWait::Block;
}

SocketServerConfig::Parser::~Parser() {
   if (this->Owner_.ServiceArgs_.ThreadCount_ <= 0)
      this->Owner_.ServiceArgs_.ThreadCount_ = GetDefaultServerThreadCount();
}

ConfigParser::Result SocketServerConfig::Parser::OnTagValue(StrView tag, StrView& value) {
   if (tag == "ListenBacklog") {
      if ((this->Owner_.ListenBacklog_ = StrTo(value, int{5})) <= 0)
         this->Owner_.ListenBacklog_ = 5;
   }
   else if (tag == "ClientOptions") { // value = "{AcceptedSocketOptions_|AcceptedClientOptions_}"
      struct ClientParser : public ConfigParser {
         fon9_NON_COPY_NON_MOVE(ClientParser);
         Parser& ParentParser_;
         ClientParser(Parser& pparser) : ParentParser_(pparser) {
         }
         Result OnTagValue(StrView tag, StrView& value) override {
            return this->ParentParser_.OnTagValueClient(tag, value);
         }
         bool OnErrorBreak(ErrorEventArgs& e) override {
            return this->ParentParser_.OnErrorBreakClient(e);
         }
      };
      value = SbrFetchInsideNoTrim(value);
      return ClientParser{*this}.Parse(value);
   }
   else {
      Result res = BaseParser::OnTagValue(tag, value);
      if(res == Result::EUnknownTag)
         if((res = this->Owner_.ServiceArgs_.OnTagValue(tag, value)) == Result::EUnknownTag)
            return this->Owner_.ListenConfig_.OnTagValue(tag, value);
      return res;
   }
   return Result::Success;
}

ConfigParser::Result SocketServerConfig::Parser::OnTagValueClient(StrView tag, StrView& value) {
   Result res = this->Owner_.AcceptedClientOptions_.OnTagValue(tag, value);
   if (res == Result::EUnknownTag)
      return this->Owner_.AcceptedSocketOptions_.OnTagValue(tag, value);
   return res;
}

bool SocketServerConfig::Parser::OnErrorBreakClient(ErrorEventArgs& e) {
   (void)e;
   return true;
}

bool SocketServerConfig::CreateListenSocket(Socket& soListen, SocketResult& soRes) const {
   if(soListen.CreateDeviceSocket(this->ListenConfig_.GetAF(), SocketType::Stream, soRes)
      && soListen.SetSocketOptions(this->ListenConfig_.Options_, soRes)
      && soListen.Bind(this->ListenConfig_.AddrBind_, soRes)) {
      if (::listen(soListen.GetSocketHandle(), this->ListenBacklog_) == 0)
         return true;
      soRes = SocketResult{"listen"};
   }
   return false;
}

} } // namespaces
