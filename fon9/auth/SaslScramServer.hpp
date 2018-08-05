/// \file fon9/auth/SaslScramServer.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_SaslScramServer_hpp__
#define __fon9_auth_SaslScramServer_hpp__
#include "fon9/auth/UserMgr.hpp"

namespace fon9 { namespace auth {

fon9_WARN_DISABLE_PADDING;
/// \ingroup auth
/// 簡易 SASL SCRAM Server 端處理:
/// 不考慮 SASLprep 字串正規化的問題.
/// https://tools.ietf.org/html/rfc5802
class fon9_API SaslScramServer : public AuthSession {
   fon9_NON_COPY_NON_MOVE(SaslScramServer);
   using base = AuthSession;

   void AppendSaltAndIterator(std::string& out);
   bool CheckProof(StrView reqProof) {
      return (reqProof == ToStrView(this->MakeProof()));
   }
protected:
   const UserMgrSP   UserMgr_;
   std::string       AuthMessage_;
   PassRec           Pass_;
   unsigned          Step_{0};
   bool              IsChgPass_{false};

   /// 當找不到 User 或需要改密碼, 透過這裡設定 Pass 的預設值.
   /// - this->Pass_.AlgParam_ = ITERATOR; // for "i=ITERATOR"
   /// - RandomBytes(this->Pass_.Salt_.alloc(kSaltBytes), kSaltBytes);
   /// - 但不能改變 this->Pass_.SaltedPass_
   virtual void SetDefaultPass() = 0;

   virtual void AppendServerNonce(std::string& svr1stMsg) const = 0;

   /// \code
   ///   if (this->Pass_.SaltedPass_.size() != SaslScramSha256::kOutputSize)
   ///      return std::string{};
   ///   return SaslScramSha256::MakeProof(this->Pass_.SaltedPass_.begin(), &this->AuthMessage_);
   /// \endcode
   virtual std::string MakeProof() = 0;

   /// 必定在 MakeProof() 之後呼叫, 所以已經檢查過 this->Pass_.SaltedPass_.size();
   /// \code
   ///   return SaslScramSha256::MakeVerify(this->Pass_.SaltedPass_.begin(), &this->AuthMessage_);
   /// \endocde
   virtual std::string MakeVerify() = 0;

   void ParseClientFirst(const AuthRequest& req);
   void ProofError(const AuthRequest& req);
   void ParseClientProof(const AuthRequest& req);
   void ParseChangePass(const AuthRequest& req);

public:
   SaslScramServer(AuthMgr& authMgr, FnOnAuthVerifyCB&& cb, UserMgrSP userMgr)
      : AuthSession{authMgr, std::move(cb)}
      , UserMgr_(userMgr) {
   }

   void AuthVerify(const AuthRequest& req) override;
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_auth_SaslScramServer_hpp__
