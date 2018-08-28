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
   std::string ClientFirstMessage_; //= gs2-header client-first-message-bare
   size_t      Gs2HeaderSize_;
public:
   virtual ~SaslClient();

   const std::string& GetFirstMessage() const {
      return this->ClientFirstMessage_;
   }
   void SetFirstMessage(std::string v, size_t gs2HeaderSize) {
      this->ClientFirstMessage_ = std::move(v);
      this->Gs2HeaderSize_ = gs2HeaderSize;
   }
   size_t GetGs2HeaderSize() const {
      return this->Gs2HeaderSize_;
   }

   /// message = server challenge message.
   virtual AuthR OnChallenge(StrView message) = 0;
};
using SaslClientSP = std::unique_ptr<SaslClient>;

/// \ingroup auth
/// 透過 CreateSaslClient() 所建立的 SaslClient.
/// 用來協助得知選用的是哪種 SASL 認證機制.
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
                                      const StrView& authz, const StrView& authc, const StrView& pass);

/// \ingroup auth
/// 提供 SASL SCRAM Client 端的基底.
/// - 不支援 channel binding.
/// - 不支援 SASLprep.
/// https://tools.ietf.org/html/rfc5802
class fon9_API SaslScramClient : public SaslClient {
   std::string Pass_;
   std::string NewPass_;
   std::string AuthMessage_;
   std::string SaltedPass_;

   // 取出 msg = "s=SALT,i=ITERATOR" 計算 SaltedPass
   // 返回 ITERATOR 的尾端, 或 nullptr(表示訊息有誤).
   const char* ParseAndCalcSaltedPass(StrView pass, const char* msgbeg, const char* msgend, std::string& saltedPass);

protected:
   virtual std::string CalcSaltedPassword(StrView pass, byte salt[], size_t saltSize, size_t iter) = 0;
   virtual void AppendProof(const std::string& saltedPass, const StrView& authMessage, std::string& out) = 0;
   virtual std::string MakeVerify(const std::string& saltedPass, const StrView& authMessage) = 0;

public:
   /// 若 authz.empty()  則會提供: "n,,n=authc,r=nonce"
   /// 若 !authz.empty() 則會提供: "n,,a=authz,n=authc,r=nonce"
   SaslScramClient(StrView authz, StrView authc, StrView pass);
   SaslScramClient(StrView authz, StrView authc, StrView oldPass, StrView newPass)
      : SaslScramClient(authz, authc, oldPass) {
      this->NewPass_ = newPass.ToString();
   }

   /// message = server challenge message.
   virtual AuthR OnChallenge(StrView message) override;
};

} } // namespaces
#endif//__fon9_auth_SaslClient_hpp__
