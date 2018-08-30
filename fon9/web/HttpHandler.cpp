/// \file fon9/web/HttpHandler.cpp
/// \author fonwinz@gmail.com
#include "fon9/web/HttpHandler.hpp"
#include "fon9/web/UrlCodec.hpp"
#include "fon9/web/HttpDate.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/FilePath.hpp"

namespace fon9 { namespace web {

void HttpRequest::RemoveFullMessage() {
   this->Message_.RemoveFullMessage();
   this->ClearFields();
}
void HttpRequest::ClearAll() {
   this->Message_.ClearAll();
   this->ClearFields();
}
void HttpRequest::ClearFields() {
   this->MessageSt_ = HttpMessageSt::Incomplete;
   this->HttpHandler_.reset();
   this->Method_.clear();
   this->TargetOrig_.clear();
}
void HttpRequest::ParseStartLine() {
   assert(this->Message_.IsHeaderReady());
   assert(!this->IsHeaderReady());
   StrView  startLine = this->Message_.StartLine();
   this->Method_.assign(StrFetchNoTrim(startLine, ' '));

   assert(this->TargetOrig_.empty());
   UrlDecodeAppend(this->TargetOrig_, StrFetchNoTrim(startLine, ' '));
   this->TargetRemain_ = ToStrView(this->TargetOrig_);

   this->Version_.assign(startLine);
}

//--------------------------------------------------------------------------//

HttpHandler::~HttpHandler() {
}
io::RecvBufferSize HttpHandler::SendErrorPrefix(io::Device& dev, HttpRequest& req, StrView httpStatus, RevBufferList&& currbuf) {
   if (req.IsMethod("HEAD") || currbuf.cfront() == nullptr) {
      currbuf.MoveOut();
      RevPrint(currbuf, fon9_kCSTR_HTTPCRLN);
   }
   else {
      RevPrint(currbuf,
               "<!DOCTYPE html>" "<html>"
               "<head>"
               "<meta charset='utf-8'/>"
               "<title>fon9:error</title>"
               "<style>.error{color:red;background:lightgray}</style>"
               "</head>");
      size_t sz = CalcDataSize(currbuf.cfront());
      RevPrint(currbuf,
               "Content-Type: text/html; charset=utf-8" fon9_kCSTR_HTTPCRLN
               "Content-Length: ", sz,                  fon9_kCSTR_HTTPCRLN
               fon9_kCSTR_HTTPCRLN);
   }
   RevPrint(currbuf, fon9_kCSTR_HTTP11 " ", httpStatus, fon9_kCSTR_HTTPCRLN
            "Date: ", FmtHttpDate{UtcNow()}, fon9_kCSTR_HTTPCRLN);
   dev.Send(currbuf.MoveOut());
   return io::RecvBufferSize::Default;
}

//--------------------------------------------------------------------------//

HttpDispatcher::~HttpDispatcher() {
}

seed::LayoutSP HttpDispatcher::MakeDefaultLayout() {
   seed::Fields fields = NamedSeed::MakeDefaultFields();
   //fields.Add(fon9_MakeField(Named{"Config"}, HttpHandler, Value_));
   return new seed::Layout1(fon9_MakeField(Named{"Name"}, HttpHandler, Name_),
                            new seed::Tab{Named{"Handler"}, std::move(fields)});
}
seed::TreeSP HttpDispatcher::GetSapling() {
   return this->Sapling_;
}

io::RecvBufferSize HttpDispatcher::OnHttpRequest(io::Device& dev, HttpRequest& req) {
   assert(req.IsHeaderReady());
   req.TargetRemain_ = FilePath::RemovePathHead(req.TargetRemain_);
   req.TargetCurr_   = StrFetchNoTrim(req.TargetRemain_, '/');
   if (auto handler = this->Get(req.TargetCurr_))
      return handler->OnHttpRequest(dev, req);
   return this->OnHttpHandlerNotFound(dev, req);
}

io::RecvBufferSize HttpDispatcher::OnHttpHandlerNotFound(io::Device& dev, HttpRequest& req) {
   RevBufferList rbuf{128};
   if (!req.IsMethod("HEAD")) {
      if (req.TargetCurr_.empty()) {
         RevPrint(rbuf,
                  "<body>"
                  "Handler not support: ",
                  req.TargetOrig_,
                  "</body></html>");
      }
      else {
         const char* pcurr = req.TargetCurr_.begin() - 1;
         RevPrint(rbuf,
                  "<body>"
                  "Handler not found: ",
                  StrView{req.TargetOrig_.begin(), pcurr},
                  "<b class='error'>", StrView{pcurr, req.TargetCurr_.end()}, "</b>",
                  StrView{req.TargetCurr_.end(), req.TargetOrig_.end()},
                  "</body></html>");
      }
   }
   return this->SendErrorPrefix(dev, req, "404 Not found", std::move(rbuf));
}

} } // namespaces
