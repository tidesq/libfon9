/// \file fon9/auth/SaslScramServer.cpp
/// \author fonwinz@gmail.com
#include "fon9/auth/SaslScramServer.hpp"
#include "fon9/Base64.hpp"
#include "fon9/ToStr.hpp"

namespace fon9 { namespace auth {

bool SaslScramServer::ParseClientFirst(const AuthRequest& req) {
   if (++this->Step_ != 1)
      return false;
   // 由於使用 std::string 建構, 所以必定有 EOS.
   // 因此底下的 *pbeg 判斷都不用再檢查長度是否足夠.
   StrView reqstr{&req.Response_};
   // 僅支援 "n,,"
   switch (reqstr.Get1st()) {
   default:
      break;
   case 'p':
   case 'y':
      this->OnVerifyCB_(AuthR(fon9_Auth_ENoSupport, "channel-binding-not-supported"), this);
      return false;
   case 'n':
      // client first message:
      // "n,,n=USER,r=ClientNONCE"
      // "n,,a=AUTHZ,n=USER,r=ClientNONCE"
      const char* pbeg = reqstr.begin();
      if (pbeg[1] == ',' && pbeg[2] == ',') {
         reqstr.SetBegin(pbeg += 3);
         // 移除 "n,," = client_first_message_bare = "a=authz,n=authc,r=nonce" 或 "n=authc,r=nonce"
         // this->AuthMessage_ = client_first_message_bare + ","
         //                    + server_first_message + ","
         //                    + client_final_message_without_proof;
         this->AuthMessage_ = reqstr.ToString(); // client_first_message_bare
         this->AuthMessage_.push_back(',');
         if (pbeg[0] == 'a') {
            if (pbeg[1] != '=')
               break;
            reqstr.SetBegin(pbeg + 2);
            this->AuthResult_.AuthzId_.assign(StrFetchNoTrim(reqstr, ','));
            pbeg = reqstr.begin();
         }
         if (pbeg[0] != 'n' || pbeg[1] != '=')
            break;
         reqstr.SetBegin(pbeg + 2);
         this->AuthResult_.AuthcId_.assign(StrFetchNoTrim(reqstr, ','));
         pbeg = reqstr.begin();
         if (pbeg[0] != 'r' || pbeg[1] != '=')
            break;
         // svr1stMsg = "r=ClientNONCE+ServerNONCE,s=SALT,i=ITERATOR"
         PassRec  passRec;
         bool     isUserFound;
         {
            auto  lockedUser = this->UserMgr_->GetLockedUser(this->AuthResult_);
            if (lockedUser.second) {
               this->AuthResult_.RoleId_ = lockedUser.second->RoleId_;
               passRec = lockedUser.second->Pass_;
               isUserFound = true;
            }
            else {
               // 不論是否找到 user, 應該都要回覆, 避免攻擊得知 userid.
               // this->OnVerifyCB_(AuthR{fon9_Auth_EUserPass, "unknown-user"}, this);
               // return false;
               isUserFound = false;
            }
         }
         if (!isUserFound)
            this->SetDefaultPass(passRec);
         const size_t   kSaltB64Size = Base64EncodeLengthNoEOS(passRec.Salt_.size());
         const size_t   kServerNonceLengthReserve = 50;
         std::string    svr1stMsg;
         svr1stMsg.reserve(reqstr.size() + kServerNonceLengthReserve + kSaltB64Size + 64);
         reqstr.AppendTo(svr1stMsg);
         this->AppendServerNonce(svr1stMsg);
         const size_t   kNonceLength = svr1stMsg.size();

         svr1stMsg.append(",s=", 3);
         svr1stMsg.resize(svr1stMsg.size() + kSaltB64Size);
         Base64Encode(&*svr1stMsg.begin() + svr1stMsg.size() - kSaltB64Size, kSaltB64Size,
                      passRec.Salt_.begin(), passRec.Salt_.size());
         this->SaltedPass_ = std::move(passRec.SaltedPass_);
         svr1stMsg.append(",i=", 3);
         NumOutBuf nbuf;
         svr1stMsg.append(UIntToStrRev(nbuf.end(), passRec.AlgParam_), nbuf.end());

         if (isUserFound) {
            this->AuthMessage_.append(svr1stMsg);
            // ,client_final_message_without_proof
            this->AuthMessage_.append(",c=biws,");
            this->AuthMessage_.append(svr1stMsg.c_str(), kNonceLength);
         }
         this->OnVerifyCB_(AuthR(fon9_Auth_NeedsMore, std::move(svr1stMsg)), this);
         return true;
      }
      break;
   }
   this->OnVerifyCB_(AuthR{fon9_Auth_EArgs_Format}, this);
   return false;
}

bool SaslScramServer::ProofError(const AuthRequest& req) {
   this->OnVerifyCB_(this->UserMgr_->AuthUpdate(fon9_Auth_EProof, req, this->AuthResult_), this);
   return false;
}

bool SaslScramServer::ParseClientProof(const AuthRequest& req) {
   if (++this->Step_ != 2)
      return false;
   std::string proof = this->MakeProof();
   if (proof.empty())
      return this->ProofError(req);
   // reqstr: "c=biws,r=ClientNonceServerNonce,p=ClientProof"
   
   if (0);// TODO: 提供「改密碼」機制?
   // reqstr: "c=biws,r=ClientNonceServerNonce,s=SALT,h=SALTED-PASS,p=ClientProof"
   //                                          \____ new pass ____/
   //       使用先前提供的 ITERATOR?
   //       SALT 長度必須與先前提供的 s= 相同.
   //       SALTED-PASS 必須正確.
   //
   StrView reqstr{&req.Response_};
   if (reqstr.size() <= proof.size() + 3 + 6 + 3) // +3=",p=" & ",r=";  +6="c=biws"
      return this->ProofError(req);
   size_t sizeWithoutProof = reqstr.size() - proof.size() - 3; // -3=",p="
   const char* pend = reqstr.end() - proof.size();
   if (*(pend - 2) != 'p' || *(pend - 1) != '='
   || this->AuthMessage_.size() <= sizeWithoutProof
   || memcmp(pend, proof.c_str(), proof.size()) != 0
   || memcmp(reqstr.begin(), this->AuthMessage_.c_str() + this->AuthMessage_.size() - sizeWithoutProof, sizeWithoutProof) != 0)
      return this->ProofError(req);
   AuthR authr = this->UserMgr_->AuthUpdate(fon9_Auth_Success, req, this->AuthResult_);
   if (!authr) {
      this->OnVerifyCB_(authr, this);
      return false;
   }
   this->OnVerifyCB_(AuthR(fon9_Auth_Success, "v=" + this->MakeVerify()), this);
   return true;
}

void SaslScramServer::AuthVerify(const AuthRequest& req) {
   switch (this->Step_) {
   case 0:
      this->ParseClientFirst(req);
      break;
   case 1:
      this->ParseClientProof(req);
      break;
   default:
      this->OnVerifyCB_(AuthR(fon9_Auth_EOther, "invalid-step"), this);
      break;
   }
}

} } // namespaces
