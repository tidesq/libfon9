/// \file fon9/auth/AuthMgr.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_AuthMgr_hpp__
#define __fon9_auth_AuthMgr_hpp__
#include "fon9/auth/AuthBase.hpp"
#include "fon9/seed/MaTree.hpp"
#include "fon9/CharVector.hpp"
#include "fon9/InnDbf.hpp"

namespace fon9 { namespace auth {

class fon9_API AuthMgr;
using AuthMgrSP = intrusive_ptr<AuthMgr>;

class fon9_API AuthSession;
using AuthSessionSP = intrusive_ptr<AuthSession>;

using PolicyName = CharVector;
using PolicyId = CharVector;
using RoleId = PolicyId;
using UserId = PolicyId;

//--------------------------------------------------------------------------//

/// \ingroup auth
/// 登入成功後根據 RoleId 取得的使用者政策列表.
using PolicyKeys = std::map<PolicyName, PolicyId>;

/// \ingroup auth
/// 使用者的角色設定.
struct RoleConfig {
   RoleId      RoleId_;
   std::string RoleName_;
   PolicyKeys  PolicyKeys_;
};

/// \ingroup auth
/// 認證結果.
struct AuthResult : public RoleConfig {
   fon9_NON_COPY_NON_MOVE(AuthResult);

   AuthMgr& AuthMgr_;
   /// manager? guest? trader?
   /// - 例如執行 sudo, 這裡用的是 "root"
   /// - 例如執行主管強迫, 這裡用的是主管Id.
   UserId   AuthzId_;
   /// Tony? Alice? Bob?
   /// - 例如執行 sudo, 這裡用的是原本登入時的 UserId
   /// - 例如執行主管強迫, 這裡用的是營業員(or Keyin)的Id.
   UserId   AuthcId_;
   /// - 登入成功後的提示訊息: e.g. "Last login: 2017/09/08 10:50:55 from 192.168.1.3";
   std::string ExtInfo_;

   AuthResult(AuthMgr& authMgr) : AuthMgr_(authMgr) {
   }
   AuthResult(AuthMgr& authMgr, UserId authcId, RoleConfig roleCfg)
      : RoleConfig(std::move(roleCfg))
      , AuthMgr_(authMgr)
      , AuthcId_{std::move(authcId)} {
   }
};

/// \ingroup auth
/// 使用者認證要求
struct AuthRequest {
   /// 依據認證機制有不同的涵義:
   /// - SASL PLAIN: authzid<EOS>authcid<EOS>password
   /// - SASL SCRAM: 依協商階段提供不同的訊息.
   std::string Response_;
   /// 使用者來自哪裡?
   /// - "|console"
   /// - "|R=ip:port|L=ip:port"
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
   AuthSession(AuthMgr& authMgr, FnOnAuthVerifyCB&& cb)
      : OnVerifyCB_(std::move(cb)), AuthResult_(authMgr) {
   }
   virtual ~AuthSession();
   virtual void AuthVerify(const AuthRequest& req) = 0;

   /// 僅保證在 FnOnAuthVerifyCB 事件, 或登入結束後, 才能安全的取得.
   /// 在登入處理的過程中, 不應該呼叫此處.
   const AuthResult& GetAuthResult() const {
      return this->AuthResult_;
   }
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

/// \ingroup auth
/// 實際執行認證的元件基底.
/// 例如: AuthAgent_SCRAM_SHA_256.cpp
class fon9_API AuthAgent : public seed::NamedSeed {
   fon9_NON_COPY_NON_MOVE(AuthAgent);
   using base = seed::NamedSeed;
public:
   using base::base;

   virtual AuthSessionSP CreateAuthSession(AuthMgr& authMgr, FnOnAuthVerifyCB cb) = 0;
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
   using Agents = seed::MaTreeSP;
   Agents               Agents_;
   const seed::MaTreeSP MaRoot_;

   AuthMgr(seed::MaTreeSP ma, std::string name)
      : base{name}
      , Agents_{new seed::MaTree{"Agents"}}
      , MaRoot_{std::move(ma)} {
   }
   virtual seed::TreeSP GetSapling() override;

   #define fon9_kCSTR_AuthAgent_Prefix    "AA_"
   /// 建立一個處理認證協商的物件.
   /// - "AA_PLAIN" 或 "PLAIN" = SASL "PLAIN"
   /// - "AA_SCRAM_SHA_256" 或 "SCRAM_SHA_256" = SASL "SCRAM-SHA-256"
   /// - SCRAM_ARGON2?
   AuthSessionSP CreateAuthSession(StrView mechanismName, FnOnAuthVerifyCB cb);

   /// 取得 SASL 機制列表: 使用 SASL 的命名慣例.
   std::string GetSaslMechList(char chSpl = ' ') const;

   #define fon9_kCSTR_AuthMgr_DefaultName "MaAuth"
   /// 在 ma 上面新增一個 AuthMgr.
   /// \retval nullptr   name 已存在.
   /// \retval !=nullptr 新增到 ma 的 AuthMgr 物件.
   static AuthMgrSP Plant(seed::MaTreeSP ma, std::string name = fon9_kCSTR_AuthMgr_DefaultName) {
      return ma->Plant<AuthMgr>("AuthMgr.Plant", std::move(name));
   }
};

} } // namespaces
#endif//__fon9_auth_AuthMgr_hpp__
