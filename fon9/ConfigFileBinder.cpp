// \file fon9/ConfigFileBinder.cpp
// \author fonwinz@gmail.com
#include "fon9/ConfigFileBinder.hpp"
#include "fon9/File.hpp"
#include "fon9/FilePath.hpp"
#include "fon9/Log.hpp"

namespace fon9 {

static std::string MakeErr(StrView logErrHeader, StrView func, File::Result fres, File& fd) {
   std::string errstr = RevPrintTo<std::string>("func=", func, '|', fres, "|file=", fd.GetOpenName());
   if (logErrHeader.begin() != nullptr)
      fon9_LOG_ERROR(logErrHeader, '|', errstr);
   return errstr;
}
static std::string CheckRW(StrView logErrHeader, StrView func, File::Result fres, File::SizeType expected, File& fd) {
   if (!fres)
      return MakeErr(logErrHeader, func, fres, fd);
   if (fres.GetResult() == expected)
      return std::string{};
   std::string errstr = RevPrintTo<std::string>("func=", func,
                                                "|err.size=", fres.GetResult(),
                                                "|expected=", expected,
                                                "|file=", fd.GetOpenName());
   if (logErrHeader.begin() != nullptr)
      fon9_LOG_ERROR(logErrHeader, '|', errstr);
   return errstr;
}

std::string ConfigFileBinder::OpenRead(StrView logErrHeader, std::string cfgfn) {
   this->FileName_ = cfgfn;
   File fd;
   auto fres = fd.Open(std::move(cfgfn), FileMode::Read);
   if (!fres)
      return MakeErr(logErrHeader, "Open", fres, fd);
   fres = fd.GetFileSize();
   if (!fres)
      return MakeErr(logErrHeader, "GetFileSize", fres, fd);
   this->ConfigStr_.resize(fres.GetResult());
   if (this->ConfigStr_.size() <= 0)
      return std::string{};
   fres = fd.Read(0, &*this->ConfigStr_.begin(), this->ConfigStr_.size());
   this->LastModifyTime_ = fd.GetLastModifyTime();
   return CheckRW(logErrHeader, "Read", fres, this->ConfigStr_.size(), fd);
}
std::string ConfigFileBinder::Write(StrView logErrHeader, std::string cfgstr) {
   if (this->ConfigStr_ == cfgstr)
      return std::string{};
   if (this->FileName_.empty()) {
      this->ConfigStr_ = std::move(cfgstr);
      return std::string{};
   }
   File fd;
   auto fres = fd.Open(this->FileName_, FileMode::Write | FileMode::CreatePath | FileMode::Trunc);
   if (!fres)
      return MakeErr(logErrHeader, "Open", fres, fd);
   std::string errmsg = WriteConfig(logErrHeader, cfgstr, fd);
   if (errmsg.empty()) {
      this->ConfigStr_ = std::move(cfgstr);
      this->LastModifyTime_ = UtcNow();
   }
   return errmsg;
}

fon9_API std::string WriteConfig(StrView logErrHeader, const std::string& cfgstr, File& fd) {
   auto fres = fd.Write(0, &*cfgstr.begin(), cfgstr.size());
   return CheckRW(logErrHeader, "Write", fres, cfgstr.size(), fd);
}

fon9_API void BackupConfig(StrView logErrHeader, StrView cfgFileName, TimeStamp utctm, const std::string& ctx) {
   std::string fname = FilePath::GetFullName(cfgFileName);
   std::string pname = FilePath::ExtractPathName(&fname);
   fname.insert(pname.size(), "/cfgbak");
   fname.push_back('.');
   char  tmstrbuf[kDateTimeStrWidth + 1];
   char* pbufend = tmstrbuf + sizeof(tmstrbuf);
   fname.append(ToStrRev_Date_Time_us(pbufend, utctm + GetLocalTimeZoneOffset()), pbufend);

   File fd;
   auto fres = fd.Open(fname, FileMode::Write | FileMode::CreatePath | FileMode::MustNew);
   if (!fres) {
      if (fres.GetError() != ErrC{std::errc::file_exists})
         MakeErr(logErrHeader, "BackupConfig", fres, fd);
      return;
   }
   std::string errmsg = WriteConfig(logErrHeader, ctx, fd);
   if (!errmsg.empty()) // 寫入失敗: 刪除此檔.
      remove(fname.c_str());
}

} // namespace

