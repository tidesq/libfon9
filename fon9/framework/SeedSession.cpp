// \file fon9/framework/SeedSession.cpp
// \author fonwinz@gmail.com
#include "fon9/framework/SeedSession.hpp"
#include "fon9/auth/SaslClient.hpp"
#include "fon9/auth/PolicyAcl.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 {

SeedSession::SeedSession(seed::MaTreeSP root, auth::AuthMgrSP authMgr, bool isAdminMode)
   : base{std::move(root)}
   , Authr_{std::move(authMgr)} {
   if (isAdminMode)
      this->SetAdminMode();
}
SeedSession::~SeedSession() {
}
void SeedSession::SetAdminMode() {
   St::Locker st{this->St_};
   assert(st->State_ == State::None);
   st->State_ = State::UserReady;
   this->Authr_.AuthcId_.assign("[AdminMode]");
   seed::AclConfig aclcfg;
   aclcfg.SetAdminMode();
   this->Fairy_->ResetAclConfig(std::move(aclcfg));
}
void SeedSession::SetCurrPath(StrView currPath) {
   base::SetCurrPath(currPath);
   St::Locker st{this->St_};
   RevBufferList rbuf{static_cast<BufferNodeSize>(currPath.size() + 64)};
   RevPrint(rbuf, this->Authr_.AuthcId_, "] ", currPath, '>');
   if (!this->Authr_.AuthzId_.empty())
      RevPrint(rbuf, this->Authr_.AuthzId_, '/');
   RevPrint(rbuf, '[');
   st->Prompt_ = BufferTo<std::string>(rbuf.MoveOut());
}

//--------------------------------------------------------------------------//

void SeedSession::ClearLogout(St::Locker& st) {
   this->Authr_.Clear();
   this->Fairy_->Clear();
   this->LastGV_.reset();
   st->Prompt_.clear();
}
SeedSession::State SeedSession::Logout() {
   St::Locker st{this->St_};
   if (!IsRunningState(st->State_)) {
      if (st->State_ == State::Authing || st->State_ == State::Logouting || st->CurrRequest_) {
         // 等認證完畢後, or 最後一筆要求結束後, 再處理登出事件.
         return st->State_ = State::Logouting;
      }
   }
   this->OnAuthEventInLocking(st->State_ = State::UserExit, DcQueueFixedMem{});
   this->ClearLogout(st);
   return st->State_;
}
void SeedSession::EmitAuthEvent(State newst, DcQueue&& msg) {
   St::Locker st{this->St_};
   assert(!st->CurrRequest_);
   st->PendingCommandLines_.clear();
   if (newst == State::UserReady && st->State_ == State::Logouting)
      this->OnAuthEventInLocking(st->State_ = State::UserExit, DcQueueFixedMem{});
   else
      this->OnAuthEventInLocking(st->State_ = newst, std::move(msg));
   if (st->State_ != State::UserReady)
      this->ClearLogout(st);
}
void SeedSession::OnAuthDone(auth::AuthR&& authr) {
   State   st = State::UserReady;
   StrView msg{&authr.Info_};
   if (authr.RCode_ != fon9_Auth_Success)
      st = State::AuthError;
   else {
      seed::AclConfig aclcfg;
      if (!this->Authr_.AuthMgr_->GetPolicy<auth::PolicyAclAgent>(fon9_kCSTR_PolicyAclAgent_Name, this->Authr_, aclcfg)) {
         st = State::UserExit;
         msg = StrView{"AuthUser.GetPolicy." fon9_kCSTR_PolicyAclAgent_Name "|err=Not found."};
      }
      else if (aclcfg.IsAccessDeny()) {
         st = State::UserExit;
         msg = StrView{"SeedSession.Run|err=Acl is empty."};
      }
      else {
         this->Fairy_->ResetAclConfig(std::move(aclcfg));
      }
   }
   if (msg.empty())
      msg = &this->Authr_.ExtInfo_;
   this->EmitAuthEvent(st, DcQueueFixedMem{msg});
}
SeedSession::State SeedSession::AuthUser(StrView authz, StrView authc, StrView pass, StrView from) {
   {
      St::Locker st{this->St_};
      if (IsRunningState(st->State_))
         return st->State_ == State::Authing ? State::None : st->State_;
      st->State_ = State::Authing;
   }
   RevBufferList  rbuf{128};
   // 也許固定使用 sasl mech: "SCRAM-SHA-256"?
   // 1.建立 SASL mech list.
   const char  kMechSplitterChar = ' ';
   std::string strSaslMechList = this->Authr_.AuthMgr_->GetSaslMechList(kMechSplitterChar);
   // 2.建立 Client 端: 認證協商處理物件.
   fon9_WARN_DISABLE_PADDING;
   struct AuthRequest : public intrusive_ref_counter<AuthRequest> {
      fon9_NON_COPY_NON_MOVE(AuthRequest);
      auth::AuthSessionSP  AuthServer_;
      auth::SaslClientR    AuthClient_;
      auth::AuthRequest    Request_;
      const SeedSessionSP  Visitor_;
      AuthRequest(SeedSession* owner, auth::SaslClientR&& cli)
         : AuthClient_{std::move(cli)}
         , Visitor_{owner} {
      }
   };
   fon9_WARN_POP;
   struct AuthRequestSP : public intrusive_ptr<AuthRequest> {
      using base = intrusive_ptr<AuthRequest>;
      using base::base;
      void operator()(auth::AuthR authr, auth::AuthSessionSP auths) {
         AuthRequest* req = this->get();
         if (authr.RCode_ == fon9_Auth_NeedsMore || authr.RCode_ == fon9_Auth_Success) {
            authr = req->AuthClient_.SaslClient_->OnChallenge(&authr.Info_);
            if (authr.RCode_ == fon9_Auth_NeedsMore) {
               req->Request_.Response_ = authr.Info_;
               auths->AuthVerify(req->Request_);
               return;
            }
         }
         req->Visitor_->Authr_ = auths->GetAuthResult();
         req->Visitor_->OnAuthDone(std::move(authr));
         req->AuthServer_.reset();
      }
   };
   AuthRequestSP req{new AuthRequest(this, auth::CreateSaslClient(&strSaslMechList, kMechSplitterChar, authz, authc, pass))};
   if (!req->AuthClient_.SaslClient_) {
      RevPrint(rbuf, "AuthUser.CreateSaslClient|mechs=", strSaslMechList, "|err=Create SASL client fail.");
      this->EmitAuthEvent(State::AuthError, DcQueueList{rbuf.MoveOut()});
      return State::AuthError;
   }
   // 3.建立 Server 端: 認證協商處理物件.
   req->AuthServer_ = this->Authr_.AuthMgr_->CreateAuthSession(req->AuthClient_.MechName_, req);
   if (!req->AuthServer_) {
      RevPrint(rbuf, "AuthUser.CreateAuthSession|err=Not found mechanism: ", req->AuthClient_.MechName_);
      this->EmitAuthEvent(State::AuthError, DcQueueList{rbuf.MoveOut()});
      return State::AuthError;
   }
   // 4.填妥認證要求.
   req->Request_.UserFrom_ = from.ToString();
   req->Request_.Response_ = req->AuthClient_.SaslClient_->GetFirstMessage();
   // 5.進行認證,等候結果: this->OnAuthDone();
   req->AuthServer_->AuthVerify(req->Request_);
   return State::Authing;
}

