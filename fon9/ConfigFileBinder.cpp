// \file fon9/ConfigFileBinder.cpp
// \author fonwinz@gmail.com
#include "fon9/ConfigFileBinder.hpp"
#include "fon9/File.hpp"
#include "fon9/Log.hpp"

namespace fon9 {

static std::string MakeErr(StrView logHeader, StrView func, File::Result fres, File& fd) {
   std::string errstr = RevPrintTo<std::string>("func=", func, '|', fres, "|file=", fd.GetOpenName());
   if (logHeader.begin() != nullptr)
      fon9_LOG_ERROR(logHeader, '|', errstr);
   return errstr;
}
static std::string CheckRW(StrView logHeader, StrView func, File::Result fres, File::SizeType expected, File& fd) {
   if (!fres)
      return MakeErr(logHeader, func, fres, fd);
   if (fres.GetResult() == expected)
      return std::string{};
   std::string errstr = RevPrintTo<std::string>("func=", func,
                                                "|err.size=", fres.GetResult(),
                                                "|expected=", expected,
                                                "|file=", fd.GetOpenName());
   if (logHeader.begin() != nullptr)
      fon9_LOG_ERROR(logHeader, '|', errstr);
   return errstr;
}

std::string ConfigFileBinder::OpenRead(StrView logHeader, std::string cfgfn) {
   this->FileName_ = cfgfn;
   File fd;
   auto fres = fd.Open(std::move(cfgfn), FileMode::Read);
   if (!fres)
      return MakeErr(logHeader, "Open", fres, fd);
   fres = fd.GetFileSize();
   if (!fres)
      return MakeErr(logHeader, "GetFileSize", fres, fd);
   this->ConfigStr_.resize(fres.GetResult());
   if (this->ConfigStr_.size() <= 0)
      return std::string{};
   fres = fd.Read(0, &*this->ConfigStr_.begin(), this->ConfigStr_.size());
   return CheckRW(logHeader, "Read", fres, this->ConfigStr_.size(), fd);
}
std::string ConfigFileBinder::Write(StrView logHeader, std::string cfgstr) {
   if (this->ConfigStr_ == cfgstr)
      return std::string{};
   if (this->FileName_.empty()) {
      this->ConfigStr_ = std::move(cfgstr);
      return std::string{};
   }
   File fd;
   auto fres = fd.Open(this->FileName_, FileMode::Write | FileMode::CreatePath | FileMode::Trunc);
   if (!fres)
      return MakeErr(logHeader, "Open", fres, fd);
   fres = fd.Write(0, &*cfgstr.begin(), cfgstr.size());
   std::string errmsg = CheckRW(logHeader, "Write", fres, cfgstr.size(), fd);
   if (errmsg.empty())
      this->ConfigStr_ = std::move(cfgstr);
   return errmsg;
}

} // namespace

