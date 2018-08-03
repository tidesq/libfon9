/// \file fon9/auth/SaslScramSha256Server.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_SaslScramSha256Server_hpp__
#define __fon9_auth_SaslScramSha256Server_hpp__
#include "fon9/auth/UserMgr.hpp"
#include "fon9/auth/SaslScramSha256.hpp"
#include "fon9/auth/SaslScramServer.hpp"

namespace fon9 { namespace auth {

/// \ingroup auth
/// - 在 authMgr->Agents_ 加上 AA_SCRAM_SHA_256 的支援.
/// - 加上 UserSha256 的使用者資料表.
fon9_API UserMgrSP PlantScramSha256(AuthMgr& authMgr);

//--------------------------------------------------------------------------//
// 底下是為了提供給 impl & unit test, 一般不會直接使用.

enum SaslScramSha256Param : size_t {
   kSaltBytes = 18,
   kAlgParam = 10000,
};

/// 簡易 SASL SCRAM-SHA-256 處理:
/// 不考慮 SASLprep 字串正規化的問題.
class AuthSession_SaslScramSha256 : public SaslScramServer {
   fon9_NON_COPY_NON_MOVE(AuthSession_SaslScramSha256);
   using base = SaslScramServer;

   void SetDefaultPass(PassRec& passRec) const override {
      passRec.AlgParam_ = SaslScramSha256Param::kAlgParam;
      RandomBytes(passRec.Salt_.alloc(SaslScramSha256Param::kSaltBytes), SaslScramSha256Param::kSaltBytes);
   }
   void AppendServerNonce(std::string& svr1stMsg) const override {
      const size_t cursz = svr1stMsg.size();
      svr1stMsg.resize(cursz + 30);
      RandomChars(&*svr1stMsg.begin() + cursz, 30);
   }
   std::string MakeProof() override {
      if (this->SaltedPass_.size() != SaslScramSha256::kOutputSize)
         return std::string{};
      return SaslScramSha256::MakeProof(this->SaltedPass_.begin(), &this->AuthMessage_);
   }
   std::string MakeVerify() override {
      return SaslScramSha256::MakeVerify(this->SaltedPass_.begin(), &this->AuthMessage_);
   }

public:
   using base::base;
};

} } // namespaces
#endif//__fon9_auth_SaslScramSha256Server_hpp__
