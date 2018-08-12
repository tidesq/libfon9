/// \file fon9/auth/UserMgr.cpp
/// \author fonwinz@gmail.com
#include "fon9/auth/UserMgr.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/BitvArchive.hpp"
#include "fon9/ThreadId.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace auth {

template <class Archive>
static void SerializeVer(Archive& ar, ArchiveWorker<Archive, UserRec>& rec, unsigned ver) {
   (void)ver; assert(ver == 0);
   ar(rec.Pass_.AlgParam_,
      rec.Pass_.Salt_,
      rec.Pass_.SaltedPass_,
      rec.RoleId_,
      rec.NotBefore_,
      rec.NotAfter_,
      rec.EvChgPass_.Time_,
      rec.EvChgPass_.From_,
      rec.EvLastAuth_.Time_,
      rec.EvLastAuth_.From_,
      rec.EvLastErr_.Time_,
      rec.EvLastErr_.From_,
      rec.ErrCount_,
      rec.UserFlags_
   );
}

template <class Archive>
static void UserRecSerialize(const Archive& ar, ArchiveWorker<Archive, UserRec>& rec) {
   CompositeVer<decltype(rec)> vrec{rec, 0};
   ar(vrec);
}

void UserRec::LoadPolicy(DcQueue& buf) {
   UserRecSerialize(BitvInArchive{buf}, *this);
}
void UserRec::SavePolicy(RevBuffer& rbuf) {
   UserRecSerialize(BitvOutArchive{rbuf}, *this);
}

void UserRec::OnSeedCommand(PolicyMaps::Locker& locker, seed::SeedOpResult& res, StrView cmdln, seed::FnCommandResultHandler resHandler) {
   // cmd:
   //    repw [NewPass]
   //
   StrView cmd = StrFetchTrim(cmdln, &isspace);
   StrTrim(&cmdln);
   if (cmd == "repw") {
      char  newpass[13];
      if (cmdln.empty()) {
         RandomString(newpass, sizeof(newpass));
         cmdln.Reset(newpass, newpass + sizeof(newpass) - 1);
      }
      static const char kCSTR_LOG_repw[] = "UserRec.OnSeedCommand.repw|userId=";
      auto& fnHashPass = static_cast<UserTree*>(res.Sender_)->FnHashPass_;
      PassRec passRec;
      if (!fnHashPass(cmdln.begin(), cmdln.size(), passRec, HashPassFlag::ResetAll)) {
         locker.unlock();
         res.OpResult_ = seed::OpResult::mem_alloc_error;
         resHandler(res, "FnHashPass");
         fon9_LOG_ERROR(kCSTR_LOG_repw, this->PolicyId_, "|err=FnHashPass()");
      }
      else {
         this->Pass_ = std::move(passRec);
         locker->WriteUpdated(*this);
         locker.unlock();
         std::string msg = "The new password is: ";
         cmdln.AppendTo(msg);
         res.OpResult_ = seed::OpResult::no_error;
         resHandler(res, &msg);
         fon9_LOG_INFO(kCSTR_LOG_repw, this->PolicyId_);
      }
      return;
   }
   locker.unlock();
   res.OpResult_ = seed::OpResult::not_supported_cmd;
   resHandler(res, cmd);
}

//--------------------------------------------------------------------------//

seed::Fields UserMgr::MakeFields() {
   seed::Fields fields;
   fields.Add(fon9_MakeField(Named{"RoleId"}, UserRec, RoleId_));

   fields.Add(fon9_MakeField_const(Named{"AlgParam"},   UserRec, Pass_.AlgParam_));
 //fields.Add(fon9_MakeField_const(Named{"Salt"},       UserRec, Pass_.Salt_));
 //fields.Add(fon9_MakeField_const(Named{"SaltedPass"}, UserRec, Pass_.SaltedPass_));

   fields.Add(fon9_MakeField(Named{"Flags"},      UserRec, UserFlags_));
   fields.Add(fon9_MakeField(Named{"NotBefore"},  UserRec, NotBefore_));
   fields.Add(fon9_MakeField(Named{"NotAfter"},   UserRec, NotAfter_));

   fields.Add(fon9_MakeField(Named{"ChgPassTime"},  UserRec, EvChgPass_.Time_));
   fields.Add(fon9_MakeField(Named{"ChgPassFrom"},  UserRec, EvChgPass_.From_));
   fields.Add(fon9_MakeField(Named{"LastAuthTime"}, UserRec, EvLastAuth_.Time_));
   fields.Add(fon9_MakeField(Named{"LastAuthFrom"}, UserRec, EvLastAuth_.From_));
   fields.Add(fon9_MakeField(Named{"LastErrTime"},  UserRec, EvLastErr_.Time_));
   fields.Add(fon9_MakeField(Named{"LastErrFrom"},  UserRec, EvLastErr_.From_));
   fields.Add(fon9_MakeField(Named{"ErrCount"},     UserRec, ErrCount_));
   return fields;
}

UserMgr::UserMgr(const seed::MaTree* authMgrAgents, std::string name, FnHashPass fnHashPass)
   : base(new UserTree(MakeFields(), std::move(fnHashPass)),
          std::move(name)) {
   (void)authMgrAgents;
}

