// \file fon9/framework/SeedSssion.cpp
// \author fonwinz@gmail.com
#include "fon9/framework/SeedSession.hpp"
#include "fon9/auth/SaslClient.hpp"
#include "fon9/auth/PolicyAcl.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 {

SeedSession::SeedSession(seed::MaTreeSP root, auth::AuthMgrSP authMgr, bool isAdminMode)
   : Fairy_{new seed::SeedFairy{std::move(root)}}
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
   this->Fairy_->Ac_.Home_.assign("/");
   this->Fairy_->Ac_.Acl_.kfetch(fon9::seed::AclPath::MakeRef("/", 1)).second = fon9::seed::AccessRight::Full;
   this->Fairy_->Ac_.Acl_.kfetch(fon9::seed::AclPath::MakeRef("/..", 3)).second = fon9::seed::AccessRight::Full;
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
   st->CommandLines_.clear();
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
   else if (!this->Authr_.AuthMgr_->GetPolicy<auth::PolicyAclAgent>(fon9_kCSTR_PolicyAclAgent_Name, this->Authr_, this->Fairy_->Ac_)) {
      st = State::UserExit;
      msg = StrView{"AuthUser.GetPolicy." fon9_kCSTR_PolicyAclAgent_Name "|err=Not found."};
   }
   else if (this->Fairy_->Ac_.Acl_.empty()) { // 沒權限, 禁止登入 console.
      st = State::UserExit;
      msg = StrView{"SeedSession.Run|err=Acl is empty."};
   }
   else {
      this->Fairy_->CurrPath_ = this->Fairy_->Ac_.Home_;
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
      const SeedSessionSP  Session_;
      AuthRequest(SeedSession* owner, auth::SaslClientR&& cli)
         : AuthClient_{std::move(cli)}
         , Session_{owner} {
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
         req->Session_->Authr_ = auths->GetAuthResult();
         req->Session_->OnAuthDone(std::move(authr));
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

void SeedSession::UpdatePrompt(StrView currPath) {
   St::Locker st{this->St_};
   this->Fairy_->CurrPath_.assign(currPath);
   RevBufferList rbuf{static_cast<BufferNodeSize>(currPath.size() + 64)};
   RevPrint(rbuf, this->Authr_.AuthcId_, "] ", currPath, '>');
   if (!this->Authr_.AuthzId_.empty())
      RevPrint(rbuf, this->Authr_.AuthzId_, '/');
   RevPrint(rbuf, '[');
   st->Prompt_ = BufferTo<std::string>(rbuf.MoveOut());
}

SeedSession::State SeedSession::FeedLine(StrView cmdln) {
   St::Locker st{this->St_};
   if (st->State_ != State::UserReady)
      return st->State_;
   if (st->CurrRequest_) {
      if (!st->CommandLines_.empty() && st->CommandLines_.back() != '\n')
         st->CommandLines_.push_back('\n');
      cmdln.AppendTo(st->CommandLines_);
      return st->State_;
   }
   this->ExecuteCommand(st, cmdln);
   return State::UserReady;
}

void SeedSession::EmitRequestDone(RequestSP req, DcQueue&& extmsg) {
   if (req->OpResult_ < seed::OpResult::no_error) {
      RevBufferList rbuf{128};
      if (req->ErrPos_ <= req->AclPath_.size()) {
         RevPutChar(rbuf, '^');
         RevPutFill(rbuf, req->ErrPos_, ' ');
      }
      RevPrint(rbuf, req->OpResult_, ':', seed::GetOpResultMessage(req->OpResult_), '\n', req->AclPath_, '\n');
      this->OnRequestError(*req, DcQueueList{rbuf.MoveOut()});
   }
   else
      this->OnRequestDone(*req, std::move(extmsg));

   St::Locker st{this->St_};
   assert(req == st->CurrRequest_);
   st->CurrRequest_.reset();
   if (st->State_ == State::Logouting) {
      this->OnAuthEventInLocking(st->State_ = State::UserExit, DcQueueFixedMem{});
      this->ClearLogout(st);
      return;
   }
   if (st->CommandLines_.empty())
      return;
   StrView full{&st->CommandLines_};
   StrView cmdln = StrFetchNoTrim(full, '\n');
   st->CommandLines_.erase(0, st->CommandLines_.size() - full.size());
   this->ExecuteCommand(st, cmdln);
}

//--------------------------------------------------------------------------//

void SeedSession::ExecuteCommand(St::Locker& st, StrView cmdln) {
   RequestSP req = st->CurrRequest_ = this->MakeRequest(cmdln);
   st.unlock();

   if (req->Root_ == nullptr)
      this->EmitRequestDone(req, DcQueueFixedMem{});
   else
      seed::StartSeedSearch(*req->Root_, req);
}

SeedSession::RequestSP SeedSession::MakeRequest(StrView cmdln) {
   StrView cmd = SbrFetchField(cmdln, ' ');
   StrTrimHead(&cmdln);
   switch (cmd.Get1st()) {
   case '`':
   case '\'':
   case '"':
      // 使用引號, 表示強制使用 seed name, 且從 CurrPath 開始.
   case '/':
   case '.':
   case '~':
      return this->MakeSeedCommandRequest(cmd, cmdln);
   default:
      return this->MakeCommandRequest(cmd, cmdln);
   }
}

//--------------------------------------------------------------------------//

SeedSession::RequestBase::RequestBase(SeedSession* owner, StrView& seed, seed::AccessRight needsRights)
   : Session_{owner} {
   this->OpResult_ = owner->Fairy_->NormalizeSeedPath(seed, this->AclPath_, needsRights);
   if (this->OpResult_ != seed::OpResult::no_error)
      this->Root_ = nullptr;
   else {
      seed = ToStrView(this->AclPath_);
      this->Root_ = owner->Fairy_->GetRootPath(seed);
   }
   this->ErrPos_ = static_cast<size_t>(seed.begin() - this->AclPath_.begin());
}

void SeedSession::Request::OnError(seed::OpResult res) {
   this->OpResult_ = res;
   this->ErrPos_ += static_cast<size_t>(this->RemainPath_.begin() - this->OrigPath_.begin());
   this->Session_->EmitRequestDone(this, DcQueueFixedMem{});
}

void SeedSession::Request::OnLastStep(seed::TreeOp& op, StrView keyText, seed::Tab& tab) {
   this->RemainPath_.SetBegin(keyText.begin());
   op.Get(keyText, std::bind(&Request::OnLastSeedOp, this, std::placeholders::_1, std::placeholders::_2, std::ref(tab)));
}

SeedSession::RequestSP SeedSession::MakeSeedCommandRequest(StrView seed, StrView cmdln) {
   struct SeedCommandRequest : public Request {
      fon9_NON_COPY_NON_MOVE(SeedCommandRequest);
      using base = Request;
      const std::string SeedCommandLine_;
      SeedCommandRequest(SeedSession* owner, StrView seed, StrView cmdln)
         : base{owner, seed, cmdln.empty() ? seed::AccessRight::None : seed::AccessRight::Exec}
         , SeedCommandLine_{cmdln.ToString()} {
      }
      void SetNewCurrPath() {
         this->Session_->UpdatePrompt(ToStrView(this->AclPath_));
         this->Session_->EmitRequestDone(this, DcQueueFixedMem{});
      }
      void OnFoundTree(seed::TreeOp&) override {
         if (this->SeedCommandLine_.empty())
            this->SetNewCurrPath();
         else
            this->OnError(seed::OpResult::not_supported_cmd);
      }
      void OnLastSeedOp(const seed::PodOpResult& resPod, seed::PodOp* pod, seed::Tab& tab) override {
         if (!pod)
            this->OnError(resPod.OpResult_);
         else if (this->SeedCommandLine_.empty())
            this->SetNewCurrPath();
         else
            pod->OnSeedCommand(&tab, &this->SeedCommandLine_,
                               std::bind(&SeedCommandRequest::OnSeedCommandResult, intrusive_ptr<SeedCommandRequest>(this),
                                         std::placeholders::_1, std::placeholders::_2));
      }
      void OnSeedCommandResult(const seed::SeedOpResult& res, StrView msg) {
         this->OpResult_ = res.OpResult_;
         if (msg.empty()) {
            msg = (this->OpResult_ == seed::OpResult::no_error
                  ? StrView{"Seed command done."}
                  : StrView{"Seed command error."});
         }
         this->Session_->EmitRequestDone(this, DcQueueFixedMem{msg});
      }
   };
   return new SeedCommandRequest(this, seed, cmdln);
}

//--------------------------------------------------------------------------//

void SeedSession::OutputSeedFields(RequestSP req, const seed::SeedOpResult& res, const seed::RawRd& rd, StrView exhead) {
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
   this->EmitRequestDone(std::move(req), DcQueueList{rbuf.MoveOut()});
}

struct SeedSession::RequestTree : public Request {
   fon9_NON_COPY_NON_MOVE(RequestTree);
   using base = Request;
   RequestTree(SeedSession* owner, StrView seed)
      : base{owner, seed, seed::AccessRight::Read} {
   }
   void OnLastStep(seed::TreeOp& opTree, StrView keyText, seed::Tab& tab) {
      this->ContinueSeed(opTree, keyText, tab); // 進入 OnFoundTree();
   }
   void OnLastSeedOp(const seed::PodOpResult&, seed::PodOp*, seed::Tab&) override {}
};

struct SeedSession::RequestOpError : public Request {
   fon9_NON_COPY_NON_MOVE(RequestOpError);
   using base = Request;
   RequestOpError(SeedSession* owner, seed::OpResult errn, const StrView& errmsg) : base{owner, errn, errmsg} {
   }
   void OnFoundTree(seed::TreeOp&) override {}
   void OnLastSeedOp(const seed::PodOpResult&, seed::PodOp*, seed::Tab&) override {}
};

struct SeedSession::SetSeedFields : public Request {
   fon9_NON_COPY_NON_MOVE(SetSeedFields);
   using base = Request;
   const std::string FieldValues_;
   SetSeedFields(SeedSession* owner, StrView seed, StrView fldValues)
      : base{owner, seed, seed::AccessRight::Write}
      , FieldValues_{fldValues.ToString()} {
   }
   void OnFoundTree(seed::TreeOp&) override {
      this->OnError(seed::OpResult::not_supported_write);
   }
   void OnLastStep(seed::TreeOp& op, StrView keyText, seed::Tab& tab) override {
      this->RemainPath_.SetBegin(keyText.begin());
      op.Add(keyText, std::bind(&SetSeedFields::OnLastSeedOp, this, std::placeholders::_1, std::placeholders::_2, std::ref(tab)));
   }
   void OnLastSeedOp(const seed::PodOpResult& resPod, seed::PodOp* pod, seed::Tab& tab) override {
      if (pod)
         pod->BeginWrite(tab, std::bind(&SetSeedFields::OnWrite, this, std::placeholders::_1, std::placeholders::_2));
      else
         this->OnError(resPod.OpResult_);
   }
   void OnWrite(const seed::SeedOpResult& res, const seed::RawWr* wr) {
      if (!wr) {
         this->OnError(res.OpResult_);
         return;
      }
      StrView       fldvals{&this->FieldValues_};
      RevBufferList rbuf{128};
      while (!fldvals.empty()) {
         StrView val = SbrFetchField(fldvals, ',');
         StrView fldName = StrFetchTrim(val, '=');
         auto    fld = res.Tab_->Fields_.Get(fldName);
         if (fld == nullptr)
            RevPrint(rbuf, "fieldName=", fldName, "|err=field not found\n");
         else {
            seed::OpResult r = fld->StrToCell(*wr, StrRemoveHeadTailQuotes(val));
            if (r != seed::OpResult::no_error)
               RevPrint(rbuf, "fieldName=", fldName, "|err=StrToCell():", r, ':', seed::GetOpResultMessage(r), '\n');
         }
      }
      this->Session_->OutputSeedFields(this, res, *wr, ToStrView(BufferTo<std::string>(rbuf.MoveOut())));
   }
};

struct SeedSession::PrintSeed : public Request {
   fon9_NON_COPY_NON_MOVE(PrintSeed);
   using base = Request;
   PrintSeed(SeedSession* owner, StrView seed)
      : base{owner, seed, seed::AccessRight::Read} {
   }
   void OnFoundTree(seed::TreeOp&) override {
      this->OnError(seed::OpResult::not_supported_read);
   }
   void OnLastSeedOp(const seed::PodOpResult& resPod, seed::PodOp* pod, seed::Tab& tab) override {
      if (pod)
         pod->BeginRead(tab, std::bind(&PrintSeed::OnRead, this, std::placeholders::_1, std::placeholders::_2));
      else
         this->OnError(resPod.OpResult_);
   }
   void OnRead(const seed::SeedOpResult& res, const seed::RawRd* rd) {
      if (rd)
         this->Session_->OutputSeedFields(this, res, *rd, StrView{});
      else
         this->OnError(res.OpResult_);
   }
};

struct SeedSession::RemoveSeed : public Request {
   fon9_NON_COPY_NON_MOVE(RemoveSeed);
   using base = Request;
   seed::FnPodRemoved RemovedHandler_;
   RemoveSeed(SeedSession* owner, StrView seed)
      : base{owner, seed, seed::AccessRight::Write} {
      this->RemovedHandler_ = [this](const seed::PodRemoveResult& res) {
         RevBufferList rbuf{64};
         RevPrint(rbuf, this->AclPath_);
         if (res.OpResult_ == seed::OpResult::removed_pod)
            RevPrint(rbuf, "Pod removed: ");
         else
            RevPrint(rbuf, "Seed removed: ");
         this->Session_->EmitRequestDone(this, DcQueueList{rbuf.MoveOut()});
      };
   }
   void OnFoundTree(seed::TreeOp&) override {
      this->OnError(seed::OpResult::not_supported_remove_pod);
   }
   void OnLastSeedOp(const seed::PodOpResult&, seed::PodOp*, seed::Tab&) override {
      // 不會執行到這兒.
      this->OnError(seed::OpResult::not_supported_remove_pod);
   }
   void ContinuePod(seed::TreeOp& opTree, StrView keyText, StrView tabName) override {
      this->ContinuePodForRemove(opTree, keyText, tabName, this->RemovedHandler_);
   }
};

struct SeedSession::PrintLayout : public RequestTree {
   fon9_NON_COPY_NON_MOVE(PrintLayout);
   using base = RequestTree;
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
      RevPrint(rbuf, this->AclPath_, "\n" "KeyField: ", fldcfg);
      this->Session_->EmitRequestDone(this, DcQueueList{rbuf.MoveOut()});
   }
};

fon9_WARN_DISABLE_PADDING;
struct SeedSession::GridView : public RequestTree {
   fon9_NON_COPY_NON_MOVE(GridView);
   using base = RequestTree;
   CharVector  StartKeyBuf_;
   StrView     StartKey_;
   CharVector  TabName_;
   uint16_t    RowCount_;
   CharVector  LastKey_; // for Continue();
   GridView(SeedSession* owner, StrView seed, uint16_t rowCount, StrView startKey, StrView tabName)
      : base{owner, seed}
      , StartKeyBuf_{startKey}
      , StartKey_{startKey}
      , TabName_{tabName}
      , RowCount_{rowCount} {
      if (!seed::TreeOp::IsTextBegin(startKey) && !seed::TreeOp::IsTextEnd(startKey))
         this->StartKey_ = ToStrView(this->StartKeyBuf_);
   }
   void Continue() {
      this->RemainPath_ = ToStrView(this->OrigPath_);
      this->StartKeyBuf_ = this->LastKey_;
      this->StartKey_ = ToStrView(this->StartKeyBuf_);
   }
   void OnFoundTree(seed::TreeOp& opTree) override {
      seed::GridViewRequest req{this->StartKey_};
      req.MaxRowCount_ = this->RowCount_;
      req.Tab_ = this->TabName_.empty()
         ? opTree.Tree_.LayoutSP_->GetTab(0)
         : opTree.Tree_.LayoutSP_->GetTab(ToStrView(this->TabName_));
      if (req.Tab_ == nullptr) {
         this->OnError(seed::OpResult::not_found_tab);
         return;
      }
      opTree.GridView(req, [this](seed::GridViewResult& res) {
         if (res.OpResult_ != seed::OpResult::no_error) {
            this->OnError(res.OpResult_);
            return;
         }
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
         StrView lastKey;
         while (!gv.empty()) {
            StrView ln = StrFetchNoTrim(gv, static_cast<char>(res.kRowSplitter));
            for (size_t L = 0; L < fmts.size(); ++L) {
               StrView val = StrFetchNoTrim(ln, static_cast<char>(res.kCellSplitter));
               iFldPos->ValPos_ = val.begin();
               iFldPos->FldIdx_ = L;
               ++iFldPos;
               if (fmts[L].Width_ < val.size())
                  fmts[L].Width_ = static_cast<FmtDef::WidthType>(val.size());
               if (L == 0)
                  lastKey = val;
            }
         }
         this->LastKey_.assign(lastKey);

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
         // TODO: this->StartKeyBuf_ = 紀錄最後那筆的 keyText; this->StartKey_ = ToStrView(this->StartKeyBuf_);
         this->Session_->EmitRequestDone(this, DcQueueList{rbuf.MoveOut()});
      });
   }
};
fon9_WARN_POP;

SeedSession::RequestSP SeedSession::MakeCommandRequest(StrView cmd, StrView seed) {
   StrView args{cmd};
   cmd = StrFetchNoTrim(args, ',');

   if (cmd == "ss") {
      // set seed field value, seed 若不存在則會建立, 建立後再設定 field 的值.
      // ss,fld1=val1,fld2=val2... [seedName]
      return new SetSeedFields{this, seed, args};
   }
   if (cmd == "ps") {
      // print seed values
      // ps [seedName]
      if (!args.empty())
         goto __ARGS_MUST_EMPTY;
      return new PrintSeed{this, seed};
   }
   if (cmd == "rs") {
      // remove seed(or pod)
      // rs [seedName]
      if (!args.empty())
         goto __ARGS_MUST_EMPTY;
      return new RemoveSeed{this, seed};
   }
   if (cmd == "pl") {
      // print layout
      // pl [treeName]
      if (!args.empty())
         goto __ARGS_MUST_EMPTY;
      return new PrintLayout{this, seed};
   }
   if (cmd == "gv") {
      // grid view
      // gv[,[rowCount][,startKey[^tabName]]] [treePath]
      // gv,,startKey   [treePath]   使用預設 rowCount, 從指定的 startKey 開始, 使用 Tab[0]
      // gv,rowCount    [treePath]   指定 rowCount, 從 begin 開始, 使用 Tab[0]
      // gv,,^tabName   [treePath]   指定 tabName, 從 begin 開始
      // gv,,''^tabName [treePath]   指定 tabName, 從 key=empty 開始
      //
      uint16_t rowCount = this->GetDefaultGridViewRowCount();
      StrView  startKey = seed::TreeOp::TextBegin();
      if (isdigit(args.Get1st())) {
         const char* pend;
         rowCount = StrTo(args, static_cast<uint16_t>(0), &pend);
         args.SetBegin(pend);
      }
      else if (rowCount > 5) // 若沒指定 rowCount 則, 保留3行給 Header/Footer/Prompt 使用.
         rowCount = static_cast<uint16_t>(rowCount - 3);
      if (!args.empty()) {
         if(args.Get1st() != ',')
            return new RequestOpError{this, seed::OpResult::bad_command_argument, "Invalid GridView arguments."};
         args.SetBegin(args.begin() + 1);
         startKey = seed::ParseKeyTextAndTabName(args);
      }
      this->LastGV_.reset(new GridView{this, seed, rowCount, startKey, args});
      return this->LastGV_;
   }
   if (cmd == "gv+") {
      // cmd == "gv+"   接續上次的最後一筆
      if (!seed.empty() || !args.empty())
         return new RequestOpError{this, seed::OpResult::bad_command_argument, "Invalid gv+ arguments."};
      if (this->LastGV_)
         this->LastGV_->Continue();
      else
         this->LastGV_.reset(new GridView{this, seed, this->GetDefaultGridViewRowCount(), seed::TreeOp::TextBegin(), args});
      return this->LastGV_;
   }

   return new RequestOpError{this, seed::OpResult::not_supported_cmd, "Unknown command."};

__ARGS_MUST_EMPTY:
   return new RequestOpError{this, seed::OpResult::bad_command_argument, "Arguments must be empty."};
}

uint16_t SeedSession::GetDefaultGridViewRowCount() {
   return 25;
}

} // namespace fon9
