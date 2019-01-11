// \file f9tws/ExgTypes.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgTypes_hpp__
#define __f9tws_ExgTypes_hpp__
#include "f9tws/Config.h"
#include "fon9/CharAry.hpp"

namespace f9tws {

struct BrkId : public fon9::CharAry<4> {
   using base = fon9::CharAry<4>;
   using base::base;
};
constexpr fon9::StrView ToStrView(const BrkId& brkid) {
   return fon9::StrView{brkid.Chars_, brkid.size()};
}

struct OrdNo : public fon9::CharAry<5> {
   using base = fon9::CharAry<5>;
   using base::base;
};
constexpr fon9::StrView ToStrView(const OrdNo& ordno) {
   return fon9::StrView{ordno.Chars_, ordno.size()};
}

struct StkNo : public fon9::CharAry<6> {
   using base = fon9::CharAry<6>;
   using base::base;
};
constexpr fon9::StrView ToStrView(const StkNo& stkno) {
   return fon9::StrView{stkno.Chars_,
        stkno.Chars_[stkno.size() - 1] != ' ' ? stkno.size()
      : stkno.Chars_[stkno.size() - 2] != ' ' ? stkno.size() - 1
      : stkno.size() - 2};
}

} // namespaces
#endif//__f9tws_ExgTypes_hpp__
