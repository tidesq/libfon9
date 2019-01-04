// \file fon9/fmkt/FmktTypes.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_FmktTypes_hpp__
#define __fon9_fmkt_FmktTypes_hpp__
#include "fon9/fmkt/FmktTypes.h"
#include "fon9/Decimal.hpp"

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 一般價格型別, 最大/最小值: +- 92,233,720,368.54775807
using Pri = Decimal<int64_t, 8>;
/// \ingroup fmkt
/// 一般數量型別.
using Qty = uint64_t;
/// \ingroup fmkt
/// [價+量] 成對出現時使用.
struct PriQty {
   Pri   Pri_{};
   Qty   Qty_{};
};

/// \ingroup fmkt
/// 一般金額型別(=Pri型別): 最大約可表達 +- 92e (92 * 10^6);
using Amt = Pri;
/// \ingroup fmkt
/// 大金額型別, 但小數位縮減成 2 位, 最大約可表達 +- 92,233,720e;
using BigAmt = Decimal<int64_t, 2>;

} } // namespaces
#endif//__fon9_fmkt_FmktTypes_hpp__
