/// \file fon9/web/HttpPlugins.cpp
/// \author fonwinz@gmail.com
#include "fon9/web/HttpHandlerStatic.hpp"
#include "fon9/web/HttpSession.hpp"
#include "fon9/web/WsSeedVisitor.hpp"
#include "fon9/seed/Plugins.hpp"
#include "fon9/framework/IoFactory.hpp"

namespace fon9 { namespace web {

static bool MakeHttpStaticDispatcher(seed::PluginsHolder& holder, StrView args) {
   // plugins args: "Name=HttpRoot|Cfg=wwwroot/HttpStatic.cfg"
   std::string name, errmsg;
   while (!args.empty()) {
      StrView fld = SbrFetchNoTrim(args, '|');
      StrView tag = StrFetchTrim(fld, '=');
      StrTrim(&fld);
      if (tag == "Name")
         name = fld.ToString();
      else if (tag == "Config" || tag == "Cfg") {
         if (!holder.Root_->Add(new HttpHandlerStatic{fld, name}))
            errmsg += "|err=Add HttpStaticDispatcher fail: 'Name=" + name + "'";
      }
   }
   if (!errmsg.empty())
      holder.SetPluginsSt(LogLevel::Error, errmsg);
   return true;
}

static bool MakeHttpSession(seed::PluginsHolder& holder, StrView args) {
   // plugins args: "Name=WsSeedVisitor|HttpDispatcher=HttpRoot|AddTo=FpSession"
   // 參數: Name, HttpDispatcher 必須在 AddTo 之前先提供!
   struct ArgsParser : public SessionFactoryConfigParser {
      using base = SessionFactoryConfigParser;
      HttpHandlerSP HttpDispatcher_;
      ArgsParser() : base{"HttpSession"} {
      }
      bool OnUnknownTag(seed::PluginsHolder& holder, StrView tag, StrView value) override {
         if (tag == "HttpDispatcher") {
            this->HttpDispatcher_ = holder.Root_->Get<HttpHandler>(value);
            return true;
         }
         return base::OnUnknownTag(holder, tag, value);
      }
      SessionFactorySP CreateSessionFactory() override {
         if (this->HttpDispatcher_)
            return web::HttpSession::MakeFactory(this->Name_, this->HttpDispatcher_);
         this->ErrMsg_ += "|err=Unknown HttpDispatcher";
         return nullptr;
      }
   };
   return ArgsParser{}.Parse(holder, args);
}

static bool MakeWsSeedVisitor(seed::PluginsHolder& holder, StrView args) {
   // plugins args: "Name=WsSeedVisitor|AuthMgr=MaAuth|HttpDispatcher=HttpRoot"
   std::string       name, errmsg;
   HttpDispatcherSP  httpDispatcher;
   auth::AuthMgrSP   authMgr;
   while (!args.empty()) {
      StrView fld = SbrFetchNoTrim(args, '|');
      StrView tag = StrFetchTrim(fld, '=');
      StrTrim(&fld);
      if (tag == "Name")
         name = fld.ToString();
      else if (tag == "AuthMgr") {
         if (!(authMgr = holder.Root_->Get<auth::AuthMgr>(fld)))
            errmsg += "|err=Not found AuthMgr: 'AuthMgr=" + fld.ToString() + "'";
      }
      else if (tag == "HttpDispatcher" || tag == "AddTo") {
         if (!(httpDispatcher = holder.Root_->Get<HttpDispatcher>(fld)))
            errmsg += "|err=Not found dispatcher: '" + tag.ToString() + "=" + fld.ToString() + "'";
      }
      else
         errmsg += "|err=Unknown tag: '" + tag.ToString() + "=" + fld.ToString() + "'";
   }
   if (errmsg.empty()) {
      if (httpDispatcher && authMgr)
         httpDispatcher->Add(new web::WsSeedVisitorCreator(holder.Root_, authMgr, name));
      else {
         if (!httpDispatcher)
            errmsg += "|err=Not found 'HttpDispatcher='";
         if (!authMgr)
            errmsg += "|err=Not found 'AuthMgr='";
      }
   }
   if (!errmsg.empty())
      holder.SetPluginsSt(LogLevel::Error, errmsg);
   return true;
}

} } // namespaces

extern "C" fon9_API fon9::seed::PluginsDesc f9p_HttpStaticDispatcher;
fon9::seed::PluginsDesc f9p_HttpStaticDispatcher{ "", &fon9::web::MakeHttpStaticDispatcher, nullptr, nullptr, };

extern "C" fon9_API fon9::seed::PluginsDesc f9p_HttpSession;
fon9::seed::PluginsDesc f9p_HttpSession{"", &fon9::web::MakeHttpSession, nullptr, nullptr,};

extern "C" fon9_API fon9::seed::PluginsDesc f9p_WsSeedVisitor;
fon9::seed::PluginsDesc f9p_WsSeedVisitor{"", &fon9::web::MakeWsSeedVisitor, nullptr, nullptr,};

static fon9::seed::PluginsPark f9pRegister{
   "HttpStaticDispatcher", &f9p_HttpStaticDispatcher,
   "HttpSession", &f9p_HttpSession,
   "WsSeedVisitor", &f9p_WsSeedVisitor};
