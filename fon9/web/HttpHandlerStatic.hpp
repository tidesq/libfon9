/// \file fon9/web/HttpHandlerStatic.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_web_HttpHandlerStatic_hpp__
#define __fon9_web_HttpHandlerStatic_hpp__
#include "fon9/web/HttpHandler.hpp"
#include "fon9/FilePath.hpp"

namespace fon9 { namespace web {

/// \ingroup web
/// 從檔案系統載入靜態檔案當作回應.
class fon9_API HttpHandlerStatic : public HttpDispatcher {
   fon9_NON_COPY_NON_MOVE(HttpHandlerStatic);
   using base = HttpDispatcher;

   /// 靜態檔案所在路徑.
   std::string FilePath_;
   std::string CacheControlCRLN_;
   using ContentTypeMap = SortedVector<CharVector, CharVector, CharVectorComparer>;
   ContentTypeMap ContentTypeMap_;

   void LoadConfig(const StrView& cfgfn);

protected:
   /// 預設從檔案系統載入.
   io::RecvBufferSize OnHttpHandlerNotFound(io::Device& dev, HttpRequest& req) override;

public:
   template <class... ArgsT>
   HttpHandlerStatic(const StrView& cfgfn, ArgsT&&... args)
      : base{std::forward<ArgsT>(args)...} {
      this->LoadConfig(cfgfn);
   }

   ~HttpHandlerStatic();
};
using HttpHandlerStaticSP = intrusive_ptr<HttpHandlerStatic>;

} } // namespaces
#endif//__fon9_web_HttpHandlerStatic_hpp__
