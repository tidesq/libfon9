/// \file fon9/auth/SaslScramSha256Server.cpp
/// \author fonwinz@gmail.com
#include "fon9/auth/SaslScramSha256Server.hpp"

namespace fon9 { namespace auth {

class AA_SCRAM_SHA_256 : public AuthAgent {
   fon9_NON_COPY_NON_MOVE(AA_SCRAM_SHA_256);
   using base = AuthAgent;
public:
   UserMgrSP UserMgr_;
   AA_SCRAM_SHA_256(const seed::MaTreeSP& /*maTree*/, std::string name, UserMgrSP userMgr)
      : base{std::move(name)}
      , UserMgr_{std::move(userMgr)} {
   }
   AuthSessionSP CreateAuthSession(AuthMgrSP authMgr, FnOnAuthVerifyCB cb) override {
      return new AuthSession_SaslScramSha256(authMgr, std::move(cb), this->UserMgr_);
   }
};

fon9_API UserMgrSP PlantScramSha256(AuthMgr& authMgr) {
   if (UserMgrSP userMgr = UserMgr::Plant(authMgr, "UserSha256", &PassRec::HashPass<crypto::Sha256, SaslScramSha256Param>)) {
      if (authMgr.Agents_->Plant<AA_SCRAM_SHA_256>("AA_SCRAM_SHA_256.Plant", "AA_SCRAM_SHA_256", userMgr))
         return userMgr;
      authMgr.Agents_->Remove(&userMgr->Name_);
   }
   return nullptr;
}

} } // namespaces
