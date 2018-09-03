// \file fon9/web/WsSeedVisitor.cpp
// \author fonwinz@gmail.com
#include "fon9/web/WsSeedVisitor.hpp"
#include "fon9/auth/PolicyAcl.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 { namespace web {

enum {
   kWsSeedVisitor_HbIntervalSecs = 30
};

WsSeedVisitorCreator::~WsSeedVisitorCreator() {
}
web::WebSocketSP WsSeedVisitorCreator::CreateWebSocketService(io::Device& dev, auth::AuthResult& authResult) {
   seed::AclConfig aclcfg;
   if (!this->AuthMgr_->GetPolicy<auth::PolicyAclAgent>(fon9_kCSTR_PolicyAclAgent_Name, authResult, aclcfg))
      authResult.ExtInfo_ = "AuthUser.GetPolicy." fon9_kCSTR_PolicyAclAgent_Name "|err=Not found.";
   else if (aclcfg.IsAccessDeny())
      authResult.ExtInfo_ = "SeedSession.Run|err=Acl is empty.";
   else
      return web::WebSocketSP{new WsSeedVisitor(&dev, this->Root_, authResult, std::move(aclcfg))};
   return WebSocketSP{nullptr};
}

//--------------------------------------------------------------------------//

struct WsSeedVisitor::SeedVisitor : public seed::SeedVisitor {
   fon9_NON_COPY_NON_MOVE(SeedVisitor);
   using base = seed::SeedVisitor;
   const io::DeviceSP   Device_;
   SeedVisitor(io::DeviceSP dev, seed::MaTreeSP root, seed::AclConfig&& aclcfg)
      : base{std::move(root)}
      , Device_{std::move(dev)} {
      this->Fairy_->ResetAclConfig(std::move(aclcfg));
   }
   WsSeedVisitor* GetWsSeedVisitor() const {
      return dynamic_cast<WsSeedVisitor*>(static_cast<HttpSession*>(this->Device_->Session_.get())->GetRecvHandler());
   }
   void OnTicketRunnerDone(seed::TicketRunner& runner, DcQueue&& extmsg) override {
      if (auto ws = GetWsSeedVisitor()) {
         RevBufferList rbuf{128, std::move(extmsg)};
         RevPrint(rbuf, runner.Bookmark_, ' ', runner.Path_, '\n');//first list: [seedName]
         if (runner.OpResult_ < seed::OpResult::no_error)
            RevPrint(rbuf, "e=", runner.OpResult_, ':', seed::GetOpResultMessage(runner.OpResult_), '\n');
         else
            RevPrint(rbuf, '>');
         ws->Send(WebSocketOpCode::TextFrame, std::move(rbuf));
      }
   }
   void OnTicketRunnerWrite(seed::TicketRunnerWrite&, const seed::SeedOpResult& res, const seed::RawWr& wr) override {
   }
   void OnTicketRunnerRead(seed::TicketRunnerRead&, const seed::SeedOpResult& res, const seed::RawRd& rd) override {
   }
   void OnTicketRunnerRemoved(seed::TicketRunnerRemove&, const seed::PodRemoveResult& res) override {
   }
   void OnTicketRunnerGridView(seed::TicketRunnerGridView& runner, seed::GridViewResult& res) override {
      this->OnTicketRunnerDone(runner, DcQueueFixedMem{res.GridView_});
   }
   void OnTicketRunnerCommand(seed::TicketRunnerCommand&, const seed::SeedOpResult& res, StrView msg) override {
   }
   void OnTicketRunnerSetCurrPath(seed::TicketRunnerCommand& runner) override {
      this->OnTicketRunnerDone(runner, DcQueueFixedMem{});
   }
};

//--------------------------------------------------------------------------//

WsSeedVisitor::WsSeedVisitor(io::DeviceSP dev, seed::MaTreeSP root, const auth::AuthResult& authResult, seed::AclConfig&& aclcfg)
   : base{dev}
   , Visitor_{new SeedVisitor(std::move(dev), std::move(root), std::move(aclcfg))}
   , Authr_{authResult} {
   this->HbTimer_.RunAfter(TimeInterval_Second(kWsSeedVisitor_HbIntervalSecs));
}
WsSeedVisitor::~WsSeedVisitor() {
   this->HbTimer_.StopAndWait();
}
void WsSeedVisitor::EmitOnTimer(TimerEntry* timer, TimeStamp now) {
   (void)now;
   timer->RunAfter(TimeInterval_Second(kWsSeedVisitor_HbIntervalSecs));
   WsSeedVisitor& rthis = ContainerOf(*static_cast<decltype(WsSeedVisitor::HbTimer_)*>(timer), &WsSeedVisitor::HbTimer_);
   rthis.Send(web::WebSocketOpCode::Ping, nullptr);
}

io::RecvBufferSize WsSeedVisitor::OnWebSocketMessage() {
   seed::SeedFairy::Request req(*this->Visitor_, &this->Payload_);

   struct PrintLayout : public seed::TicketRunnerTree {
      fon9_NON_COPY_NON_MOVE(PrintLayout);
      using base = seed::TicketRunnerTree;
      using base::base;
      void OnFoundTree(seed::TreeOp& op) override {
         // layout 使用 JSON 格式, 方便 js 處理.
         //    field:{
         //      "name"        : fld->Name_,
         //      "title"       : fld->GetTitle(),
         //      "description" : fld->GetDescription(),
         //      "type"        : fld->GetTypeId(),
         //      "readonly"    : true/false
         //    }
         //    tab:{
         //      "name"        : tab->Name_,
         //      "title"       : tab->GetTitle(),
         //      "description" : tab->GetDescription(),
         //      "readonly"    : true/false,
         //      "needsApply"  : true/false,
         //      "fields"      : [field],
         //      "sapling"     : tab->SaplingLayout_,
         //    }
         //    layout:
         //       "key"        : field,
         //       "tabs"       : [tab]
         //    }
         this->Visitor_->OnTicketRunnerDone(*this, DcQueueFixedMem{""});
      }
   };

   if (!req.Runner_) {
      if(req.Command_ == "pl")
         req.Runner_ = new PrintLayout(*this->Visitor_, req.SeedName_);
   }
   if (req.Runner_) {
      req.Runner_->Bookmark_.assign(req.Command_);
      req.Runner_->Run();
   }
   else {
      RevBufferList rbuf{32};
      RevPrint(rbuf, "e=Unknown command: ", this->Payload_);
      this->Send(web::WebSocketOpCode::TextFrame, std::move(rbuf));
   }
   return io::RecvBufferSize::Default;
}

} } // namespaces
