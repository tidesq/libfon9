/// \file fon9/auth/SaslScramSha256Client.cpp
/// \author fonwinz@gmail.com
#include "fon9/auth/SaslScramSha256Client.hpp"
#include "fon9/auth/SaslScramSha256.hpp"

namespace fon9 { namespace auth {

class ScramSha256Client : public SaslScramClient {
   using base = SaslScramClient;
   ScramSha256Client() = delete;

   void MakeClientFinalMessage(byte salt[], size_t saltSize, size_t iter, StrView authMessage, std::string& clientFinalMessage) override {
      SaslScramSha256::MakeClientFinalMessage(this->Pass_, salt, saltSize, iter, authMessage, clientFinalMessage, this->Verify_);
   }
public:
   using base::base;
};

fon9_API SaslClientSP SaslScramSha256ClientCreator(StrView authz, StrView authc, StrView pass) {
   return SaslClientSP{new ScramSha256Client{authz, authc, pass}};
}

} } // namespaces
