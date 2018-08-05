/// \file fon9/auth/SaslScramSha256Client.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_SaslScramSha256Client_hpp__
#define __fon9_auth_SaslScramSha256Client_hpp__
#include "fon9/auth/SaslClient.hpp"

namespace fon9 { namespace auth {

fon9_API SaslClientSP SaslScramSha256ClientCreator(const StrView& authz, const StrView& authc, const StrView& pass);
fon9_API SaslClientSP SaslScramSha256ClientCreator_ChgPass(const StrView& authz, const StrView& authc, const StrView& oldPass, const StrView& newPass);

} } // namespaces
#endif//__fon9_auth_SaslScramSha256Client_hpp__
