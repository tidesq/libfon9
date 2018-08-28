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
typedef SaslClientSP (*FnSaslClientCreator) (const StrView& authz, const StrView& authc, const StrView& pass);
struct SaslClientMech_Password {
   StrView              Name_;
   FnSaslClientCreator  Creator_;
};
static SaslClientMech_Password SaslClientMech_Password_[]{
   {StrView{"SCRAM-SHA-256"}, &SaslScramSha256ClientCreator}
};

fon9_API SaslClientR CreateSaslClient(StrView saslMechList, char chSplitter,
                                      const StrView& authz, const StrView& authc, const StrView& pass) {
   for (SaslClientMech_Password& c : SaslClientMech_Password_) {
      if (StrSearchSubstr(saslMechList, c.Name_, chSplitter))
         return SaslClientR{c.Name_, c.Creator_(authz, authc, pass)};
   }
   return SaslClientR{StrView{}, SaslClientSP{}};
}

//--------------------------------------------------------------------------//

static constexpr size_t  kClientNonceLength = 24;
static constexpr size_t  kMaxSaltSize       = 128;

SaslScramClient::SaslScramClient(StrView authz, StrView authc, StrView pass)
   : Pass_{pass.ToString()} {
   if (authz.empty()) {
      this->ClientFirstMessage_ = "n,,n=";
      this->Gs2HeaderSize_ = 3; //"n,,"
   }
   else {
      this->ClientFirstMessage_ = "n,a="; // "n,a=authzid"
      authz.AppendTo(this->ClientFirstMessage_);
      this->Gs2HeaderSize_ = this->ClientFirstMessage_.size() + 1; // +1: include ','
      this->ClientFirstMessage_.append(",n=", 3);
   }
   authc.AppendTo(this->ClientFirstMessage_);

   this->ClientFirstMessage_.append(",r=", 3);
   const size_t szNoNonce = this->ClientFirstMessage_.size();
   this->ClientFirstMessage_.resize(szNoNonce + kClientNonceLength);
   RandomChars(&*this->ClientFirstMessage_.begin() + szNoNonce, kClientNonceLength);
}

const char* SaslScramClient::ParseAndCalcSaltedPass(StrView pass, const char* msgbeg, const char* msgend, std::string& saltedPass) {
   //  "s=SALT,i=N"
   if (msgend - msgbeg < 10)
      return nullptr;
   if (msgbeg[0] != 's' || msgbeg[1] != '=')
      return nullptr;
   msgbeg += 2;
   const char* pSaltEnd = StrView::traits_type::find(msgbeg, static_cast<size_t>(msgend - msgbeg), ',');
   if (!pSaltEnd)
      return nullptr;
   if (pSaltEnd[1] != 'i' || pSaltEnd[2] != '=')
      return nullptr;
   const char*   rEnd;
   const size_t  iterN = StrTo(StrView{pSaltEnd + 3, msgend}, static_cast<size_t>(0), &rEnd);
   byte  salt[kMaxSaltSize];
   auto  saltSize = Base64Decode(salt, sizeof(salt), msgbeg, static_cast<size_t>(pSaltEnd - msgbeg));
   if (!saltSize)
      return nullptr;
   saltedPass = this->CalcSaltedPassword(pass, salt, saltSize.GetResult(), iterN);
   return rEnd;
}

