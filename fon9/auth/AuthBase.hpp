/// \file fon9/auth/AuthBase.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_AuthBase_hpp__
#define __fon9_auth_AuthBase_hpp__
#include "fon9/auth/AuthBase.h"
#include "fon9/StrView.hpp"

namespace fon9 { namespace auth {

fon9_WARN_DISABLE_PADDING;
/// \ingroup auth
/// 認證要求的結果代碼及額外資訊(字串).
struct AuthR {
   fon9_Auth_R    RCode_;
   /// - RCode_ == fon9_Auth_Success, 則此處為成功的額外訊息, 例如: "3天後密碼過期,請盡速更換密碼".
   /// - RCode_ == fon9_Auth_NeedsMore, 則此處為 SASL 的 challenge 訊息.
   /// - fon9_IsAuthError(RCode_), 則此處為失敗原因.
   std::string    Info_;

   AuthR() = default;
   AuthR(fon9_Auth_R rcode) : RCode_{rcode} {
   }
   AuthR(fon9_Auth_R rcode, std::string info) : RCode_{rcode}, Info_{std::move(info)} {
   }

   bool IsError() const {
      return fon9_IsAuthError(this->RCode_);
   }
   bool IsSuccess() const {
      return !this->IsError();
   }
   explicit operator bool() const {
      return !this->IsError();
   }
   bool operator!() const {
      return this->IsError();
   }
};
fon9_MSC_WARN_POP;

} } // namespaces
#endif//__fon9_auth_AuthBase_hpp__
