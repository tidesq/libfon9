/// \file fon9/auth/SaslScramSha256Client.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_SaslScramSha256Client_hpp__
#define __fon9_auth_SaslScramSha256Client_hpp__
#include "fon9/auth/SaslClient.hpp"

namespace fon9 { namespace auth {

fon9_API SaslClientSP SaslScramSha256ClientCreator (StrView authz, StrView authc, StrView pass);

} } // namespaces
#endif//__fon9_auth_SaslScramSha256Client_hpp__