AuthR SaslScramClient::OnChallenge(StrView message) {
   fon9_Auth_R rcode = fon9_Auth_EArgs_Format;
   switch (message.Get1st()) {
   case 'r': // server_first_message = "r=ClientNonce" "ServerNonce" ",s=SALT,i=ITERATOR"
      if (const char* pNonceEnd = message.Find(',')) {
         if (this->ParseAndCalcSaltedPass(&this->Pass_, pNonceEnd + 1, message.end(), this->SaltedPass_) != message.end())
            break;
         // 建立 this->AuthMessage_ 及 clientFinalMessage.
         const char* const cliFirstMsg = this->ClientFirstMessage_.c_str();
         this->AuthMessage_.assign(cliFirstMsg + this->Gs2HeaderSize_, this->ClientFirstMessage_.size() - this->Gs2HeaderSize_);
         this->AuthMessage_.push_back(',');
         message.AppendTo(this->AuthMessage_);
         this->AuthMessage_.push_back(',');
         std::string clientFinalMessage;
         clientFinalMessage.reserve(message.size() * 3);
         if (this->Gs2HeaderSize_ == 3 && cliFirstMsg[0] == 'n' && cliFirstMsg[1] == ',' && cliFirstMsg[2] == ',')
            clientFinalMessage.assign("c=biws,", 7); // "biws" = Base64Encode("n,,");
         else {
            clientFinalMessage.assign("c=", 2);
            size_t e64sz = Base64EncodeLengthNoEOS(this->Gs2HeaderSize_);
            clientFinalMessage.resize(2 + e64sz);
            Base64Encode(&*(clientFinalMessage.begin() + 2), e64sz, cliFirstMsg, this->Gs2HeaderSize_);
            clientFinalMessage.push_back(',');
         }
         clientFinalMessage.append(message.begin(), pNonceEnd);// "r=" "ClientNonce" "ServerNonce"
         if (!this->NewPass_.empty()) // fon9 自己的擴充, for change password.
            clientFinalMessage.append(",s=", 3);
         this->AuthMessage_.append(clientFinalMessage);
         clientFinalMessage.append(",p=", 3);
         // 計算 proof.
         this->AppendProof(this->SaltedPass_, &this->AuthMessage_, clientFinalMessage);
         return AuthR(fon9_Auth_NeedsMore, std::move(clientFinalMessage));
      }
      break;
   case 's': // change pass: "s=NewSALT,i=NewITERATOR,v=verifier"
   {
      std::string newSaltedPass;
      if (const char* pIterEnd = this->ParseAndCalcSaltedPass(&this->NewPass_, message.begin(), message.end(), newSaltedPass)) {
         if (newSaltedPass.size() != this->SaltedPass_.size())
            return AuthR(fon9_Auth_EOther, "change pass: CalcSaltedPassword");
         // pIterEnd = ",v=..."
         if (message.end() - pIterEnd < 4)
            break;
         if (pIterEnd[0] != ',' || pIterEnd[1] != 'v' || pIterEnd[2] != '=')
            break;
         this->AuthMessage_.append(message.begin() + 2, pIterEnd); // 加上 "NewSALT,i=NewITERATOR"
         std::string v = this->MakeVerify(this->SaltedPass_, &this->AuthMessage_);
         pIterEnd += 3;
         if (v.size() != static_cast<size_t>(message.end() - pIterEnd)
         && memcmp(v.c_str(), pIterEnd, v.size()) != 0)
            return AuthR(fon9_Auth_EProof, "err verify for old pass");
         // 建立 client 改密碼訊息.
         std::string climsg;
         climsg.reserve(kMaxSaltSize * 3);
         climsg.assign("h=", 2);
         const char* pNewSalted = newSaltedPass.c_str();
         for (char& c : this->SaltedPass_)
            c = static_cast<char>(c ^ (*pNewSalted++));
         size_t encsz = Base64EncodeLengthNoEOS(newSaltedPass.size());
         climsg.resize(climsg.size() + encsz);
         Base64Encode(&*(climsg.end() - static_cast<int>(encsz)), encsz, this->SaltedPass_.c_str(), this->SaltedPass_.size());
         climsg.append(",p=");
         this->AppendProof(newSaltedPass, &this->AuthMessage_, climsg);
         this->SaltedPass_.swap(newSaltedPass);
         return AuthR(fon9_Auth_NeedsMore, std::move(climsg));
      }
   } // case 's'
   case 'h':
   case 'v':
   {
      std::string verifier = this->MakeVerify(this->SaltedPass_, &this->AuthMessage_);
      if (message.size() - 2 == verifier.size() && message.begin()[1] == '=') {
         if (memcmp(message.begin() + 2, verifier.c_str(), verifier.size()) == 0)
            return AuthR{message.Get1st() == 'h' ? fon9_Auth_PassChanged : fon9_Auth_Success};
         rcode = fon9_Auth_EProof;
      }
      break;
   } // case 'v'
   case 'e':
      rcode = fon9_Auth_EOther;
      break;
   }
   return AuthR(rcode, message.ToString());
}

} } // namespaces
