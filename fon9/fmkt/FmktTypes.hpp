// \file fon9/FmktTypes.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_FmktTypes_hpp__
#define __fon9_fmkt_FmktTypes_hpp__
#include "fon9/fmkt/FmktTypes.h"
#include "fon9/Decimal.hpp"

namespace fon9 { namespace fmkt {

using Pri = Decimal<int64_t, 8>;
using Qty = uint64_t;
using Amt = Pri;

} } // namespaces
#endif//__fon9_fmkt_FmktTypes_hpp__