//--------------------------------------------------------------------------//

SeedSession::State SeedSession::FeedLine(StrView cmdln) {
   St::Locker st{this->St_};
   if (st->State_ != State::UserReady)
      return st->State_;
   if (st->CurrRequest_) {
      if (!st->PendingCommandLines_.empty() && st->PendingCommandLines_.back() != '\n')
         st->PendingCommandLines_.push_back('\n');
      cmdln.AppendTo(st->PendingCommandLines_);
      return st->State_;
   }
   this->ExecuteCommand(st, cmdln);
   return State::UserReady;
}

void SeedSession::OnTicketRunnerDone(seed::TicketRunner& runner, DcQueue&& extmsg) {
   if (runner.OpResult_ < seed::OpResult::no_error) {
      RevBufferList rbuf{128, std::move(extmsg)};
      if (runner.ErrPos_ <= runner.Path_.size()) {
         RevPutChar(rbuf, '^');
         RevPutFill(rbuf, runner.ErrPos_, ' ');
      }
      RevPrint(rbuf, runner.OpResult_, ':', seed::GetOpResultMessage(runner.OpResult_), '\n', runner.Path_, '\n');
      this->OnRequestError(runner, DcQueueList{rbuf.MoveOut()});
   }
   else
      this->OnRequestDone(runner, std::move(extmsg));

   St::Locker st{this->St_};
   assert(st->CurrRequest_ == &runner);
   st->CurrRequest_.reset();
   if (st->State_ == State::Logouting) {
      this->OnAuthEventInLocking(st->State_ = State::UserExit, DcQueueFixedMem{});
      this->ClearLogout(st);
      return;
   }
   if (st->PendingCommandLines_.empty())
      return;
   StrView full{&st->PendingCommandLines_};
   StrView cmdln = StrFetchNoTrim(full, '\n');
   st->PendingCommandLines_.erase(0, st->PendingCommandLines_.size() - full.size());
   this->ExecuteCommand(st, cmdln);
}
void SeedSession::OutputSeedFields(seed::TicketRunner& runner, const seed::SeedOpResult& res, const seed::RawRd& rd, StrView exhead) {
   RevBufferList rbuf{128};
   size_t        ifld = res.Tab_->Fields_.size();
   while (auto fld = res.Tab_->Fields_.Get(--ifld)) {
      RevPutChar(rbuf, '\n');
      fld->CellRevPrint(rd, nullptr, rbuf);
      RevPrint(rbuf, fld->Name_, '=');
   }
   RevPrint(rbuf, res.Sender_->LayoutSP_->KeyField_->Name_, '=', res.KeyText_, '\n');
   if (!exhead.empty())
      RevPrint(rbuf, exhead, '\n');
   this->OnTicketRunnerDone(runner, DcQueueList{rbuf.MoveOut()});
}
void SeedSession::OnTicketRunnerRead(seed::TicketRunnerRead& runner, const seed::SeedOpResult& res, const seed::RawRd& rd) {
   this->OutputSeedFields(runner, res, rd, StrView{});
}
void SeedSession::OnTicketRunnerWrite(seed::TicketRunnerWrite& runner, const seed::SeedOpResult& res, const seed::RawWr& wr) {
   RevBufferList rbuf{runner.ParseSetValues(res, wr)};
   this->OutputSeedFields(runner, res, wr, ToStrView(BufferTo<std::string>(rbuf.MoveOut())));
}
void SeedSession::OnTicketRunnerRemoved(seed::TicketRunnerRemove& runner, const seed::PodRemoveResult& res) {
   RevBufferList rbuf{64};
   RevPrint(rbuf, runner.Path_);
   if (res.OpResult_ == seed::OpResult::removed_pod)
      RevPrint(rbuf, "Pod removed: ");
   else // if (res.OpResult_ == seed::OpResult::removed_seed)
      RevPrint(rbuf, "Seed removed: ");
   this->OnTicketRunnerDone(runner, DcQueueList{rbuf.MoveOut()});
}
void SeedSession::OnTicketRunnerGridView(seed::TicketRunnerGridView& runner, seed::GridViewResult& res) {
   RevBufferList rbuf{128};
   if (res.DistanceEnd_ == 1)
      RevPrint(rbuf, "=== the end ===\n");
   if (res.ContainerSize_ != res.kNotSupported)
      RevPrint(rbuf, "= ", res.RowCount_, '/', res.ContainerSize_, " =");
   RevPrint(rbuf, '\n');

   std::vector<FmtDef>  fmts(res.Tab_ ? (res.Tab_->Fields_.size() + 1) : 1);
   const seed::Field*   fld = res.Sender_->LayoutSP_->KeyField_.get();
   size_t               fldIdx = 0;
   auto                 iFmts = fmts.begin();
   for (; fld; ++iFmts) {
      iFmts->Width_ = static_cast<FmtDef::WidthType>(fld->GetTitleOrName().size());
      if (!seed::IsFieldTypeNumber(fld->Type_))
         iFmts->Flags_ |= FmtFlag::LeftJustify;
      fld = res.Tab_ ? res.Tab_->Fields_.Get(fldIdx++) : nullptr;
   }

   struct FldPos {
      const char* ValPos_;
      size_t      FldIdx_;
   };
   std::vector<FldPos>  fldPos(res.RowCount_ * fldIdx);
   auto    iFldPosBeg = fldPos.begin();
   auto    iFldPos = iFldPosBeg;
   StrView gv{&res.GridView_};
   while (!gv.empty()) {
      StrView ln = StrFetchNoTrim(gv, static_cast<char>(res.kRowSplitter));
      for (size_t L = 0; L < fmts.size(); ++L) {
         StrView val = StrFetchNoTrim(ln, static_cast<char>(res.kCellSplitter));
         iFldPos->ValPos_ = val.begin();
         iFldPos->FldIdx_ = L;
         ++iFldPos;
         if (fmts[L].Width_ < val.size())
            fmts[L].Width_ = static_cast<FmtDef::WidthType>(val.size());
      }
   }
   if (iFldPos != iFldPosBeg) {
      const char* pend = gv.end();
      for (;;) {
         --iFldPos;
         StrView val(iFldPos->ValPos_, pend);
         RevPrint(rbuf, val, fmts[iFldPos->FldIdx_]);
         if (iFldPos == iFldPosBeg)
            break;
         pend = val.begin() - 1;
         RevPrint(rbuf, iFldPos->FldIdx_ == 0 ? '\n' : '|');
      }
   }
   RevPrint(rbuf, '\n');
   if (fldIdx > 0) {
      while (--fldIdx > 0)
         RevPrint(rbuf, '|', res.Tab_->Fields_.Get(fldIdx - 1)->GetTitleOrName(), fmts[fldIdx]);
   }
   RevPrint(rbuf, res.Sender_->LayoutSP_->KeyField_->GetTitleOrName(), fmts[0]);
   this->OnTicketRunnerDone(runner, DcQueueList{rbuf.MoveOut()});
}
void SeedSession::OnTicketRunnerCommand(seed::TicketRunnerCommand& runner, const seed::SeedOpResult&, StrView msg) {
   if (msg.empty()) {
      msg = (runner.OpResult_ == seed::OpResult::no_error
             ? StrView{"Seed command done."}
      : StrView{"Seed command error."});
   }
   this->OnTicketRunnerDone(runner, DcQueueFixedMem{msg});
}
void SeedSession::OnTicketRunnerSetCurrPath(seed::TicketRunnerCommand& runner) {
   this->SetCurrPath(ToStrView(runner.Path_));
   this->OnTicketRunnerDone(runner, DcQueueFixedMem{});
}

