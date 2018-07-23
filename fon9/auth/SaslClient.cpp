/// \file fon9/auth/SaslClient.cpp
/// \author fonwinz@gmail.com
#include "fon9/auth/SaslClient.hpp"
#include "fon9/auth/SaslScramSha256Client.hpp"
#include "fon9/Random.hpp"
#include "fon9/StrTools.hpp"
#include "fon9/StrTo.hpp"
#include "fon9/Base64.hpp"

namespace fon9 { namespace auth {

SaslClient::~SaslClient() {
}

//--------------------------------------------------------------------------//

// 使用 Password 機制的 SaslClient 定義.
typedef SaslClientSP (*FnSaslClientCreator) (StrView authz, StrView authc, StrView pass);
struct SaslClientMech_Password {
   StrView              Name_;
   FnSaslClientCreator  Creator_;
};
static SaslClientMech_Password SaslClientMech_Password_[]{
   {StrView{"SCRAM-SHA-256"}, &SaslScramSha256ClientCreator}
};

fon9_API SaslClientR CreateSaslClient(StrView saslMechList, char chSplitter,
                                      StrView authz, StrView authc, StrView pass) {
   for (SaslClientMech_Password& c : SaslClientMech_Password_) {
      if (SearchSubstr(saslMechList, c.Name_, chSplitter))
         return SaslClientR{c.Name_, c.Creator_(authz, authc, pass)};
   }
   return SaslClientR{StrView{}, SaslClientSP{}};
}

//--------------------------------------------------------------------------//

static const size_t  kClientNonceLength = 24;

SaslScramClient::SaslScramClient(StrView authz, StrView authc, StrView pass)
   : Pass_{pass.ToString()} {
   if (authz.empty())
      this->ClientFirstMessage_ = "n,,n=";
   else {
      this->ClientFirstMessage_ = "n,,a=";
      authz.AppendTo(this->ClientFirstMessage_);
      this->ClientFirstMessage_.append(",n=", 3);
   }
   authc.AppendTo(this->ClientFirstMessage_);

   this->ClientFirstMessage_.append(",r=", 3);
   const size_t szNoNonce = this->ClientFirstMessage_.size();
   this->ClientFirstMessage_.resize(szNoNonce + kClientNonceLength);
   RandomChars(&*this->ClientFirstMessage_.begin() + szNoNonce, kClientNonceLength);
}

AuthR SaslScramClient::OnChallenge(StrView message) {
   fon9_Auth_R rcode = fon9_Auth_EArgs_Format;
   switch (message.Get1st()) {
   case 'r': // server_first_message = "r=ClientNonce" "ServerNonce" ",s=SALT,i=ITERATOR"
      if (const char* pSalt = StrView::traits_type::find(message.begin(), message.size(), ',')) {
         const char* const mend = message.end();
         if (mend - pSalt < 11) // ",s=SALT,i=N"
            break;
         const char* pNonceEnd = pSalt;
         if (*++pSalt != 's' || *++pSalt != '=')
            break;
         ++pSalt;
         const char* pSaltEnd = StrView::traits_type::find(pSalt, static_cast<size_t>(mend - pSalt), ',');
         if (!pSaltEnd)
            break;
         if (mend - pSaltEnd < 4) // ",i=x"
            break;
         if (pSaltEnd[1] != 'i' || pSaltEnd[2] != '=')
            break;
         StrView        sp{pSaltEnd + 3, message.end()};
         const size_t   iter = StrTo(sp, static_cast<size_t>(0));
         if (!sp.empty())
            break;
         byte  salt[128];
         auto  saltSize = Base64Decode(salt, sizeof(salt), pSalt, static_cast<size_t>(pSaltEnd - pSalt));
         if (!saltSize)
            break;
         // 計算 proof.
         const char* cliFirstMsg = this->ClientFirstMessage_.c_str();
         if (cliFirstMsg[0] != 'n' || cliFirstMsg[1] != ',' || cliFirstMsg[2] != ',') // 移除 "n,,"
            break;
         std::string authMessage{cliFirstMsg + 3, this->ClientFirstMessage_.size() - 3};
         authMessage.push_back(',');
         message.AppendTo(authMessage);
         authMessage.push_back(',');
         std::string clientFinalMessage{"c=biws,"};// "biws" = Base64Encode("n,,");
         clientFinalMessage.append(message.begin(), pNonceEnd);// "r=" "ClientNonce" "ServerNonce"
         authMessage.append(clientFinalMessage);
         clientFinalMessage.append(",p=", 3);
         this->MakeClientFinalMessage(salt, saltSize.GetResult(), iter, &authMessage, clientFinalMessage);
         return AuthR(fon9_Auth_NeedsMore, std::move(clientFinalMessage));
      }
      break;
   case 'v':
      if (message.size() - 2 == this->Verify_.size() && message.begin()[1] == '=') {
         if (memcmp(message.begin() + 2, this->Verify_.c_str(), this->Verify_.size()) == 0)
            return AuthR{fon9_Auth_Success};
         rcode = fon9_Auth_EProof;
      }
      break;
   case 'e':
      rcode = fon9_Auth_EOther;
      break;
   }
   return AuthR(rcode, message.ToString());
}

} } // namespaces
