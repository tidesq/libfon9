/// \file fon9/web/HttpHandler.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_web_HttpHandler_hpp__
#define __fon9_web_HttpHandler_hpp__
#include "fon9/seed/MaTree.hpp"
#include "fon9/web/HttpMessage.hpp"
#include "fon9/io/Device.hpp"

namespace fon9 { namespace web {

class fon9_API HttpHandler;
using HttpHandlerSP = seed::NamedSeedSPT<HttpHandler>;

enum class HttpMessageSt {
   Incomplete = 0,
   FullMessage = 1,
   ChunkAppended = 2,
};

fon9_WARN_DISABLE_PADDING;
struct fon9_API HttpRequest {
   HttpMessage    Message_;
   HttpMessageSt  MessageSt_{};
   HttpHandlerSP  HttpHandler_;
   CharVector     Version_;
   CharVector     Method_;
   CharVector     TargetOrig_;
   StrView        TargetRemain_;
   StrView        TargetCurr_;

   void RemoveFullMessage();
   void ClearAll();
   void ParseStartLine();
   bool IsHeaderReady() const {
      return !this->Method_.empty();
   }
   bool IsMethod(StrView method) const {
      return method == ToStrView(this->Method_);
   }

private:
   void ClearFields();
};
fon9_WARN_POP;

/// \ingroup web
/// 一個提供 Web 服務的「起點」.
class fon9_API HttpHandler : public seed::NamedSeed {
   fon9_NON_COPY_NON_MOVE(HttpHandler);
   using base = NamedSeed;

public:
   using base::base;

   ~HttpHandler();
   
   /// 解析 req.Message_ 的
   /// - HeaderReady 事件, 只會觸發一次.
   ///   - HttpMessageSt::Incomplete 則 assert(req.Message_.IsHeaderReady());
   ///   - 此時應尋找並設定 req.HttpHandler_, 如果確定沒有可用的 HttpHandler, 則應拒絕此 req.
   /// - 有可能直接提供 HttpMessageSt::FullMessage, 而沒有先提供 HeaderReady 事件.
   /// - 若將 dev.Session_ 升級為 WebSocket, 因升級後 HttpMessageReceiver 會死亡,
   ///   所以應返回 io::RecvBufferSize::NoLink 告知中斷解析, 否則會 crash!
   virtual io::RecvBufferSize OnHttpRequest(io::Device& dev, HttpRequest& req) = 0;

   /// - 若 currbuf.cfront() != nullptr
   ///   - 則在 currbuf 前端, 增加底下訊息, 並加入 http header 之後, 送出:
   ///     "<!DOCTYPE html>" "<html>"
   ///     "<head>" "<meta charset='utf-8'/>" "<title>fon9:error</title>"
   ///     "<style>.error{color:red;background:lightgray}</style>"
   ///     "</head>"
   ///   - 所以您的 currbuf, 尾端必須有 "</html>"
   /// - httpStatus 範例:
   ///   - "404 Not found"
   ///   - "400 Bad Request"
   static io::RecvBufferSize SendErrorPrefix(io::Device& dev, HttpRequest& req,
                                             StrView httpStatus, RevBufferList&& currbuf);
};

/// \ingroup web
/// 根據加入的 HttpHandler 來分派 HttpRequest.
class fon9_API HttpDispatcher : public HttpHandler {
   fon9_NON_COPY_NON_MOVE(HttpDispatcher);
   using base = HttpHandler;

protected:
   using DispatcherTree = seed::MaTree;
   static seed::LayoutSP MakeDefaultLayout();

   const seed::TreeSPT<DispatcherTree> Sapling_;

   /// 預設回覆 404 Not found
   virtual io::RecvBufferSize OnHttpHandlerNotFound(io::Device& dev, HttpRequest& req);

public:
   template <class... ArgsT>
   HttpDispatcher(ArgsT&&... args)
      : base{std::forward<ArgsT>(args)...}
      , Sapling_{new DispatcherTree{MakeDefaultLayout()}} {
   }
   ~HttpDispatcher();

   seed::TreeSP GetSapling() override;
   bool Add(HttpHandlerSP item) {
      return static_cast<DispatcherTree*>(this->Sapling_.get())->Add(item);
   }
   HttpHandlerSP Get(const StrView& name) const {
      return static_cast<DispatcherTree*>(this->Sapling_.get())->Get<HttpHandler>(name);
   }

   /// 根據 HttpRequest.TargetRemain_ 來尋找適當的 HttpHandler
   io::RecvBufferSize OnHttpRequest(io::Device& dev, HttpRequest& req) override;
};
using HttpDispatcherSP = intrusive_ptr<HttpDispatcher>;

} } // namespaces
#endif//__fon9_web_HttpHandler_hpp__