//--------------------------------------------------------------------------//

struct SeedSession::PrintLayout : public seed::TicketRunnerTree {
   fon9_NON_COPY_NON_MOVE(PrintLayout);
   using base = seed::TicketRunnerTree;
   using base::base;
   void OnFoundTree(seed::TreeOp& op) override {
      RevBufferList        rbuf{128};
      const seed::Layout*  layout = op.Tree_.LayoutSP_.get();
      for (size_t iTab = layout->GetTabCount(); iTab > 0;) {
         seed::Tab*  tab = layout->GetTab(--iTab);
         RevPrint(rbuf, seed::MakeFieldsConfig(tab->Fields_, '|', '\n'));
         RevPrint(rbuf, "Tab[", iTab, "] ", tab->Name_, '\n');
      }
      std::string fldcfg;
      seed::AppendFieldConfig(fldcfg, *layout->KeyField_);
      fldcfg.push_back(' ');
      SerializeNamed(fldcfg, *layout->KeyField_, '|', '\n');
      RevPrint(rbuf, this->Path_, "\n" "KeyField: ", fldcfg);
      this->Visitor_->OnTicketRunnerDone(*this, DcQueueList{rbuf.MoveOut()});
   }
};

static inline uint16_t AdjustGridViewRowCount(uint16_t rowCount) {
   // 若沒指定 rowCount 則, 保留3行給 Header/Footer/Prompt 使用.
   return (rowCount > 5) ? static_cast<uint16_t>(rowCount - 3) : rowCount;
}
uint16_t SeedSession::GetDefaultGridViewRowCount() {
   return 25;
}

