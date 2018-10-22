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
      if (auto ws = this->GetWsSeedVisitor()) {
         RevBufferList rbuf{128, std::move(extmsg)};
         RevPrint(rbuf, runner.Bookmark_, runner.Path_, '\n');//first list: [seedName]
         if (runner.OpResult_ < seed::OpResult::no_error)
            RevPrint(rbuf, "e=", runner.OpResult_, ':', seed::GetOpResultMessage(runner.OpResult_), '\n');
         ws->Send(WebSocketOpCode::TextFrame, std::move(rbuf));
      }
   }

   void OnTicketRunnerWrite(seed::TicketRunnerWrite& runner, const seed::SeedOpResult& res, const seed::RawWr& wr) override {
      RevBufferList rbuf{runner.ParseSetValues(res, wr)};
      RevPutChar(rbuf, '\n');
      this->OnTicketRunnerOutputSeed(runner, res, wr, std::move(rbuf));
   }
   void OnTicketRunnerRead(seed::TicketRunnerRead& runner, const seed::SeedOpResult& res, const seed::RawRd& rd) override {
      this->OnTicketRunnerOutputSeed(runner, res, rd, RevBufferList{128});
   }
   void OnTicketRunnerOutputSeed(seed::TicketRunner& runner, const seed::SeedOpResult& res, const seed::RawRd& rd, RevBufferList&& rbuf) {
      size_t ifld = res.Tab_->Fields_.size();
      while (auto fld = res.Tab_->Fields_.Get(--ifld)) {
         fld->CellRevPrint(rd, nullptr, rbuf);
         if (ifld > 0)
            RevPutChar(rbuf, seed::GridViewResult::kCellSplitter);
      }
      this->OnTicketRunnerDone(runner, DcQueueList{rbuf.MoveOut()});
   }

   void OnTicketRunnerRemoved(seed::TicketRunnerRemove& runner, const seed::PodRemoveResult& res) override {
      (void)res; assert(runner.OpResult_ == res.OpResult_);
      this->OnTicketRunnerDone(runner, DcQueueFixedMem{});
   }
   void OnTicketRunnerBeforeGridView(seed::TicketRunnerGridView& runner, seed::TreeOp& opTree, seed::GridViewRequest& req) override {
      if (!seed::IsTextBegin(req.OrigKey_))
         return;
      auto subr = this->NewSubscribe();
      if (subr->Subscribe(ToStrView(runner.OrigPath_), *req.Tab_, opTree) != seed::OpResult::no_error)
         this->Unsubscribe();
   }
   void OnTicketRunnerSubscribe(seed::TicketRunnerSubscribe&, bool isSubOrUnsub) override {
      (void)isSubOrUnsub;
      // 不支援使用 TicketRunnerSubscribe 訂閱.
      // 在 OnTicketRunnerBeforeGridView() 處理訂閱.
      // 並在 OnTicketRunnerGridView() 告知訂閱結果.
   }
   void OnSeedNotify(seed::VisitorSubr& subr, const seed::SeedNotifyArgs& args) override {
      if (auto ws = this->GetWsSeedVisitor()) {
         RevBufferList rbuf{128};
         const char*   cmdEcho;
         switch (args.NotifyType_) {
         case fon9::seed::SeedNotifyArgs::NotifyType::ParentSeedClear:
            return;
         case fon9::seed::SeedNotifyArgs::NotifyType::PodRemoved:
            RevPrint(rbuf, '\n');
            cmdEcho = ">rs ";
            goto __REVPRINT_PATH_WITH_KEY_TEXT;
         case fon9::seed::SeedNotifyArgs::NotifyType::SeedChanged:
            RevPrint(rbuf, args.GetGridView());
            // 不用 break; 繼續填入 ">ss path\n"
         case fon9::seed::SeedNotifyArgs::NotifyType::SeedRemoved:
            cmdEcho = ">ss ";
            RevPrint(rbuf, '\n');
            if (args.Tab_->GetIndex() != 0)
               RevPrint(rbuf, '^', args.Tab_->Name_);
__REVPRINT_PATH_WITH_KEY_TEXT:
            RevPrint(rbuf, cmdEcho, subr.GetPath(), '/', args.KeyText_);
            break;
         case fon9::seed::SeedNotifyArgs::NotifyType::TableChanged:
            RevPrint(rbuf, ' ', subr.GetPath(), "\n" ",0,1,SubrOK" "\n", args.GetGridView());
            if (args.Tab_->GetIndex() != 0)
               RevPrint(rbuf, ",,^", args.Tab_->Name_);
            RevPrint(rbuf, ">gv");
            break;
         }
         ws->Send(WebSocketOpCode::TextFrame, std::move(rbuf));
      }
   }
   void OnTicketRunnerGridView(seed::TicketRunnerGridView& runner, seed::GridViewResult& res) override {
      struct Output {
         static inline void gvSize(RevBufferList& rbuf, size_t n, char chSpl) {
            RevPutChar(rbuf, chSpl);
            if (n != seed::GridViewResult::kNotSupported)
               RevPrint(rbuf, n);
         }
      };
      RevBufferList rbuf{128};
      if (!res.GridView_.empty())
         RevPrint(rbuf, res.kRowSplitter, res.GridView_);
      auto subr = this->GetSubr();
      if (subr && subr->GetTab() == res.Tab_ && subr->GetTree() == res.Sender_)
         RevPrint(rbuf, "SubrOK");
      Output::gvSize(rbuf, res.DistanceEnd_, ',');
      Output::gvSize(rbuf, res.DistanceBegin_, ',');
      Output::gvSize(rbuf, res.ContainerSize_, ',');
      this->OnTicketRunnerDone(runner, DcQueueList{rbuf.MoveOut()});
   }
   void OnTicketRunnerCommand(seed::TicketRunnerCommand& runner, const seed::SeedOpResult& res, StrView msg) override {
      (void)res; assert(runner.OpResult_ == res.OpResult_);
      this->OnTicketRunnerDone(runner, DcQueueFixedMem{msg});
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
   void LayoutToJSON(RevBufferList& rbuf, seed::Layout& layout) {
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
         if (tab.Flags_ != seed::TabFlag{})
            RevPrint(rbuf, R"(,"flags":")", tab.Flags_, '"');
         NamedToJSON(rbuf, tab);
         RevPrint(rbuf, tabIdx == 0 ? "{" : ",{");
      }
      RevPrint(rbuf, R"(,"tabs":[)");
      if (layout.Flags_ != seed::TreeFlag{})
         RevPrint(rbuf, R"(,"flags":")", layout.Flags_, '"');
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
      //      "flags"       : tab->Flags_,
      //      "fields"      : [field],
      //      "sapling"     : tab->SaplingLayout_,
      //    }
      RevBufferList rbuf{128};
      LayoutToJSON(rbuf, *op.Tree_.LayoutSP_);
      // - 輸出內容:
      //   accessRights\n
      //   layout_JSON
      RevPrint(rbuf, this->Rights_, '\n'); // TODO: 是否可能針對不同 tab 設定不同的權限?
      this->Visitor_->OnTicketRunnerDone(*this, DcQueueList{rbuf.MoveOut()});
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
   this->Visitor_->Unsubscribe();
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
      size_t cmdsz = static_cast<size_t>(req.CommandArgs_.end() - req.Command_.begin());
      char*  bmbuf = static_cast<char*>(req.Runner_->Bookmark_.alloc(cmdsz + 2));
      char   chSpl;
      if (fon9_UNLIKELY(dynamic_cast<seed::TicketRunnerCommand*>(req.Runner_.get()))) {
         // SeedCommand 回覆: fon9_kCSTR_CELLSPL + "command" + fon9_kCSTR_CELLSPL + "seedName"
         // 因為 command 可能會有 '/', ' ' 之類的字元, 所以用 fon9_kCSTR_CELLSPL 分隔比較保險.
         // 因為 command 指令可能為任意字串(無控制字元), 無法保證不跟一般指令(pl, gv...)重複,
         // 所以使用 fon9_kCSTR_CELLSPL 開頭當作雨衣般指令的區別.
         *bmbuf = chSpl = *fon9_kCSTR_CELLSPL;
      }
      else {
         // 一般指令(pl, gv...) 回覆: ">command,args" + " " + "path"
         *bmbuf = '>';
         chSpl = ' ';
      }
      memcpy(bmbuf + 1, req.Command_.begin(), cmdsz);
      bmbuf[cmdsz + 1] = chSpl;
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
