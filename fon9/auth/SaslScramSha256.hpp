/// \file fon9/auth/SaslScramSha256.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_SaslScramSha256_hpp__
#define __fon9_auth_SaslScramSha256_hpp__
#include "fon9/crypto/Sha256.hpp"
#include "fon9/auth/SaslScram.hpp"

namespace fon9 { namespace auth {

fon9_API_TEMPLATE_CLASS(SaslScramSha256, SaslScram, crypto::Sha256);

} } // namespaces
#endif//__fon9_auth_SaslScramSha256_hpp__
