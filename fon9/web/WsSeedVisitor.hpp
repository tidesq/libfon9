// \file fon9/web/WsSeedVisitor.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_web_WsSeedVisitor_hpp__
#define __fon9_web_WsSeedVisitor_hpp__
#include "fon9/web/WebSocketAuther.hpp"
#include "fon9/seed/SeedVisitor.hpp"

namespace fon9 { namespace web {

class fon9_API WsSeedVisitor : public WebSocket {
   fon9_NON_COPY_NON_MOVE(WsSeedVisitor);
   using base = WebSocket;
   struct SeedVisitor;
   struct PrintLayout;
   using SeedVisitorSP = intrusive_ptr<SeedVisitor>;
   io::RecvBufferSize OnWebSocketMessage() override;
   static void EmitOnTimer(TimerEntry* timer, TimeStamp now);
   using Timer = DataMemberEmitOnTimer<&WsSeedVisitor::EmitOnTimer>;
   Timer                   HbTimer_;
   const SeedVisitorSP     Visitor_;
   const auth::AuthResult  Authr_;
public:
   WsSeedVisitor(io::DeviceSP dev, seed::MaTreeSP root, const auth::AuthResult& authResult, seed::AclConfig&& aclcfg);
   ~WsSeedVisitor();
};

class fon9_API WsSeedVisitorCreator : public HttpWebSocketAuthHandler {
   fon9_NON_COPY_NON_MOVE(WsSeedVisitorCreator);
   using base = HttpWebSocketAuthHandler;

   WebSocketSP CreateWebSocketService(io::Device& dev, auth::AuthResult& authResult) override;
public:
   const seed::MaTreeSP Root_;

   template <class... ArgsT>
   WsSeedVisitorCreator(seed::MaTreeSP root, auth::AuthMgrSP authMgr, ArgsT&&... args)
      : base(std::move(authMgr), std::forward<ArgsT>(args)...)
      , Root_(std::move(root)) {
   }

   ~WsSeedVisitorCreator();
};

} } // namespaces
#endif//__fon9_web_WsSeedVisitor_hpp__
