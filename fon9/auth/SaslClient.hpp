/// \file fon9/auth/SaslClient.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_SaslClient_hpp__
#define __fon9_auth_SaslClient_hpp__
#include "fon9/auth/AuthBase.hpp"
#include <memory>

namespace fon9 { namespace auth {

/// \ingroup auth
/// 提供 SASL Client 端的處理基底.
class fon9_API SaslClient {
protected:
   std::string ClientFirstMessage_;
public:
   virtual ~SaslClient();

   const std::string& GetFirstMessage() const {
      return this->ClientFirstMessage_;
   }

   /// message = server challenge message.
   virtual AuthR OnChallenge(StrView message) = 0;
};
using SaslClientSP = std::unique_ptr<SaslClient>;

struct SaslClientR {
   fon9_NON_COPYABLE(SaslClientR);
   SaslClientR() = delete;
   SaslClientR(SaslClientR&&) = default;
   SaslClientR& operator=(SaslClientR&&) = default;

   SaslClientR(StrView name, SaslClientSP&& cli) : MechName_{name}, SaslClient_{std::move(cli)} {
   }

   StrView        MechName_;
   SaslClientSP   SaslClient_;
};

/// \ingroup auth
/// 從 saslMechList 裡面選一個有支援的 sasl mech, 並建立 SaslClient.
fon9_API SaslClientR CreateSaslClient(StrView saslMechList, char chSplitter,
                                      StrView authz, StrView authc, StrView pass);

/// \ingroup auth
/// 提供 SASL SCRAM Client 端的基底.
/// - 不支援 channel binding.
/// - 不支援 SASLprep.
class fon9_API SaslScramClient : public SaslClient {
protected:
   std::string Pass_;
   std::string Verify_;

   // 計算 saltedPassword & proof & verify
   virtual void MakeClientFinalMessage(byte salt[], size_t saltSize, size_t iter, StrView authMessage, std::string& clientFinalMessage) = 0;

public:
   SaslScramClient(StrView authz, StrView authc, StrView pass);

   /// message = server challenge message.
   virtual AuthR OnChallenge(StrView message) override;
};

} } // namespaces
#endif//__fon9_auth_SaslClient_hpp__
