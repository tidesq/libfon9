/// \file fon9/framework/SeedSession.cpp
/// \author fonwinz@gmail.com
#ifndef __fon9_framework_SeedSession_hpp__
#define __fon9_framework_SeedSession_hpp__
#include "fon9/seed/SeedVisitor.hpp"
#include "fon9/auth/AuthMgr.hpp"
#include "fon9/seed/SeedSearcher.hpp"
#include "fon9/buffer/DcQueue.hpp"

namespace fon9 {

class fon9_API SeedSession;
using SeedSessionSP = intrusive_ptr<SeedSession>;

fon9_WARN_DISABLE_PADDING;
/// \ingroup Misc
/// 提供給 console 或 telnet 的管理介面.
class fon9_API SeedSession : public seed::SeedVisitor {
   fon9_NON_COPY_NON_MOVE(SeedSession);
   using base = seed::SeedVisitor;

public:
   SeedSession(seed::MaTreeSP root, auth::AuthMgrSP authMgr, std::string ufrom);
   virtual ~SeedSession();

   /// 設為 Admin 模式, 僅能在建構後立即設定.
   void SetAdminMode();

   enum class State {
      None = 0,

      Authing = 1,
      UserReady = 10,
      Logouting = 20,

      AuthError = -1,
      Broken = -2,

      UserExit = 999,
      QuitApp = 998,
   };
   static bool IsRunningState(State st) {
      return State::Authing <= st && st <= State::Logouting;
   }
   State GetState() {
      St::Locker st{this->St_};
      return st->State_;
   }

   /// 檢查使用者認證(非同步).
   /// - 可能在返回前已認證完畢.
   /// - 透過 OnAuthEvent() 通知結果.
   /// - 若已認證(尚未登出) IsRunningState()==true, 則無法重新認證, 返回:
   ///   - 若現正在認證中, 返回 State::None
   ///   - 否則返回現在狀態.
   State AuthUser(StrView authz, StrView authc, StrView pass, StrView from);
   /// 透過 OnAuthEvent() 通知 Logout() 結果.
   State Logout();

   /// 匯入一行指令.
   /// 只能同時執行一個指令, 若有指令正在執行, 則會放在尾端排隊.
   /// 指令執行完畢會透過 OnRequestDone() 或 OnRequestError() 通知結果.
   State FeedLine(StrView cmdln);

protected:
   virtual void OnAuthEventInLocking(State st, DcQueue&& msg) = 0;
   virtual void OnRequestDone(const seed::TicketRunner& req, DcQueue&& extmsg) = 0;
   virtual void OnRequestError(const seed::TicketRunner& req, DcQueue&& errmsg) = 0;

   /// 預設傳回 25.
   virtual uint16_t GetDefaultGridViewRowCount();

   void SetCurrPath(StrView currPath) override;
   void GetPrompt(std::string& prompt) {
      St::Locker st{this->St_};
      prompt.assign(st->Prompt_);
   }
   const auth::AuthResult& GetAuthr() const {
      return this->Authr_;
   }

private:
   using Request = seed::TicketRunner;
   using RequestSP = intrusive_ptr<Request>;
   struct StateImpl {
      State       State_{State::None};
      std::string PendingCommandLines_;
      RequestSP   CurrRequest_;
      std::string Prompt_;
   };
   using St = MustLock<StateImpl>;
   St St_;
   auth::AuthResult  Authr_;

   struct PrintLayout;
   seed::TicketRunnerGridViewSP  LastGV_;

   void OnTicketRunnerDone(seed::TicketRunner&, DcQueue&& extmsg) override;
   void OnTicketRunnerWrite(seed::TicketRunnerWrite&, const seed::SeedOpResult& res, const seed::RawWr& wr) override;
   void OnTicketRunnerRead(seed::TicketRunnerRead&, const seed::SeedOpResult& res, const seed::RawRd& rd) override;
   void OnTicketRunnerRemoved(seed::TicketRunnerRemove&, const seed::PodRemoveResult& res) override;
   void OnTicketRunnerGridView(seed::TicketRunnerGridView&, seed::GridViewResult& res) override;
   void OnTicketRunnerCommand(seed::TicketRunnerCommand&, const seed::SeedOpResult& res, StrView msg) override;
   void OnTicketRunnerSetCurrPath(seed::TicketRunnerCommand&) override;
   void OnTicketRunnerSubscribe(seed::TicketRunnerSubscribe&, bool isSubOrUnsub) override;

   void EmitAuthEvent(State st, DcQueue&& msg);
   void OnAuthDone(auth::AuthR&& authr, const auth::AuthRequest& req);
   void ClearLogout(St::Locker&);

   void OutputSeedFields(seed::TicketRunner& runner, const seed::SeedOpResult& res, const seed::RawRd& rd, StrView exhead);
   void ExecuteCommand(St::Locker& st, StrView cmdln);
   RequestSP MakeRequest(StrView cmdln);
};
fon9_WARN_POP;

} // namespace
#endif//__fon9_framework_SeedSession_hpp__
