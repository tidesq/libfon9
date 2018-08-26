/// \file fon9/web/HttpHandlerStatic.cpp
/// \author fonwinz@gmail.com
#include "fon9/web/HttpHandlerStatic.hpp"
#include "fon9/web/HttpDate.hpp"
#include "fon9/ConfigLoader.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 { namespace web {

HttpHandlerStatic::~HttpHandlerStatic() {
}

void HttpHandlerStatic::LoadConfig(const StrView& cfgfn) {
   ConfigLoader cfgld{std::string{}};
   try {
      cfgld.LoadFile(cfgfn);
   }
   catch (ConfigLoader::Err& e) {
      this->SetDescription(e.what());
   }

   if (auto path = cfgld.GetVariable("Path"))
      this->FilePath_ = FilePath::AppendPathTail(&path->Value_.Str_);
   else {
      this->FilePath_ = FilePath::AppendPathTail(ToStrView(FilePath::ExtractPathName(cfgfn)));
   }
   if (auto cacheControl = cfgld.GetVariable("CacheControl")) {
      this->CacheControlCRLN_ = "Cache-Control: " + cacheControl->Value_.Str_ + fon9_kCSTR_HTTPCRLN;
   }
   if (auto contentType = cfgld.GetVariable("ContentType")) {
      StrView str = &contentType->Value_.Str_;
      while (!str.empty()) {
         StrView type = StrFetchTrim(str, '\n');
         StrView fext = StrFetchTrim(type, ':');
         if (StrTrimHead(&type).empty())
            continue;
         while (!fext.empty()) {
            StrView ex = StrFetchTrim(fext, ',');
            if (!ex.empty())
               this->ContentTypeMap_.kfetch(ex).second.assign(type);
         }
      }
   }
}

io::RecvBufferSize HttpHandlerStatic::OnHttpHandlerNotFound(io::Device& dev, HttpRequest& req) {
   StrView reqfname{req.TargetCurr_.begin(), req.TargetRemain_.end()};
   reqfname = StrFetchNoTrim(reqfname, '?');
      
   std::string fname = FilePath::NormalizeFileName(reqfname);
   if (!reqfname.empty())
      return base::OnHttpHandlerNotFound(dev, req);
   if (fname.empty())
      fname = "index.html";
   File fd;
   auto res = fd.Open(this->FilePath_ + fname, FileMode::Read);
   StrView errfn;
   if (!res) {
      errfn = "Open";
__FILE_ERROR:
      RevBufferList rbuf{128};
      RevPrint(rbuf, "<body>"
               "Target: ", req.TargetOrig_, "<br>"
               "File: ", fname, "<br>"
               "Seed: ", this->Name_, "<br>"
               "Error: ", errfn, ':', res,
               "</body></html>");
      return this->SendErrorPrefix(dev, req, "404 Not found", rbuf);
   }
   StrView contentType{"text/html; charset=utf-8"};
   std::string::size_type pos = fname.rfind('.');
   if (pos != std::string::npos) {
      StrView  fext{fname.c_str() + pos + 1, fname.c_str() + fname.size()};
      auto     ifind = this->ContentTypeMap_.find(fext);
      if (ifind != this->ContentTypeMap_.end())
         contentType = ToStrView(ifind->second);
   }

   auto fdLastModifyTime = fd.GetLastModifyTime();
   RevBufferList rbuf{128};
   auto fldIfModifiedSince = req.Message_.FindHeadField("if-modified-since");
   if (fldIfModifiedSince.size() > 0) {
      TimeStamp ifModifiedSince = HttpDateTo(fldIfModifiedSince);
      if (ifModifiedSince.ToEpochSeconds() == fdLastModifyTime.ToEpochSeconds()) {
         RevPrint(rbuf, fon9_kCSTR_HTTP11 " 304 Not Modified" fon9_kCSTR_HTTPCRLN
                  "Date: ", FmtHttpDate{UtcNow()},            fon9_kCSTR_HTTPCRLN
                  "Content-Type: ", contentType,              fon9_kCSTR_HTTPCRLN,
                  this->CacheControlCRLN_,
                  "Last-Modified: ", FmtHttpDate{fdLastModifyTime}, fon9_kCSTR_HTTPCRLN2);
         dev.Send(rbuf.MoveOut());
         return io::RecvBufferSize::Default;
      }
   }

   if (req.IsMethod("HEAD"))
      RevPrint(rbuf, fon9_kCSTR_HTTPCRLN);
   else {
      res = fd.GetFileSize();
      if (!res) {
         errfn = "GetFileSize";
         goto __FILE_ERROR;
      }
      auto  fsz = res.GetResult();
      char* pfbuf = rbuf.AllocPrefix(fsz) - fsz;
      res = fd.Read(0, pfbuf, fsz);
      if (!res) {
         errfn = "Read";
         goto __FILE_ERROR;
      }
      if (res.GetResult() != fsz) {
         errfn = "Read.Size";
         goto __FILE_ERROR;
      }
      rbuf.SetPrefixUsed(pfbuf);
      RevPrint(rbuf, "Content-Length: ", fsz, fon9_kCSTR_HTTPCRLN2);
   }
   RevPrint(rbuf, fon9_kCSTR_HTTP11 " 200 OK" fon9_kCSTR_HTTPCRLN
            "Date: ", FmtHttpDate{UtcNow()},  fon9_kCSTR_HTTPCRLN
            "Content-Type: ", contentType,    fon9_kCSTR_HTTPCRLN,
            this->CacheControlCRLN_,
            "Last-Modified: ", FmtHttpDate{fdLastModifyTime}, fon9_kCSTR_HTTPCRLN);
   dev.Send(rbuf.MoveOut());
   return io::RecvBufferSize::Default;
}

} } // namespaces
