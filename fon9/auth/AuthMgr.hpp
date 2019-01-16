/// \file fon9/auth/AuthMgr.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_AuthMgr_hpp__
#define __fon9_auth_AuthMgr_hpp__
#include "fon9/auth/RoleMgr.hpp"

namespace fon9 { namespace auth {

/// \ingroup auth
/// 認證結果.
struct fon9_API AuthResult : public RoleConfig {
   AuthMgrSP   AuthMgr_;
   /// manager? guest? trader?
   /// - 例如執行 sudo, 這裡用的是 "root"
   /// - 例如執行主管強迫, 這裡用的是主管Id.
   UserId      AuthzId_;
   /// Tony? Alice? Bob?
   /// - 例如執行 sudo, 這裡用的是原本登入時的 UserId
   /// - 例如執行主管強迫, 這裡用的是營業員(or Keyin)的Id.
   UserId      AuthcId_;
   /// - 認證成功後的提示訊息: e.g. "Last login: 2017/09/08 10:50:55 from 192.168.1.3";
   std::string ExtInfo_;

   AuthResult(AuthMgrSP authMgr) : AuthMgr_{std::move(authMgr)} {
   }
   AuthResult(AuthMgrSP authMgr, UserId authcId, RoleConfig roleCfg)
      : RoleConfig(std::move(roleCfg))
      , AuthMgr_{std::move(authMgr)}
      , AuthcId_{std::move(authcId)} {
   }

   void Clear() {
      this->RoleId_.clear();
      this->PolicyKeys_.clear();
      this->AuthzId_.clear();
      this->AuthcId_.clear();
      this->ExtInfo_.clear();
   }

   /// 若 this->AuthzId_.empty()  返回: AuthcId_
   /// 若 !this->AuthzId_.empty() 返回: AuthzId_
   StrView GetUserId() const {
      return this->AuthzId_.empty() ? ToStrView(this->AuthcId_) : ToStrView(this->AuthzId_);
   }

   /// 若 this->AuthzId_.empty()  輸出: AuthcId_
   /// 若 !this->AuthzId_.empty() 輸出: AuthzId_ + "/" + AuthcId_
   void RevPrintUser(RevBuffer& rbuf) const;
   /// 輸出: RevPrintUser() + "|" + deviceId;
   void RevPrintUFrom(RevBuffer& rbuf, StrView deviceId) const;
   /// 輸出: RevPrintUser() + "|" + deviceId;
   std::string MakeUFrom(StrView deviceId) const;

   /// 登入成功後, 使用 AuthResult 的人必須自行決定是否需要更新 RoleConfig.
   /// 因為有些登入只想要驗證 userid/pass 是否正確.
   void UpdateRoleConfig();

   /// 若 policyName 在 this->PolicyKeys_ 找不到,
   /// 則應直接用 RoleId_ 當作該 policyName 的 PolicyId.
   StrView GetPolicyId(StrView policyName) const;
};

/// \ingroup auth
/// 使用者認證要求
struct AuthRequest {
   /// 依據認證機制有不同的涵義:
   /// - SASL PLAIN: authzid<EOS>authcid<EOS>password
   /// - SASL SCRAM: 依協商階段提供不同的訊息.
   std::string Response_;
   /// 使用者來自哪裡?
   /// - "console"
   /// - "R=ip:port|L=ip:port"
   std::string UserFrom_;
};

//--------------------------------------------------------------------------//

/// \ingroup auth
/// 使用者認證結果 callback.
using FnOnAuthVerifyCB = std::function<void(AuthR authr, AuthSessionSP authSession)>;

fon9_WARN_DISABLE_PADDING;
/// \ingroup auth
/// 認證處理階段.
/// - 一般而言, 每個認證要求, 都會有一個 AuthSession 來負責處理每個認證步驟.
/// - 當 FnOnAuthVerifyCB 收到 fon9_Auth_NeedsMore 時,
///   取得後續的 response 之後, 再包成 AuthRequest 丟給 AuthVerify() 處理.
class fon9_API AuthSession : public intrusive_ref_counter<AuthSession> {
   fon9_NON_COPY_NON_MOVE(AuthSession);
protected:
   const FnOnAuthVerifyCB  OnVerifyCB_;
   AuthResult              AuthResult_;
public:
   AuthSession(AuthMgrSP authMgr, FnOnAuthVerifyCB&& cb)
      : OnVerifyCB_(std::move(cb)), AuthResult_(std::move(authMgr)) {
   }
   virtual ~AuthSession();

