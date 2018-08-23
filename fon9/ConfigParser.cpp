// \file fon9/ConfigParser.cpp
// \author fonwinz@gmail.com
#include "fon9/ConfigParser.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 {

ConfigParser::~ConfigParser() {
}

ConfigParser::Result ConfigParser::Parse(StrView& cfgstr, char chFieldDelim, char chEqual) {
   ErrorEventArgs args;
   while (!cfgstr.empty()) {
      args.Value_ = SbrFetchFieldNoTrim(cfgstr, chFieldDelim);
      args.Tag_   = StrTrimSplit(args.Value_, chEqual);
      const char* const pvalbeg = args.Value_.begin();
      args.Result_ = this->OnTagValue(args.Tag_, args.Value_);
      if (args.Result_ == Result::Success)
         continue;
      args.ErrPos_ = (args.Result_ == Result::EUnknownTag || pvalbeg == nullptr ? args.Tag_.begin() : args.Value_.begin());
      if (pvalbeg == nullptr)
         args.Value_.Reset(args.Tag_.end(), args.Tag_.end());
      else
         args.Value_.SetBegin(pvalbeg);
      if (this->OnErrorBreak(args)) {
         cfgstr.SetBegin(args.ErrPos_);
         return args.Result_;
      }
   }
   return Result::Success;
}

bool ConfigParser::OnErrorBreak(ErrorEventArgs& e) {
   (void)e;
   return true;
}

fon9_API void RevPrint(RevBuffer& rbuf, const ConfigParser::ErrorEventArgs& e) {
   if (e.Result_ == ConfigParser::Result::Success)
      return;
   RevPutChar(rbuf, ']');
   RevPutMem(rbuf, e.Tag_.begin(), e.Value_.end());
   RevPrint(rbuf, " @", (e.ErrPos_ - e.Tag_.begin() + 1), " [");
   switch(e.Result_) {
   default:
   case ConfigParser::Result::Success:
      break;
   #define case_value(eval) case ConfigParser::Result::eval: RevPrint(rbuf, #eval); break;
   case_value(EUnknownTag);
   case_value(EValueTooLarge);
   case_value(EValueTooSmall);
   case_value(EInvalidValue);
   };
   RevPrint(rbuf, e.Result_, ':');
}

bool ConfigParserMsg::OnErrorBreak(ErrorEventArgs& e) {
   RevPrint(this->RBuf_, "|err=", e);
   return false;
}

} // namespace
