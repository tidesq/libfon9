/// \file fon9/framework/SeedSession.cpp
/// \author fonwinz@gmail.com
#ifndef __fon9_framework_SeedSession_hpp__
#define __fon9_framework_SeedSession_hpp__
#include "fon9/seed/SeedFairy.hpp"
#include "fon9/auth/AuthMgr.hpp"
#include "fon9/seed/SeedSearcher.hpp"
#include "fon9/buffer/DcQueue.hpp"

namespace fon9 {

class fon9_API SeedSession;
using SeedSessionSP = intrusive_ptr<SeedSession>;

fon9_WARN_DISABLE_PADDING;
/// \ingroup Misc
/// 提供給 console 或 telnet 的管理介面.
class fon9_API SeedSession : public intrusive_ref_counter<SeedSession> {
   fon9_NON_COPY_NON_MOVE(SeedSession);

public:
   SeedSession(seed::MaTreeSP root, auth::AuthMgrSP authMgr, bool isAdminMode);
   virtual ~SeedSession();

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
   /// 指令執行完畢會透過 OnRequestDone() 通知結果.
   State FeedLine(StrView cmdln);

protected:
   struct fon9_API RequestBase {
      fon9_NON_COPY_NON_MOVE(RequestBase);
      const SeedSessionSP  Session_;
      seed::MaTree*        Root_;
      seed::AclPath        AclPath_;
      size_t               ErrPos_; // AclPath_.begin() + ErrPos_ = 發生錯誤的位置.
      seed::OpResult       OpResult_;
      RequestBase(SeedSession* owner, StrView& seed, seed::AccessRight needsRights);
      RequestBase(SeedSession* owner, seed::OpResult errn, StrView errmsg)
         : Session_{owner}
         , Root_{nullptr}
         , AclPath_{errmsg}
         , ErrPos_{errmsg.size() + 1}
         , OpResult_{errn} {
      }
   };
   class fon9_API Request : public RequestBase, public seed::SeedSearcher {
      fon9_NON_COPY_NON_MOVE(Request);
      using base = seed::SeedSearcher;
   public:
      Request(SeedSession* owner, StrView seed, seed::AccessRight needsRights)
         : RequestBase{owner, seed, needsRights}
         , base{seed} {
      }
      Request(SeedSession* owner, seed::OpResult errn, const StrView& errmsg)
         : RequestBase{owner, errn, errmsg}
         , base{StrView{}} {
      }
      void OnError(seed::OpResult res) override;
      void OnLastStep(seed::TreeOp& op, StrView keyText, seed::Tab& tab) override;
      virtual void OnLastSeedOp(const seed::PodOpResult& resPod, seed::PodOp* pod, seed::Tab& tab) = 0;
   };
   using RequestSP = intrusive_ptr<Request>;

   virtual void OnAuthEventInLocking(State st, DcQueue&& msg) = 0;
   virtual void OnRequestDone(const Request& req, DcQueue&& extmsg) = 0;
   virtual void OnRequestError(const Request& req, DcQueue&& errmsg) = 0;
   /// 預設傳回 25.
   virtual uint16_t GetDefaultGridViewRowCount();
   void SetCurrPathToHome() {
      this->UpdatePrompt(ToStrView(this->Fairy_->Ac_.Home_));
   }
   void GetPrompt(std::string& prompt) {
      St::Locker st{this->St_};
      prompt.assign(st->Prompt_);
   }

private:
   struct StateImpl {
      State       State_{State::None};
      std::string CommandLines_;
      RequestSP   CurrRequest_;
      std::string Prompt_;
   };
   using St = MustLock<StateImpl>;
   St St_;
   const seed::SeedFairySP Fairy_;
   auth::AuthResult        Authr_;

   struct RequestOpError;
   struct RequestTree;
   struct SetSeedFields;
   struct PrintSeed;
   struct RemoveSeed;
   struct PrintLayout;
   struct GridView;
   using GridViewSP = intrusive_ptr<GridView>;
   GridViewSP  LastGV_;

   void SetAdminMode();
   void UpdatePrompt(StrView currPath);
   void EmitRequestDone(RequestSP req, DcQueue&& extmsg);
   void EmitAuthEvent(State st, DcQueue&& msg);
   void OnAuthDone(auth::AuthR&& authr);
   void ClearLogout(St::Locker&);

   void OutputSeedFields(RequestSP req, const seed::SeedOpResult& res, const seed::RawRd& rd, StrView exhead);
   void ExecuteCommand(St::Locker& st, StrView cmdln);
   RequestSP MakeRequest(StrView cmdln);
   RequestSP MakeSeedCommandRequest(StrView seed, StrView cmdln);
   RequestSP MakeCommandRequest(StrView cmd, StrView seed);
};
fon9_WARN_POP;

} // namespace
#endif//__fon9_framework_SeedSession_hpp__