   /// 當收到 client 的 request 時, 透過這裡處理.
   /// 如果認證過程, 有多個步驟, 一樣透過這裡處理.
   virtual void AuthVerify(const AuthRequest& req) = 0;

   /// 僅保證在 FnOnAuthVerifyCB 事件, 或認證結束後, 才能安全的取得及使用.
   /// 在認證處理的過程中, 不應該呼叫此處.
   /// 一旦認證結束, 此處的資料都不會再變動(包含 ExtInfo_ 也不會變).
   AuthResult& GetAuthResult() {
      return this->AuthResult_;
   }
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

/// \ingroup auth
/// 實際執行認證的元件基底.
/// 例如: AuthAgentSaslScramSha256Server.cpp
class fon9_API AuthAgent : public seed::NamedSeed {
   fon9_NON_COPY_NON_MOVE(AuthAgent);
   using base = seed::NamedSeed;
public:
   using base::base;

   virtual AuthSessionSP CreateAuthSession(AuthMgrSP authMgr, FnOnAuthVerifyCB cb) = 0;
};
using AuthAgentSP = intrusive_ptr<AuthAgent>;

//--------------------------------------------------------------------------//

/// \ingroup auth
/// 使用者 **認證&授權** 管理員.
/// - 一個 AuthMgr 可以有多個認證代理員(AuthAgent), 可提供多種認證方式.
///   - 盡量使用 SASL 規範.
///   - AgentName = fon9_kCSTR_AuthAgent_Prefix + Mechanism Name
///     - 若為 SASL 規範的機制, 則將其中的 '-' 改成 '_'
///     - 例如: "AA_PLAIN"; "AA_SCRAM_SHA_256";
/// - 因此 AuthMgr 必須:
///   - 包含一個 RoleMgr
///   - Policy 的管理.
/// - 所以 AuthMgr::Agents_ 包含了: AuthAgents, RoleMgr, Policies, OnlineUserMgr...
class fon9_API AuthMgr : public seed::NamedSeed {
   fon9_NON_COPY_NON_MOVE(AuthMgr);
   using base = seed::NamedSeed;

public:
   /// 擁有此 AuthMgr 的管理員.
   const seed::MaTreeSP MaRoot_;
   /// 包含 UserMgrs, Role, Policies...
   const seed::MaTreeSP Agents_;
   /// 負責儲存 Agents 所需的資料.
   const InnDbfSP       Storage_;
   const RoleMgrSP      RoleMgr_;

   AuthMgr(seed::MaTreeSP ma, std::string name, InnDbfSP storage);
   virtual seed::TreeSP GetSapling() override;

   #define fon9_kCSTR_AuthAgent_Prefix    "AA_"
   /// 建立一個處理認證協商的物件.
   /// - "AA_PLAIN" 或 "PLAIN" = SASL "PLAIN"
   /// - "AA_SCRAM_SHA_256" 或 "SCRAM_SHA_256" = SASL "SCRAM-SHA-256"
   /// - SCRAM_ARGON2?
   AuthSessionSP CreateAuthSession(StrView mechanismName, FnOnAuthVerifyCB cb);

   /// 取得 SASL 機制列表: 使用 SASL 的命名慣例.
   std::string GetSaslMechList(char chSpl = ' ') const;

   template <class PolicyConfig, class PolicyAgent = typename PolicyConfig::PolicyAgent>
   bool GetPolicy(StrView agentName, const AuthResult& authr, PolicyConfig& res) const {
      if (auto agent = this->Agents_->Get<PolicyAgent>(agentName))
         return agent->GetPolicy(authr, res);
      return false;
   }
   template <class PolicyAgent, class PolicyConfig = typename PolicyAgent::PolicyConfig>
   bool GetPolicy(StrView agentName, const AuthResult& authr, PolicyConfig& res) const {
      if (auto agent = this->Agents_->Get<PolicyAgent>(agentName))
         return agent->GetPolicy(authr, res);
      return false;
   }

   /// 在 ma 上面新增一個 AuthMgr.
   /// \retval nullptr   name 已存在.
   /// \retval !=nullptr 新增到 ma 的 AuthMgr 物件.
   static AuthMgrSP Plant(seed::MaTreeSP ma, InnDbfSP storage, std::string name) {
      return ma->Plant<AuthMgr>("AuthMgr.Plant", std::move(name), std::move(storage));
   }
};

} } // namespaces
#endif//__fon9_auth_AuthMgr_hpp__
