/// \file fon9/auth/SaslScramServer.cpp
/// \author fonwinz@gmail.com
#include "fon9/auth/SaslScramServer.hpp"
#include "fon9/Base64.hpp"
#include "fon9/ToStr.hpp"

namespace fon9 { namespace auth {

void SaslScramServer::AppendSaltAndIterator(std::string& out) {
   const size_t   kSaltB64Size = Base64EncodeLengthNoEOS(this->Pass_.Salt_.size());
   out.resize(out.size() + kSaltB64Size);
   Base64Encode(&*out.begin() + out.size() - kSaltB64Size, kSaltB64Size,
                this->Pass_.Salt_.begin(), this->Pass_.Salt_.size());
   out.append(",i=", 3);
   NumOutBuf nbuf;
   out.append(UIntToStrRev(nbuf.end(), this->Pass_.AlgParam_), nbuf.end());
}

void SaslScramServer::ParseClientFirst(const AuthRequest& req) {
   if (++this->Step_ != 1)
      return;
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
      return;
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
         bool  isUserFound;
         {
            auto  lockedUser = this->UserMgr_->GetLockedUser(this->AuthResult_);
            if (lockedUser.second) {
               this->AuthResult_.RoleId_ = lockedUser.second->RoleId_;
               this->Pass_ = lockedUser.second->Pass_;
               isUserFound = true;
            }
            else {
               // 不論是否找到 user, 應該都要回覆, 避免攻擊得知 userid.
               // this->OnVerifyCB_(AuthR{fon9_Auth_EUserPass, "unknown-user"}, this);
               // return;
               isUserFound = false;
            }
         }
         if (!isUserFound)
            this->SetDefaultPass();
         const size_t   kServerNonceLengthReserve = 50;
         std::string    svr1stMsg;
         svr1stMsg.reserve(reqstr.size() + kServerNonceLengthReserve + 128);
         reqstr.AppendTo(svr1stMsg);
         this->AppendServerNonce(svr1stMsg);
         const size_t   kNonceLength = svr1stMsg.size();
         svr1stMsg.append(",s=", 3);
         this->AppendSaltAndIterator(svr1stMsg);
         if (isUserFound) {
            this->AuthMessage_.append(svr1stMsg);
            // ,client_final_message_without_proof
            this->AuthMessage_.append(",c=biws,");
            this->AuthMessage_.append(svr1stMsg.c_str(), kNonceLength);
         }
         this->OnVerifyCB_(AuthR(fon9_Auth_NeedsMore, std::move(svr1stMsg)), this);
         return;
      }
      break;
   }
   this->OnVerifyCB_(AuthR{fon9_Auth_EArgs_Format}, this);
}

void SaslScramServer::ProofError(const AuthRequest& req) {
   this->OnVerifyCB_(this->UserMgr_->AuthUpdate(fon9_Auth_EProof, req, this->AuthResult_), this);
}

