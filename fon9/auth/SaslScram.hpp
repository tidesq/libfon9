/// \file fon9/auth/SaslScram.hpp
/// \author fonwinz@gmail.com
#include "fon9/Base64.hpp"
#include "fon9/StrView.hpp"

namespace fon9 { namespace auth {

/// - authMessage = client_first_message_bare + "," + server_first_message + "," + client_final_message_without_proof;
/// - client_first_message_bare          = "a=authz,n=authc,r=ClientNonce"
/// - server_first_message               = "r=ClientNonce" "ServerNonce" ",s=SALT,i=ITERATOR"
/// - client_final_message_without_proof = "c=biws,r=ClientNonce" "ServerNonce"
template <class Alg>
class SaslScram : public Alg {
public:
   enum : size_t {
      kOutputSize = Alg::kOutputSize
   };

   static void AppendProof(const byte saltedPassword[kOutputSize], StrView authMessage, std::string& out) {
      static const char   cstrClientKey[] = "Client Key";

      byte  client_key[kOutputSize];
      byte  stored_key[kOutputSize];
      Alg::Hmac(cstrClientKey, sizeof(cstrClientKey) - 1, saltedPassword, kOutputSize, client_key);
      Alg::Hash(client_key, kOutputSize, stored_key);

      byte  client_signature[kOutputSize];
      byte  client_proof[kOutputSize];
      Alg::Hmac(authMessage.begin(), authMessage.size(), stored_key, sizeof(stored_key), client_signature);
      for (size_t i = 0; i < kOutputSize; ++i)
         client_proof[i] = static_cast<byte>(client_key[i] ^ client_signature[i]);

      const size_t proofLen = Base64EncodeLengthNoEOS(sizeof(client_proof));
      char         proofBuf[proofLen * 2];
      Base64Encode(proofBuf, proofLen, client_proof, sizeof(client_proof));
      out.append(proofBuf, proofLen);
   }
   static std::string MakeProof(const byte saltedPassword[kOutputSize], StrView authMessage) {
      std::string out;
      AppendProof(saltedPassword, authMessage, out);
      return out;
   }

   static std::string MakeVerify(const byte saltedPassword[kOutputSize], StrView authMessage) {
      // Server: Check "r=NONCEs" & "p=ClientProof"
      static const char  cstrSetverKey[] = "Server Key";
      byte  server_key[kOutputSize];
      byte  server_signature[kOutputSize];
      Alg::Hmac(cstrSetverKey, sizeof(cstrSetverKey) - 1, saltedPassword, kOutputSize, server_key);
      Alg::Hmac(authMessage.begin(), authMessage.size(), server_key, sizeof(server_key), server_signature);

      const size_t vLen = Base64EncodeLengthNoEOS(sizeof(server_signature));
      char         vBuf[vLen * 2];
      Base64Encode(vBuf, vLen, server_signature, sizeof(server_signature));
      return std::string(vBuf, vLen);
   }
};

} } // namespaces
