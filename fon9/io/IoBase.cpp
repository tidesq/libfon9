/// \file fon9/io/IoBase.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/IoBase.hpp"
#include "fon9/Utility.hpp"
#include "fon9/TimeInterval.hpp"

namespace fon9 { namespace io {

// iostStrMap[] 應該只有資料, 不應該有用來初始化的程式碼!
// g++ 4.8.4 查看 IoBase.s 是否有最佳化:
//    g++ -S fon9/io/IoBase.cpp -o IoBase.s -I ~/devel/fon9 -O2 -std=c++11
// VC 19.00.23506.0: 查看 IoBase.cod 是否有最佳化:
//    cl fon9/io/IoBase.cpp /O2 /FAcs /I \devel\fon9 /D fon9_EXPORT /c
static const StrView iostStrMap[]{
   fon9_MAKE_ENUM_CLASS_StrView( 0, State, Initializing),//"0.Initializing"
   fon9_MAKE_ENUM_CLASS_StrView( 1, State, Initialized),
   fon9_MAKE_ENUM_CLASS_StrView( 2, State, ConfigError),
   fon9_MAKE_ENUM_CLASS_StrView( 3, State, Opening),
   fon9_MAKE_ENUM_CLASS_StrView( 4, State, WaitingLinkIn),
   fon9_MAKE_ENUM_CLASS_StrView( 5, State, Linking),
   fon9_MAKE_ENUM_CLASS_StrView( 6, State, LinkError),
   fon9_MAKE_ENUM_CLASS_StrView( 7, State, LinkReady),
   fon9_MAKE_ENUM_CLASS_StrView( 8, State, LinkBroken),
   fon9_MAKE_ENUM_CLASS_StrView( 9, State, Listening),
   fon9_MAKE_ENUM_CLASS_StrView(10, State, ListenBroken),
   fon9_MAKE_ENUM_CLASS_StrView(11, State, Lingering),
   fon9_MAKE_ENUM_CLASS_StrView(12, State, Closing),
   fon9_MAKE_ENUM_CLASS_StrView(13, State, Closed),
   fon9_MAKE_ENUM_CLASS_StrView(14, State, Disposing),
   fon9_MAKE_ENUM_CLASS_StrView(15, State, Destructing),
   {"?"}
};
fon9_API StrView GetStateStr(State st) {
   size_t ust = static_cast<size_t>(static_cast<std::underlying_type<State>::type>(st));
   if (ust >= numofele(iostStrMap))
      ust = numofele(iostStrMap) - 1;
   return iostStrMap[ust];
}

//--------------------------------------------------------------------------//

static ConfigParser::Result ParseTimeIntervalToMS(StrView& value, uint32_t& res) {
   TimeInterval ti{StrTo(&value, TimeInterval{})};
   res = (ti.GetOrigValue() <= 0 ? 0u : static_cast<uint32_t>((ti * 1000).GetIntPart()));
   return ConfigParser::Result::Success;
}

ConfigParser::Result DeviceOptions::OnTagValue(StrView tag, StrView& value) {
   if (tag == "SendASAP") {
      if (toupper(value.Get1st()) == 'N')
         this->Flags_ -= DeviceFlag::SendASAP;
      else
         this->Flags_ |= DeviceFlag::SendASAP;
      return ConfigParser::Result::Success;
   }
   if (tag == "RetryInterval")
      return ParseTimeIntervalToMS(value, this->LinkErrorRetryInterval_);
   if (tag == "ReopenInterval")
      return ParseTimeIntervalToMS(value, this->LinkBrokenReopenInterval_);
   return ConfigParser::Result::EUnknownTag;
}

} } // namespaces
