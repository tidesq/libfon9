/// \file fon9/framework/SeedImporter.cpp
///
/// - 僅適合匯入少量初始化資料, 不適合處理整批大量資料.
///   如果需要匯入整批大量資料(例: 客戶庫存, 全市場商品...), 建議使用專用匯入模組.
///
/// - SeedImporter factory:
///   - 參數設定: "Name=SeedImporter|AuthMgr=|AddTo="
///   - 若沒提供 Name 則預設為 "SeedImporter"
///   - AuthMgr= 透過哪個 auth mgr 取得 Acl.
///   - AddTo= 或 SessionFactoryPark= 加入到哪個 session factory park.
///   - IoMgr= 或 IoManager= 加入到哪個 IoManager 所參考的 session factory park.
///
/// - SeedImporter session:
///   - 參數設定: "Role=roleId|OutputTo=Dev"
///   - 連線後直接使用設定的 role 取得 Acl, 不用登入.
///   - 預設為 "OutputTo=Log": 當有錯誤時記錄到 log,
///     且不論成功或失敗都不會透過 Dev 回覆任何訊息.
///
/// - 連線到 SeedImporter session 之後, 指令範例(使用 '\n' 換行):
///   ss,PriRef=35.7,PriUpLmt=39.25,PriDnLmt=32.15 /SymbIn/1101^Ref
///
/// \author fonwinz@gmail.com
#include "fon9/framework/IoFactory.hpp"
#include "fon9/auth/AuthMgr.hpp"
#include "fon9/auth/PolicyAcl.hpp"
#include "fon9/seed/SeedVisitor.hpp"

namespace fon9 {

enum class OutputTo {
   /// 錯訊息輸出到 Log, 一般訊息直接拋棄.
   Log,
   /// 訊息透過 Device 回覆.
   Dev,
};

fon9_WARN_DISABLE_PADDING;
struct SeedImporterArgs {
   SeedImporterArgs() = delete;
   SeedImporterArgs(auth::AuthMgrSP authMgr) : Authr_{std::move(authMgr)} {
   }
   auth::AuthResult  Authr_;
   seed::AclConfig   Acl_;
   OutputTo          OutputTo_{OutputTo::Log};
};
fon9_WARN_POP;

struct SeedImporterVisitor : public seed::SeedVisitor {
   fon9_NON_COPY_NON_MOVE(SeedImporterVisitor);
   using base = seed::SeedVisitor;
   const io::DeviceSP   Dev_;
   SeedImporterVisitor(const SeedImporterArgs& args, io::Device& dev)
      : base(args.Authr_.AuthMgr_->MaRoot_, args.Authr_.MakeUFrom(ToStrView(dev.WaitGetDeviceId())))
      , Dev_(args.OutputTo_ == OutputTo::Dev ? &dev : nullptr) {
      this->Fairy_->ResetAclConfig(args.Acl_);
   }

