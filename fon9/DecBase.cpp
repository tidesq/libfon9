// \file fon9/DecBase.cpp
// \author fonwinz@gmail.com
#include "fon9/DecBase.hpp"
namespace fon9 {

fon9_API uintmax_t GetDecDivisor(DecScaleT scale) {
   // 使用 uintmax_t 可以到19位, 但卻無法處理 intmax_t 的負數, 所以這裡必須使用 DecDivisor<intmax_t,n>::Divisor
   static constexpr intmax_t dmap[DecScaleMax + 1]{
      DecDivisor<intmax_t,0>::Divisor,
      DecDivisor<intmax_t,1>::Divisor,
      DecDivisor<intmax_t,2>::Divisor,
      DecDivisor<intmax_t,3>::Divisor,
      DecDivisor<intmax_t,4>::Divisor,
      DecDivisor<intmax_t,5>::Divisor,
      DecDivisor<intmax_t,6>::Divisor,
      DecDivisor<intmax_t,7>::Divisor,
      DecDivisor<intmax_t,8>::Divisor,
      DecDivisor<intmax_t,9>::Divisor,
      DecDivisor<intmax_t,10>::Divisor,
      DecDivisor<intmax_t,11>::Divisor,
      DecDivisor<intmax_t,12>::Divisor,
      DecDivisor<intmax_t,13>::Divisor,
      DecDivisor<intmax_t,14>::Divisor,
      DecDivisor<intmax_t,15>::Divisor,
      DecDivisor<intmax_t,16>::Divisor,
      DecDivisor<intmax_t,17>::Divisor,
      DecDivisor<intmax_t,18>::Divisor,
   };
   if (fon9_LIKELY(scale < numofele(dmap)))
      return static_cast<uintmax_t>(dmap[scale]);
   throw std::range_error("GetDecDivisor(scale > DecScaleMax)");
}

} // namespaces