void SaslScramServer::ParseClientProof(const AuthRequest& req) {
   if (++this->Step_ != 2)
      return;
   StrView     reqstr{&req.Response_};
   // reqstr: "c=biws,r=ClientNonceServerNonce,p=ClientProof"
   // reqstr: "c=biws,r=ClientNonceServerNonce,s=,p=ClientProof"
   //                                          \_ change pass request.
   //         此時的 AuthMessage = SaslAuthMessage + ",s="
   //         然後用調整後的 AuthMessage 計算 Proof.
   if (reqstr.size() < 12) // "c=biws,r=,p="
      return this->ProofError(req);
   const char* pProof = StrView::traits_type::find(reqstr.begin() + 10, reqstr.size() - 10, ',');
   if (!pProof)
      return this->ProofError(req);
   if (pProof[1] == 's' && pProof[2] == '=') {
      // fon9: 提供「改密碼」機制.
      this->IsChgPass_ = true;
      this->AuthMessage_.append(pProof, 3); // ",s="
      pProof += 3;
   }
   if (pProof[1] != 'p' || pProof[2] != '=')
      return this->ProofError(req);

   size_t sizeWithoutProof = static_cast<size_t>(pProof - reqstr.begin());
   if (this->AuthMessage_.size() <= sizeWithoutProof
   || memcmp(reqstr.begin(), this->AuthMessage_.c_str() + this->AuthMessage_.size() - sizeWithoutProof, sizeWithoutProof) != 0)
      return this->ProofError(req);

   if (!this->CheckProof(StrView{pProof + 3, reqstr.end()}))
      return this->ProofError(req);

   if (this->IsChgPass_) {
      this->SetDefaultPass();
      // s=NewSALT,i=NewITERATOR,v=verifier
      //         此時的 AuthMessage = SaslAuthMessage + ",s=NewSALT,i=NewITERATOR"
      //         然後用調整後的 AuthMessage 計算 verifier.
      AuthR rChgPass{fon9_Auth_NeedsMore};
      rChgPass.Info_.reserve(128);
      rChgPass.Info_.assign("s=", 2);
      this->AppendSaltAndIterator(rChgPass.Info_);
      this->AuthMessage_.append(rChgPass.Info_.c_str() + 2, rChgPass.Info_.size() - 2);
      rChgPass.Info_.append(",v=", 3);
      rChgPass.Info_.append(this->MakeVerify());
      this->OnVerifyCB_(std::move(rChgPass), this);
      return;
   }
   AuthR authr = this->UserMgr_->AuthUpdate(fon9_Auth_Success, req, this->AuthResult_);
   if (!authr)
      this->OnVerifyCB_(authr, this);
   else
      this->OnVerifyCB_(AuthR(fon9_Auth_Success, "v=" + this->MakeVerify()), this);
}

void SaslScramServer::ParseChangePass(const AuthRequest& req) {
   // C: h=XSaltedPassword,p=NewProof
   StrView reqstr{&req.Response_};
   if (reqstr.Get1st() != 'h' || reqstr.size() < 10) {
   __ERROR_CB:
      this->OnVerifyCB_(AuthR{fon9_Auth_EArgs_NewPass}, this);
      return;
   }
   if (reqstr.begin()[1] != '=')
      goto __ERROR_CB;
   const char* pProof = reqstr.Find(',');
   if (!pProof || pProof[1] != 'p' || pProof[2] != '=')
      goto __ERROR_CB;
   ByteVector xSaltedPass;
   auto szSaltedPass = Base64Decode(xSaltedPass.alloc(this->Pass_.SaltedPass_.size()), this->Pass_.SaltedPass_.size(),
                                    reqstr.begin() + 2, static_cast<size_t>(pProof - reqstr.begin() - 2));
   if (!szSaltedPass || szSaltedPass.GetResult() != xSaltedPass.size())
      goto __ERROR_CB;
   byte* pSaltedPass = xSaltedPass.begin();
   for (byte& b : this->Pass_.SaltedPass_)
      b = static_cast<byte>(b ^ *pSaltedPass++);
   if (!this->CheckProof(StrView{pProof + 3, reqstr.end()}))
      goto __ERROR_CB;

   AuthR authr = this->UserMgr_->PassChanged(this->Pass_, req, this->AuthResult_);
   if (!authr)
      this->OnVerifyCB_(authr, this);
   else
      this->OnVerifyCB_(AuthR(fon9_Auth_PassChanged, "h=" + this->MakeVerify()), this);
}

void SaslScramServer::AuthVerify(const AuthRequest& req) {
   switch (this->Step_) {
   case 0:
      this->ParseClientFirst(req);
      break;
   case 1:
      this->ParseClientProof(req);
      break;
   case 2:
      if (this->IsChgPass_) {
         this->ParseChangePass(req);
         break;
      }
      // 不用 break; 不是改密碼, 就是錯誤的 step.
   default:
      this->OnVerifyCB_(AuthR(fon9_Auth_EOther, "invalid-step"), this);
      break;
   }
}

} } // namespaces
