/// \file fon9/auth/SaslScramSha256Client.cpp
/// \author fonwinz@gmail.com
#include "fon9/auth/SaslScramSha256Client.hpp"
#include "fon9/auth/SaslScramSha256.hpp"

namespace fon9 { namespace auth {

class ScramSha256Client : public SaslScramClient {
   using base = SaslScramClient;
   ScramSha256Client() = delete;

   std::string CalcSaltedPassword(StrView pass, byte salt[], size_t saltSize, size_t iter) override {
      char saltedPassword[SaslScramSha256::kOutputSize];
      SaslScramSha256::CalcSaltedPassword(pass.begin(), pass.size(), salt, saltSize, iter, sizeof(saltedPassword), saltedPassword);
      return std::string{saltedPassword, sizeof(saltedPassword)};
   }
   void AppendProof(const std::string& saltedPass, const StrView& authMessage, std::string& out) override {
      SaslScramSha256::AppendProof(reinterpret_cast<const byte*>(saltedPass.c_str()), authMessage, out);
   }
   std::string MakeVerify(const std::string& saltedPass, const StrView& authMessage) override {
      return SaslScramSha256::MakeVerify(reinterpret_cast<const byte*>(saltedPass.c_str()), authMessage);
   }
public:
   using base::base;
};

fon9_API SaslClientSP SaslScramSha256ClientCreator(const StrView& authz, const StrView& authc, const StrView& pass) {
   return SaslClientSP{new ScramSha256Client{authz, authc, pass}};
}

fon9_API SaslClientSP SaslScramSha256ClientCreator_ChgPass(const StrView& authz, const StrView& authc, const StrView& oldPass, const StrView& newPass) {
   return SaslClientSP{new ScramSha256Client{authz, authc, oldPass, newPass}};
}

} } // namespaces
