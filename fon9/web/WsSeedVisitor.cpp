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

struct WsSeedVisitor::PrintLayout : public seed::TicketRunnerTree {
   fon9_NON_COPY_NON_MOVE(PrintLayout);
   using base = seed::TicketRunnerTree;
   using base::base;

   static void NamedToJSON(RevBufferList& rbuf, const Named& named) {
      if (!named.GetDescription().empty()) // ,"description":"named.GetDescription()"
         RevPrint(rbuf, R"(,"description":")", named.GetDescription(), '"');
      if (!named.GetTitle().empty()) // ,"title":"named.GetTitle()"
         RevPrint(rbuf, R"(,"title":")", named.GetTitle(), '"');
      RevPrint(rbuf, R"("name":")", named.Name_, '"');
   }
   static void FieldToJSON(RevBufferList& rbuf, const seed::Field& fld) {
      RevPutChar(rbuf, '}');
      NumOutBuf nbuf;
      if (fld.Flags_ != seed::FieldFlag{})
         RevPrint(rbuf, R"(,"flags":")", fld.Flags_, '"');
      RevPrint(rbuf, R"(,"type":")", fld.GetTypeId(nbuf), '"');
      NamedToJSON(rbuf, fld);
      RevPutChar(rbuf, '{');
   }
   static void LayoutToJSON(RevBufferList& rbuf, seed::Layout& layout) {
      size_t tabIdx = layout.GetTabCount();
      RevPrint(rbuf, "]}");
      while (tabIdx > 0) {
         RevPrint(rbuf, '}');
         seed::Tab& tab = *layout.GetTab(--tabIdx);
         if (seed::Layout* sapling = tab.SaplingLayout_.get()) {
            LayoutToJSON(rbuf, *sapling);
            RevPrint(rbuf, R"(,"sapling":)");
         }
         RevPutChar(rbuf, ']');
         size_t fldIdx = tab.Fields_.size();
         while (fldIdx > 0) {
            FieldToJSON(rbuf, *tab.Fields_.Get(--fldIdx));
            if (fldIdx != 0)
               RevPutChar(rbuf, ',');
         }
         RevPrint(rbuf, R"(,"fields":[)");
         // tab.Flags_;
         NamedToJSON(rbuf, tab);
         RevPrint(rbuf, tabIdx == 0 ? "{" : ",{");
      }
      RevPrint(rbuf, R"(,"tabs":[)");
      FieldToJSON(rbuf, *layout.KeyField_);
      RevPrint(rbuf, R"({"key":)");
   }
   void OnFoundTree(seed::TreeOp& op) override {
      // layout 使用 JSON 格式, 方便 js 處理.
      //    layout:{
      //       "key"        : field,
      //       "tabs"       : [tab]
      //    }
      //    field:{
      //      "name"        : fld->Name_,
      //      "title"       : fld->GetTitle(),
      //      "description" : fld->GetDescription(),
      //      "type"        : fld->GetTypeId(),
      //      "flags"       : fld->Flags_,
      //    }
      //    tab:{
      //      "name"        : tab->Name_,
      //      "title"       : tab->GetTitle(),
      //      "description" : tab->GetDescription(),
      //      "flags"       : ,
      //      "fields"      : [field],
      //      "sapling"     : tab->SaplingLayout_,
      //    }
      RevBufferList rbuf{128};
      LayoutToJSON(rbuf, *op.Tree_.LayoutSP_);
      this->Visitor_->OnTicketRunnerDone(*this, DcQueueList{rbuf.MoveOut()});

      // op.GridView();
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
   if (!req.Runner_) {
      if(req.Command_ == "pl")
         req.Runner_ = new PrintLayout(*this->Visitor_, req.SeedName_);
   }
   if (req.Runner_) {
      req.Runner_->Bookmark_.assign(req.Command_.begin(), req.CommandArgs_.end());
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