SeedSession::RequestSP SeedSession::MakeRequest(StrView cmdln) {
   seed::SeedFairy::Request req{*this, cmdln};
   if (req.Runner_) {
      if (seed::TicketRunnerGridView* gv = dynamic_cast<seed::TicketRunnerGridView*>(req.Runner_.get())) {
         if (gv->RowCount_ <= 0)
            gv->RowCount_ = AdjustGridViewRowCount(this->GetDefaultGridViewRowCount());
         this->LastGV_.reset(gv);
      }
      return req.Runner_;
   }
   if (req.Command_ == "pl") { // print layout: `pl [treeName]`
      if (!req.CommandArgs_.empty())
         return seed::TicketRunnerError::ArgumentsMustBeEmpty(*this);
      return new PrintLayout(*this, req.SeedName_);
   }
   if (req.Command_ == "gv+") { // 接續上次的最後一筆: `gv+` or `gv+`
      if (!req.SeedName_.empty() || !req.CommandArgs_.empty())
         return new seed::TicketRunnerError(*this, seed::OpResult::bad_command_argument, "Invalid gv+ arguments.");
      if (this->LastGV_)
         this->LastGV_->Continue();
      else
         this->LastGV_.reset(new seed::TicketRunnerGridView(*this,
                                                            req.SeedName_,
                                                            AdjustGridViewRowCount(this->GetDefaultGridViewRowCount()),
                                                            seed::TreeOp::TextBegin(),
                                                            req.CommandArgs_ //tabName
                           ));
      return this->LastGV_;
   }
   return seed::TicketRunnerError::UnknownCommand(*this);
}
void SeedSession::ExecuteCommand(St::Locker& st, StrView cmdln) {
   RequestSP req = st->CurrRequest_ = this->MakeRequest(cmdln);
   st.unlock();
   req->Run();
}

} // namespace fon9
