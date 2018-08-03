/// \file fon9/auth/SaslScramServer.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_SaslScramServer_hpp__
#define __fon9_auth_SaslScramServer_hpp__
#include "fon9/auth/UserMgr.hpp"

namespace fon9 { namespace auth {

/// \ingroup auth
/// 簡易 SASL SCRAM Server 端處理:
/// 不考慮 SASLprep 字串正規化的問題.
/// https://tools.ietf.org/html/rfc5802
/// - Client 1st message:
///   - "n,,n=USER,r=ClientNONCE"
///   - "n,,a=AUTHZ,n=USER,r=ClientNONCE"
///   - client_first_message_bare = 移除 "n,," 之後剩餘的字串.
/// - Server 1st message:
///   - "r=ClientNONCE+ServerNONCE,s=SALT,i=ITERATOR"
class fon9_API SaslScramServer : public AuthSession {
   fon9_NON_COPY_NON_MOVE(SaslScramServer);
   using base = AuthSession;

protected:
   const UserMgrSP   UserMgr_;
   std::string       AuthMessage_;
   SaltedPass        SaltedPass_;
   size_t            Step_{0};

   /// 當找不到 User 時, 透過這裡設定 Pass 的預設值.
   /// - passRec.AlgParam_ = ITERATOR; // for "i=ITERATOR"
   /// - RandomBytes(passRec.Salt_.alloc(kSaltBytes), kSaltBytes);
   virtual void SetDefaultPass(PassRec&) const = 0;

   virtual void AppendServerNonce(std::string& svr1stMsg) const = 0;

   /// \code
   ///   if (this->SaltedPass_.size() != SaslScramSha256::kOutputSize)
   ///      return std::string{};
   ///   return SaslScramSha256::MakeProof(this->SaltedPass_.begin(), &this->AuthMessage_);
   /// \endcode
   virtual std::string MakeProof() = 0;

   /// 必定在 MakeProof() 之後呼叫, 所以已經檢查過 this->SaltedPass_.size();
   /// \code
   ///   return SaslScramSha256::MakeVerify(this->SaltedPass_.begin(), &this->AuthMessage_);
   /// \endocde
   virtual std::string MakeVerify() = 0;

   bool ParseClientFirst(const AuthRequest& req);
   bool ProofError(const AuthRequest& req);
   bool ParseClientProof(const AuthRequest& req);

public:
   SaslScramServer(AuthMgr& authMgr, FnOnAuthVerifyCB&& cb, UserMgrSP userMgr)
      : AuthSession{authMgr, std::move(cb)}
      , UserMgr_(userMgr) {
   }

   void AuthVerify(const AuthRequest& req) override;
};

} } // namespaces
#endif//__fon9_auth_SaslScramServer_hpp__