   template <class... ArgsT>
   void OnError(ArgsT&&... args) {
      RevBufferList  rbuf{fon9::kLogBlockNodeSize};
      fon9::RevPrint(rbuf, "SeedImporter.", std::forward<ArgsT>(args)..., '\n');
      this->OutputError(std::move(rbuf));
   }
   void OutputError(RevBufferList&& rbuf) {
      if ((this->Dev_ && this->Dev_->OpImpl_GetState() == io::State::LinkReady))
         this->Dev_->Send(rbuf.MoveOut());
      else if (LogLevel::Error >= fon9::LogLevel_)
         fon9::LogWrite(LogLevel::Error, std::move(rbuf));
   }
   void OnTicketRunnerDone(seed::TicketRunner& runner, DcQueue&& extmsg) override {
      (void)extmsg;
      if (runner.OpResult_ >= seed::OpResult::no_error)
         return;
      if (!this->Dev_) { // 使用 Log 記錄失敗.
         if (dynamic_cast<seed::TicketRunnerRemove*>(&runner))
            // Remove 在 SeedVisitor.cpp: TicketRunnerRemove::OnBeforeRemove(); 已處理.
            return;
      }
      this->OnError(runner.Bookmark_,
                    "|path=", runner.OrigPath_,
                    "|err=", runner.OpResult_, ':', seed::GetOpResultMessage(runner.OpResult_));
   }
   void OnTicketRunnerGridView(seed::TicketRunnerGridView&, seed::GridViewResult& res) override {
      (void)res;
   }
   void OnTicketRunnerRead(seed::TicketRunnerRead&, const seed::SeedOpResult& res, const seed::RawRd& rd) override {
      (void)res; (void)rd;
   }
   void OnTicketRunnerWrite(seed::TicketRunnerWrite& runner, const seed::SeedOpResult& res, const seed::RawWr& wr) override {
      // runner.ParseSetValues() 已經有寫log: SeedVisitor.cpp;
      // 沒有消息就是好消息, 所以成功時不回覆.
      RevBufferList rbuf{runner.ParseSetValues(res, wr)};
      if (rbuf.cfront() != nullptr)
         this->OutputError(std::move(rbuf));
   }
   void OnTicketRunnerRemoved(seed::TicketRunnerRemove&, const seed::PodRemoveResult& res) override {
      (void)res;
      // Remove 不論成功或失敗, 都有寫log: SeedVisitor.cpp: TicketRunnerRemove::OnBeforeRemove();
      // 所以只要考慮失敗時需要送 Dev_ 的情況: 在 OnTicketRunnerDone() 處理.
   }
   void OnTicketRunnerCommand(seed::TicketRunnerCommand&, const seed::SeedOpResult& res, StrView msg) override {
      (void)res; (void)msg;
   }
   void OnTicketRunnerSetCurrPath(seed::TicketRunnerCommand&) override {
   }
   void OnSeedNotify(seed::VisitorSubr&, const seed::SeedNotifyArgs&) override {
   }
   void OnTicketRunnerSubscribe(seed::TicketRunnerSubscribe&, bool isSubOrUnsub) override {
      (void)isSubOrUnsub;
   }
};
using VisitorSP = intrusive_ptr<SeedImporterVisitor>;

/// SeedImporter 僅支援 Write, Remove.
class SeedImporterSession : public io::Session {
   fon9_NON_COPY_NON_MOVE(SeedImporterSession);
   SeedImporterArgs  Args_;
   LinePeeker        LinePeeker_;
   VisitorSP         Visitor_;
public:
   SeedImporterSession(SeedImporterArgs&& args) : Args_(std::move(args)) {
   }
   SeedImporterSession(const SeedImporterArgs& args) : Args_(args) {
   }
   void OnDevice_StateChanged(io::Device&, const io::StateChangedArgs& e) override {
      if (e.BeforeState_ == io::State::LinkReady) {
         this->Visitor_.reset();
         this->LinePeeker_.Clear();
      }
   }
   io::RecvBufferSize OnDevice_LinkReady(io::Device& dev) override {
      this->Visitor_.reset(new SeedImporterVisitor(this->Args_, dev));
      return io::RecvBufferSize::Default;
   }
   io::RecvBufferSize OnDevice_Recv(io::Device&, DcQueueList& rxbuf) override {
      while (const char* pln = this->LinePeeker_.PeekUntil(rxbuf, '\n')) {
         StrView cmdln{pln, this->LinePeeker_.LineSize_};
         if (StrTrimTail(&cmdln).empty())
            goto __POP_CONSUMED_AND_CONTINUE;
         // SeedImporter 僅支援 "ss", "rs"
         if (cmdln.size() > 2) {
            switch (cmdln.Get1st()) {
            case 's': case 'r':
               if (cmdln.begin()[1] == 's') {
                  seed::SeedFairy::Request req(*this->Visitor_, cmdln);
                  if (fon9_LIKELY(req.Runner_)) {
                     req.Runner_->Bookmark_.assign(cmdln.begin(), 2);
                     req.Runner_->Run();
                     goto __POP_CONSUMED_AND_CONTINUE;
                  }
               }
               break;
            }
         }
         this->Visitor_->OnError("Unknown command: ", cmdln);
      __POP_CONSUMED_AND_CONTINUE:
         this->LinePeeker_.PopConsumed(rxbuf);
      }
      return io::RecvBufferSize::Default;
   }
};
struct SeedImporterServer : public io::SessionServer {
   fon9_NON_COPY_NON_MOVE(SeedImporterServer);
   SeedImporterArgs  Args_;
   SeedImporterServer(SeedImporterArgs&& args) : Args_(std::move(args)) {
   }
   io::SessionSP OnDevice_Accepted(io::DeviceServer&) override {
      return new SeedImporterSession{this->Args_};
   }
};

//--------------------------------------------------------------------------//

struct SeedImporterFactory : public SessionFactory {
   fon9_NON_COPY_NON_MOVE(SeedImporterFactory);
   auth::AuthMgrSP   AuthMgr_;
   SeedImporterFactory(std::string name, auth::AuthMgrSP&& authMgr)
      : SessionFactory(std::move(name))
      , AuthMgr_{std::move(authMgr)} {
   }
   bool ParseSeedImporterArgs(StrView cfgstr, std::string& errReason, SeedImporterArgs& args) {
      StrView roleId;
      StrView tag, value;
      while (StrFetchTagValue(cfgstr, tag, value)) {
         if (tag == "Role")
            roleId = value;
         else if (tag == "OutputTo") {
            if (value == "Dev" || value == "Device")
               args.OutputTo_ = OutputTo::Dev;
            else if (value == "Log")
               args.OutputTo_ = OutputTo::Log;
            else {
               errReason = value.ToString("Unknown value: OutputTo=");
               return false;
            }
         }
         else {
            errReason = tag.ToString("Unknown tag: ");
            return false;
         }
      }
      args.Authr_.AuthcId_.assign(&this->Name_);
      // args.Authr_.AuthzId_.assign(...);
      this->AuthMgr_->RoleMgr_->GetRole(roleId, args.Authr_);
      if (!this->AuthMgr_->GetPolicy<auth::PolicyAclAgent>(fon9_kCSTR_PolicyAclAgent_Name, args.Authr_, args.Acl_)) {
         errReason = "AuthMgr.GetPolicy." fon9_kCSTR_PolicyAclAgent_Name "|err=Not found.";
         return false;
      }
      if (args.Acl_.IsAccessDeny()) {
         errReason = "err=Acl is empty.";
         return false;
      }
      return true;
   }
   template <class SessionT>
   SessionT* CreateSeedImporter(const IoConfigItem& cfg, std::string& errReason) {
      SeedImporterArgs  args{this->AuthMgr_};
      if (this->ParseSeedImporterArgs(ToStrView(cfg.SessionArgs_), errReason, args))
         return new SessionT(std::move(args));
      return nullptr;
   }
   io::SessionSP CreateSession(IoManager&, const IoConfigItem& cfg, std::string& errReason) override {
      return this->CreateSeedImporter<SeedImporterSession>(cfg, errReason);
   }
   io::SessionServerSP CreateSessionServer(IoManager&, const IoConfigItem& cfg, std::string& errReason) override {
      return this->CreateSeedImporter<SeedImporterServer>(cfg, errReason);
   }
};
class SeedImporterArgsParser : public SessionFactoryConfigParser {
   fon9_NON_COPY_NON_MOVE(SeedImporterArgsParser);
   using base = SessionFactoryConfigParser;
   seed::PluginsHolder& PluginsHolder_;
   StrView              AuthMgrName_;
public:
   SeedImporterArgsParser(seed::PluginsHolder& pluginsHolder)
      : SessionFactoryConfigParser{"SeedImporter"}
      , PluginsHolder_{pluginsHolder} {
   }
   bool OnUnknownTag(seed::PluginsHolder& holder, StrView tag, StrView value) override {
      if (tag == "AuthMgr") {
         this->AuthMgrName_ = value;
         return true;
      }
      return base::OnUnknownTag(holder, tag, value);
   }
   SessionFactorySP CreateSessionFactory() override {
      if (auto authMgr = this->PluginsHolder_.Root_->Get<auth::AuthMgr>(this->AuthMgrName_))
         return new SeedImporterFactory(this->Name_, std::move(authMgr));
      this->ErrMsg_ += "|err=Unknown AuthMgr";
      if (!this->AuthMgrName_.empty()) {
         this->ErrMsg_.push_back('=');
         this->AuthMgrName_.AppendTo(this->ErrMsg_);
      }
      return nullptr;
   }
};

static bool SeedImporter_Start(seed::PluginsHolder& holder, StrView args) {
   return SeedImporterArgsParser{holder}.Parse(holder, args);
}

} // namespaces

//--------------------------------------------------------------------------//

extern "C" fon9_API fon9::seed::PluginsDesc f9p_SeedImporter;
static fon9::seed::PluginsPark f9pRegister{"SeedImporter", &f9p_SeedImporter};

fon9::seed::PluginsDesc f9p_SeedImporter{"", &fon9::SeedImporter_Start, nullptr, nullptr,};