//--------------------------------------------------------------------------//

UserTree::LockedUser UserTree::GetLockedUser(const AuthResult& uid) {
   Locker   container{this->PolicyMaps_};
   auto     ifind = container->ItemMap_.find(uid.GetUserId());
   if (ifind == container->ItemMap_.end())
      return LockedUser{};
   // TODO: 如何檢查 uid.AuthcId_ 可以使用 uid.AuthzId_?
   return LockedUser{std::move(container), static_cast<UserRec*>(ifind->get())};
}

AuthR UserTree::AuthUpdate(fon9_Auth_R rcode, const AuthRequest& req, AuthResult& authz, const PassRec* passRec, UserMgr& owner) {
   struct LogAux {
      fon9_NON_COPY_NON_MOVE(LogAux);
      LogArgs              LogArgs_{LogLevel::Warn};
      RevBufferList        RBuf_{sizeof(NumOutBuf) + 128};
      const AuthRequest&   Req_;
      const AuthResult&    Authz_;
      const UserMgr&       Owner_;
      LogAux(const AuthRequest& req, const AuthResult& authz, const UserMgr& owner)
         : Req_(req)
         , Authz_(authz)
         , Owner_(owner) {
         RevPutChar(this->RBuf_, '\n');
      }
      ~LogAux() {
         if (fon9::LogLevel_ > this->LogArgs_.Level_)
            return;
         if (!this->Authz_.AuthzId_.empty())
            RevPrint(this->RBuf_, "|authzId=", this->Authz_.AuthzId_);
         RevPrint(this->RBuf_, "UserMgr.AuthUpdate"
                  "|table=", this->Owner_.Name_,
                  "|from=", this->Req_.UserFrom_,
                  "|authcId=", this->Authz_.AuthcId_);
         RevPrint(this->RBuf_, this->LogArgs_.Time_, ThisThread_.GetThreadIdStr(), GetLevelStr(this->LogArgs_.Level_));
         LogWrite(this->LogArgs_, this->RBuf_.MoveOut());
      }
   };

   #define ERR_RETURN(rc, err) do { rcode = rc; errstr = err; goto __ERR_RETURN; } while(0)
   StrView          errstr;
   LogAux           aux{req, authz, owner};
   const TimeStamp  now = aux.LogArgs_.Time_;
   auto             lockedUser = this->GetLockedUser(authz);
   if (UserRec* user = lockedUser.second) {
      fon9_WARN_DISABLE_SWITCH;
      switch (rcode) {
      default:
         if (++user->ErrCount_ == 0)
            --user->ErrCount_;
         user->EvLastErr_.Time_ = now;
         user->EvLastErr_.From_ = req.UserFrom_;
         break;
      case fon9_Auth_PassChanged:
      case fon9_Auth_Success:
         if (!user->NotBefore_.IsNull() && now < user->NotBefore_)
            ERR_RETURN(fon9_Auth_ENotBefore, "not-before");
         if (!user->NotAfter_.IsNull() && user->NotAfter_ < now)
            ERR_RETURN(fon9_Auth_ENotBefore, "not-after");
         if (IsEnumContains(user->UserFlags_, UserFlags::Locked))
            ERR_RETURN(fon9_Auth_EUserLocked, "user-locked");
         if (rcode == fon9_Auth_PassChanged) {
            user->UserFlags_ -= UserFlags::NeedChgPass;
            user->EvChgPass_.Time_ = now;
            user->EvChgPass_.From_ = req.UserFrom_;
            user->Pass_ = *passRec;
         }
         else {
            if (IsEnumContains(user->UserFlags_, UserFlags::NeedChgPass))
               ERR_RETURN(fon9_Auth_ENeedChgPass, "need-change-pass");
            RevBufferList rbuf{128};
            RevPrint(rbuf, "Last logon: ",
                     user->EvLastAuth_.Time_, FmtTS{"K T+'L'"},
                     " from ", user->EvLastAuth_.From_);
            authz.ExtInfo_ = BufferTo<std::string>(rbuf.MoveOut());
            user->EvLastAuth_.Time_ = now;
            user->EvLastAuth_.From_ = req.UserFrom_;
         }
         user->ErrCount_ = 0;
         aux.LogArgs_.Level_ = LogLevel::Info;
         break;
      }
      fon9_WARN_POP;

      lockedUser.first->WriteUpdated(*user);
      lockedUser.first.unlock();
      if (rcode == fon9_Auth_PassChanged) { // 提示更改密碼?
         RevPrint(aux.RBuf_, "|info=PassChanged");
         return AuthR{fon9_Auth_PassChanged};
      }
      if (rcode == fon9_Auth_Success) // 密碼即將過期? 提示更改密碼?
         return AuthR{fon9_Auth_Success};
      RevPrint(aux.RBuf_, ":EPass");
   }
   else
      RevPrint(aux.RBuf_, ":EUser");
   ERR_RETURN(fon9_Auth_EUserPass, "invalid-proof(maybe UserId or Password error)");
#undef ERR_RETURN

__ERR_RETURN:
   RevPrint(aux.RBuf_, "|err=", errstr);
   return AuthR(rcode, errstr.ToString());
}

} } // namespaces
