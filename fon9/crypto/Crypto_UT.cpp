// \file fon9/crypto/Crypto_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/crypto/Sha256.hpp"
#include "fon9/auth/SaslScramSha256Client.hpp"
#include "fon9/auth/SaslScramSha256Server.hpp"
#include "fon9/Timer.hpp"
#include "fon9/TestTools.hpp"

struct TestCaseItem {
   const char* Value_;
   const char* Result_;
};

//--------------------------------------------------------------------------//

template <class Alg>
void TestHash(const char* testName, const TestCaseItem* items, size_t testCount) {
   std::cout << "[TEST ] " << testName;
   fon9::byte output[Alg::kOutputSize];
   for (size_t L = 0; L < testCount; ++L) {
      Alg::Hash(items->Value_, strlen(items->Value_), output);
      char strout[Alg::kOutputSize * 3];
      for (size_t i = 0; i < Alg::kOutputSize; ++i)
         sprintf(strout + i * 2, "%02x", output[i]);
      if (strcmp(strout, items->Result_) != 0) {
         std::cout << "|in=" << items->Value_ << "\r[ERROR] " << std::endl;
         abort();
      }
      ++items;
   }
   std::cout << "\r[OK   ]" << std::endl;
}

void TestSha256() {
   static const TestCaseItem  sha256TestCases[] = {
      {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
      {"a", "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb"},
      {"ab", "fb8e20fc2e4c3f248c60c39bd652f3c1347298bb977b8b4d5903b85055620603"},
      {"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
      {"abcd", "88d4266fd4e6338d13b845fcf289579d209c897823b9217da3e161936f031589"},
      {"abcde", "36bbe50ed96841d10443bcb670d6554f0a34b761be67ec9c4a8ad2c0c44ca42c"},
      {"abcdef", "bef57ec7f53a6d40beb640a780a639c83bc29ac8a9816f1fc6c5c6dcd93c4721"},
      // block size = 64
      {"123456789012345678901234567890123456789012345678901234","2a05d676edbe39e9e37d6eb4e748ea8fab58ec0adf1d2e6ccd165af7bfb991d0"},
      {"1234567890123456789012345678901234567890123456789012345","03c3a70e99ed5eeccd80f73771fcf1ece643d939d9ecc76f25544b0233f708e9"},
      {"12345678901234567890123456789012345678901234567890123456","0be66ce72c2467e793202906000672306661791622e0ca9adf4a8955b2ed189c"},
      {"123456789012345678901234567890123456789012345678901234567890","decc538c077786966ac863b5532c4027b8587ff40f6e3103379af62b44eae44d"},
      {"1234567890123456789012345678901234567890123456789012345678901234","676491965ed3ec50cb7a63ee96315480a95c54426b0b72bca8a0d4ad1285ad55"},
      {"1234567890123456789012345678901234567890123456789012345678901234a","866426144965f6dd719796bf77d51c1532927d4d2ebfaf55a3014abe3d328f59"},
      {"1234567890123456789012345678901234567890123456789012345678901234ab","522bbc05277fcb7a20f6192a81429a0c13fd04ddbbcc4466dcd856be96dae90e"},
      {"1234567890123456789012345678901234567890123456789012345678901234abc","53a9aa1a6056c5a46e3196bb0ffd74060967ca986faef79acdbd5adf14565ab1"},
      {"1234567890123456789012345678901234567890123456789012345678901234abcd","2d0d5e38a729abbee11b5f736a9dc0f9f0f194bcf8dd57face27dd0e0c452e50"},
      {"1234567890123456789012345678901234567890123456789012345678901234123456789012345678901234567890123456789012345678901234","ec74ed96e6ba57d93d6fb28d7a5592b6b430cd36f88811d6bec35ac58cc7b310"},
      {"12345678901234567890123456789012345678901234567890123456789012341234567890123456789012345678901234567890123456789012345","86836400c79f172f7a7a10ca72b8e313f019c588d4a8c91a45e5b460103c46c3"},
      {"123456789012345678901234567890123456789012345678901234567890123412345678901234567890123456789012345678901234567890123456","c45f5a3249a4524db7eab3164036ce26da1d5582f8f953ad138d192d471ae346"},
      {"1234567890123456789012345678901234567890123456789012345678901234123456789012345678901234567890123456789012345678901234567890","6adc6cf44ff7678646106bc195c53cdf59db1f20f931b175ff66e6d3656e6c99"},
      {"12345678901234567890123456789012345678901234567890123456789012341234567890123456789012345678901234567890123456789012345678901234","e33c2742a41754c22429bb370b82bdc42d1d311a08d1c6bb45363ae29ead75d1"},
   };
   TestHash<fon9::crypto::Sha256>("Sha256", sha256TestCases, fon9::numofele(sha256TestCases));
}

//--------------------------------------------------------------------------//

// https://tools.ietf.org/html/rfc7677
// The username 'user' and password 'pencil' are being used.
const char kInitUserId[] = "user";
const char kInitPassword[] = "pencil";

void DecodeSalt(fon9::auth::Salt& out, fon9::StrView strSalt) {
   const size_t saltSize = fon9::Base64DecodeLength(strSalt.size());
   auto r64d = fon9::Base64Decode(out.alloc(saltSize), saltSize, strSalt.begin(), strSalt.size());
   out.resize(r64d.GetResult());
}

struct UserTree : public fon9::auth::UserTree {
   fon9_NON_COPY_NON_MOVE(UserTree);
   using base = fon9::auth::UserTree;
   using UserSP = fon9::intrusive_ptr<fon9::auth::UserRec>;
   UserSP InitUser_;
   UserTree(fon9::StrView strSalt, fon9::auth::PassAlgParam passAlgParam, fon9::auth::FnHashPass fnHashPass)
      : base(fon9::auth::UserMgr::MakeFields(), std::move(fnHashPass))
      , InitUser_{new fon9::auth::UserRec{kInitUserId}} {
      DecodeSalt(this->InitUser_->Pass_.Salt_, strSalt);
      this->InitUser_->Pass_.AlgParam_ = passAlgParam;
      fnHashPass(kInitPassword, sizeof(kInitPassword) - 1, this->InitUser_->Pass_, fon9::auth::HashPassFlag::ResetNone);
      Locker maps{this->PolicyMaps_};
      maps->ItemMap_.insert(this->InitUser_);
   }
   fon9::auth::AuthR AuthUpdate(fon9_Auth_R rcode, const fon9::auth::AuthRequest&, fon9::auth::AuthResult&, const fon9::auth::PassRec* passRec, fon9::auth::UserMgr&) override {
      // 我們只是要用 SASL 測試 crypto::Sha256 , 並沒有測試 UserTree::AuthUpdate() 的異動更新.
      if (passRec)
         this->InitUser_->Pass_ = *passRec;
      return fon9::auth::AuthR{rcode};
   }
};

struct TestSaslServer {
   fon9::seed::MaTreeSP  Root_;
   fon9::auth::AuthMgrSP AuthMgr_;
   fon9::auth::UserMgrSP UserMgr_;
   TestSaslServer() {
      this->Root_.reset(new fon9::seed::MaTree{"ma"});
      this->AuthMgr_ = fon9::auth::AuthMgr::Plant(this->Root_, nullptr, "AuthMgr");
   }
   virtual ~TestSaslServer() {
      this->Root_->OnParentSeedClear();
   }
   virtual fon9::auth::AuthSessionSP CreateAuthSession(fon9::auth::FnOnAuthVerifyCB cb) = 0;
};

static void CheckScramMessage(const char* head, const std::string& cur, const char* exp) {
   if (!exp || cur != exp) {
      std::cout << "|err=Unexpected " << head << "\r[ERROR]"
         "\n|req=" << cur << "\n|exp=" << (exp ? exp : "null") << std::endl;
      abort();
   }
}

// scramMessages[0] = rClientNonce
void TestSaslScram(TestSaslServer& server, fon9::auth::SaslClientSP saslcli, const char** scramMessages) {
   std::string cmsg = saslcli->GetFirstMessage();
   cmsg.erase(cmsg.find(",r=") + 3);
   cmsg.append(*scramMessages++);
   saslcli->SetFirstMessage(std::move(cmsg), saslcli->GetGs2HeaderSize());

   fon9::auth::AuthRequest req;
   req.UserFrom_ = "TestUT";
   req.Response_ = saslcli->GetFirstMessage();
   fon9_Auth_R rcode = fon9_Auth_Waiting;
   auto fnAuthCallback = [&scramMessages, &rcode, &saslcli, &req](fon9::auth::AuthR authr, fon9::auth::AuthSessionSP authSession) {
      rcode = authr.RCode_;
      if (authr.RCode_ == fon9_Auth_Success || authr.RCode_ == fon9_Auth_PassChanged) {
         CheckScramMessage("server verified", authr.Info_, *scramMessages++);
         std::cout << "\r[OK   ]" << std::endl;
         return;
      }

      if (authr.RCode_ != fon9_Auth_NeedsMore) {
         std::cout << "|err=Server:" << authr.RCode_ << ":" << authr.Info_;
         return;
      }

      fon9::auth::AuthR res = saslcli->OnChallenge(&authr.Info_);
      if (res.RCode_ != fon9_Auth_NeedsMore) {
         std::cout << "|err=Client:" << res.RCode_ << ":" << res.Info_;
         return;
      }

      CheckScramMessage("server challenge", authr.Info_, *scramMessages++);
      req.Response_ = res.Info_;
      CheckScramMessage("client request", req.Response_, *scramMessages++);
      authSession->AuthVerify(req);
   };
   CheckScramMessage("client request", req.Response_, *scramMessages++);
   server.CreateAuthSession(std::move(fnAuthCallback))->AuthVerify(req);

   if (rcode != fon9_Auth_Success && rcode != fon9_Auth_PassChanged) {
      std::cout << "\r[ERROR]" << std::endl;
      abort();
   }
   if (*scramMessages) {
      std::cout << "[ERRPR] |err=Has remain scram?|msg=" << *scramMessages << std::endl;
      abort();
   }
}

//--------------------------------------------------------------------------//
// Sha256 範例來自:
// https://tools.ietf.org/html/rfc7677#section-3
#define kSha256ClientNonce "rOprNGfwEbeRWgbNEkqO"
#define kSha256ServerNonce "%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0"
#define kSha256Salt        "W22ZaJ0SNY7soEsUEjb6gQ=="
#define kSha256ITERATOR    4096

#define kSha256NewSalt     "WWqttC9+Us100/9vaS58VjcR"
#define kSha256NewITERATOR 10000
#define kNewPassword       "i love fon9" // 可以是任意長度的字串.

static const char* kScramSha256Messages[] = {
   kSha256ClientNonce,

   // ----- step 1: client first message -----
   "n,,n=user,r=" kSha256ClientNonce,
   //    \__/     \_ client nonce _/
   //
   // ----- step 2.1: server challenge -----
   "r=" kSha256ClientNonce kSha256ServerNonce ",s=" kSha256Salt ",i=" fon9_CTXTOCSTR(kSha256ITERATOR),
   //   \_ client nonce _/ \_ server nonce _/       \_ salt __/
   // ----- step 2.2: client response -----
   "c=biws,r=" kSha256ClientNonce kSha256ServerNonce ",p=dHzbZapWIk4jUhN+Ute9ytag9zjfMHgsqmmiz7AndVQ=",
   //          \_ client nonce _/ \_ server nonce _/     \______________ client proof ______________/
   //
   // ----- step 3: server ack verify -----
   "v=6rriTRBi23WpRR/wtup+mMhUZUn/dB5nLTJRsjl95G4=",
   // Client check "v=ServerSignature" or "e=server-error-value"
   nullptr,
};
static const char* kScramSha256Messages_ChgPass[] = {
   kSha256ClientNonce,
   "n,,n=user,r=" kSha256ClientNonce,
   "r=" kSha256ClientNonce kSha256ServerNonce ",s=" kSha256Salt ",i=" fon9_CTXTOCSTR(kSha256ITERATOR),
   "c=biws,r=" kSha256ClientNonce kSha256ServerNonce ",s=,p=3hNK8VLY3MNYUtGPl7ZV22wxTdSSpqj3kBxhBzV67KI=",
   //                                                 \_/ Change pass request.
   //
   // ----- server ack: "s=NewSALT,i=NewITERATOR,v=..." -----
   "s=" kSha256NewSalt ",i=" fon9_CTXTOCSTR(kSha256NewITERATOR) ",v=fnDd96dJ1d44G+g3cEfZnunQpjoNUbGIfYjVmUHtv1k=",
   // ----- client response: "h=XSaltedPassword,p=NewProof"
   "h=kvHpTNCSD/LR4Xx45AgfNhSVd7QWfZC8ZP+U6tHf7mo=,p=eCM6XCJGqBxIAbuI6JN1EYv/2ZI41oNn09FINafSGsE=",
   // ----- server ack: "v=..." -----
   "h=+ImOUvADbsuEao41MTJoLseHlUkglbz27m0fBjAAeBQ=",
   // Client check "v=ServerSignature" or "e=server-error-value"
   nullptr,
};
static const char* kScramSha256Messages_NewPass[] = {
   kSha256ClientNonce,
   "n,,n=user,r=" kSha256ClientNonce,
   "r=" kSha256ClientNonce kSha256ServerNonce ",s=" kSha256NewSalt ",i=" fon9_CTXTOCSTR(kSha256NewITERATOR),
   "c=biws,r=" kSha256ClientNonce kSha256ServerNonce ",p=ENBgpbSqpMxJcDNa/Wwbhph/p2zGtRidRsKmJZOK9yU=",
   "v=PJKznLHNX6PPKphHf77kQq3N2/oIYEQLVhQU8fFidTY=",
   nullptr,
};
struct ScramSha256Server : public fon9::auth::AuthSession_SaslScramSha256 {
   fon9_NON_COPY_NON_MOVE(ScramSha256Server);
   using base = fon9::auth::AuthSession_SaslScramSha256;
   using base::base;
   void AppendServerNonce(std::string& svr1stMsg) const override {
      svr1stMsg.append(kSha256ServerNonce);
   }
   void SetDefaultPass() override { // 改密碼 or 找不到UserRec.
      this->Pass_.AlgParam_ = kSha256NewITERATOR;
      DecodeSalt(this->Pass_.Salt_, kSha256NewSalt);
   }
};
struct TestSaslServer_ScramSha256 : public TestSaslServer {
   TestSaslServer_ScramSha256() {
      auto fnHashPassSha256 = &fon9::auth::PassRec::HashPass<fon9::crypto::Sha256, fon9::auth::SaslScramSha256Param>;
      this->UserMgr_.reset(new fon9::auth::UserMgr(new UserTree(kSha256Salt, kSha256ITERATOR, fnHashPassSha256), "UserMgr"));
   }
   fon9::auth::AuthSessionSP CreateAuthSession(fon9::auth::FnOnAuthVerifyCB cb) override {
      return fon9::auth::AuthSessionSP{new ScramSha256Server(this->AuthMgr_, std::move(cb), this->UserMgr_)};
   }
   void Test() {
      std::cout << "[TEST ] SASL: SCRAM-SHA-256";
      TestSaslScram(*this, fon9::auth::SaslScramSha256ClientCreator("", kInitUserId, kInitPassword), kScramSha256Messages);
      std::cout << "[TEST ] SASL: SCRAM-SHA-256 (ChangePass)";
      TestSaslScram(*this, fon9::auth::SaslScramSha256ClientCreator_ChgPass("", kInitUserId, kInitPassword, kNewPassword),
                    kScramSha256Messages_ChgPass);

      std::cout << "[TEST ] NewSaltedPass";
      fon9::auth::UserRec* user = static_cast<UserTree*>(this->UserMgr_->GetSapling().get())->InitUser_.get();
      fon9::auth::PassRec  pass;
      DecodeSalt(pass.Salt_, kSha256NewSalt);
      fon9::crypto::Sha256::CalcSaltedPassword(kNewPassword, sizeof(kNewPassword) - 1, pass.Salt_.begin(), pass.Salt_.size(),
                                               kSha256NewITERATOR,
                                               fon9::crypto::Sha256::kOutputSize, pass.SaltedPass_.alloc(fon9::crypto::Sha256::kOutputSize));
      if (user->Pass_.AlgParam_ != kSha256NewITERATOR || user->Pass_.Salt_ != pass.Salt_ || user->Pass_.SaltedPass_ != pass.SaltedPass_) {
         std::cout << "\r[ERROR]\n";
         abort();
      }
      std::cout << "\r[OK   ]\n";

      std::cout << "[TEST ] SASL: SCRAM-SHA-256 (NewPassword)";
      TestSaslScram(*this, fon9::auth::SaslScramSha256ClientCreator("", kInitUserId, kNewPassword), kScramSha256Messages_NewPass);
   }
};

//--------------------------------------------------------------------------//

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

   fon9::AutoPrintTestInfo utinfo{"Crypto"};
   fon9::GetDefaultTimerThread();
   std::this_thread::sleep_for(std::chrono::milliseconds{10});

   TestSha256();

   //--------------------------------------------------------------------------//
   // Test SASL: SCRAM-SHA-256 for Sha256 HMAC / PBKDF2
   TestSaslServer_ScramSha256{}.Test();
}
