/// \file fon9/io/DeviceParseConfig.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_DeviceParseConfig_hpp__
#define __fon9_io_DeviceParseConfig_hpp__
#include "fon9/buffer/RevBufferList.hpp"

namespace fon9 { namespace io {

/// \ingroup io
/// 協助處理 Device::OpImpl_Open(std::string cfgstr);
/// \code
/// class MyDevice {
///
///   template <class DeviceT>
///   friend bool OpThr_DeviceParseConfig(DeviceT& dev, StrView cfgstr);
///
///   Config Config_;
///
///   void OpImpl_Open(std::string cfgstr) override {
///      ... clear for open ...
///      if (OpThr_DeviceParseConfig(*this, &cfgstr))
///         ... this->Config_ is ready, open device use this->Config_;
///   }
/// };
/// \endcode
template <class DeviceT>
inline bool OpThr_DeviceParseConfig(DeviceT& dev, StrView cfgstr) {
   dev.OpImpl_SetState(State::Opening, cfgstr);
   dev.Config_.SetDefaults();

   using Config = decltype(dev.Config_);
   using ParserBase = typename Config::Parser;
   struct Parser : public ParserBase {
      fon9_NON_COPY_NON_MOVE(Parser);
      using Result = typename ParserBase::Result;
      using ErrorEventArgs = typename ParserBase::ErrorEventArgs;
      RevBufferList RBuf_{128};
      DeviceT&      Device_;
      Parser(DeviceT& dev) : ParserBase{dev.Config_}, Device_(dev) {
      }
      bool OnErrorBreak(ErrorEventArgs& e) override {
         RevPrint(this->RBuf_, "err=", e);
         return true;
      }
      Result OnTagValue(StrView tag, StrView& value) override {
         Result res = ParserBase::OnTagValue(tag, value);
         if (res == Result::EUnknownTag)
            return this->Device_.OpImpl_SetProperty(tag, value);
         return res;
      }
   };

   Parser pr{dev};
   if (pr.Parse(cfgstr) == ParserBase::Result::Success)
      return true;

   dev.OpImpl_SetState(State::ConfigError, ToStrView(BufferTo<std::string>(pr.RBuf_.MoveOut())));
   return false;
}

} } // namespaces
#endif//__fon9_io_DeviceParseConfig_hpp__
