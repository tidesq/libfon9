// \file fon9/crypto/Crypto_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/crypto/Sha256.hpp"
#include "fon9/auth/SaslScramSha256.hpp"
#include "fon9/Timer.hpp"
#include "fon9/TestTools.hpp"

struct HashTestCase {
   const char* Value_;
   const char* Result_;
};

template <class Alg>
void TestHash(const char* testName, const HashTestCase* testCases, size_t testCount) {
   std::cout << "[TEST ] " << testName;
   fon9::byte output[Alg::kOutputSize];
   for (size_t L = 0; L < testCount; ++L) {
      Alg::Hash(testCases->Value_, strlen(testCases->Value_), output);
      char strout[Alg::kOutputSize * 3];
      for (size_t i = 0; i < Alg::kOutputSize; ++i)
         sprintf(strout + i * 2, "%02x", output[i]);
      if (strcmp(strout, testCases->Result_) != 0) {
         std::cout << "|in=" << testCases->Value_ << "\r[ERROR] " << std::endl;
         abort();
      }
      ++testCases;
   }
   std::cout << "\r[OK   ]" << std::endl;
}

//--------------------------------------------------------------------------//

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

   fon9::AutoPrintTestInfo utinfo{"Crypto"};
   fon9::GetDefaultTimerThread();
   std::this_thread::sleep_for(std::chrono::milliseconds{10});

   static const HashTestCase  sha256TestCases[] = {
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

   //--------------------------------------------------------------------------//
   // Test for HMAC / PBKDF2
   std::cout << "[TEST ] SASL: SCRAM-SHA-256";
   // https://tools.ietf.org/html/rfc7677
   // The username 'user' and password 'pencil' are being used.
   // C: n,,n=user,r=rOprNGfwEbeRWgbNEkqO
   std::string  client_first_message_bare = "n=user,r=rOprNGfwEbeRWgbNEkqO";

   #define kB64Salt     "W22ZaJ0SNY7soEsUEjb6gQ=="
   // S: r=rOprNGfwEbeRWgbNEkqO%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0,s=W22ZaJ0SNY7soEsUEjb6gQ==,i=4096
   //    r=ClientNONCE+ServerNONCE,s=SALT,i=ITERATOR
   std::string  server_first_message = "r=rOprNGfwEbeRWgbNEkqO%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0,s=" kB64Salt ",i=4096";

   // ===== Client: 解析 Server ack.
   char         salt[128];
   size_t       saltSize = fon9::Base64Decode(salt, sizeof(salt), kB64Salt, sizeof(kB64Salt) - 1).GetResult();
   const size_t iter = 4096;
   // ===== Client: 計算 saltedPassword
   std::string  password = "pencil";
   fon9::byte   saltedPassword[fon9::crypto::Sha256::kOutputSize];
   fon9::crypto::Sha256::CalcSaltedPassword(password.c_str(), password.size(), salt, saltSize, iter,
                                            sizeof(saltedPassword), saltedPassword);

   // C: c=biws,r=rOprNGfwEbeRWgbNEkqO%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0,p=dHzbZapWIk4jUhN+Ute9ytag9zjfMHgsqmmiz7AndVQ=
   // S: v=6rriTRBi23WpRR/wtup+mMhUZUn/dB5nLTJRsjl95G4=
   std::string client_final_message_without_proof = "c=biws,r=rOprNGfwEbeRWgbNEkqO%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0";
   std::string authMessage = client_first_message_bare + "," + server_first_message + "," + client_final_message_without_proof;

   // ===== Client/Server: 計算 proof.
   std::string cliendProof = fon9::auth::SaslScramSha256::MakeProof(saltedPassword, &authMessage);
   if (cliendProof != "dHzbZapWIk4jUhN+Ute9ytag9zjfMHgsqmmiz7AndVQ=") {
      std::cout << "|p=" << cliendProof << "|err=Client proof error" "\r[ERROR]" << std::endl;
      abort();
   }
   // ===== Server: Check "r=NONCEs" & "p=ClientProof"
   std::string serverVerify = fon9::auth::SaslScramSha256::MakeVerify(saltedPassword, &authMessage);
   if (serverVerify != "6rriTRBi23WpRR/wtup+mMhUZUn/dB5nLTJRsjl95G4=") {
      std::cout << "|v=" << serverVerify << "|err=Server verify error" "\r[ERROR]" << std::endl;
      abort();
   }
   // S => C
   // Client: Check "v=ServerSignature" or "e=server-error-value"
   std::cout << "\r[OK   ]" << std::endl;
}
