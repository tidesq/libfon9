/// \file fon9/ConfigUtils.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ConfigUtils_hpp__
#define __fon9_ConfigUtils_hpp__
#include "fon9/BitvArchive.hpp"

namespace fon9 {

enum class EnabledYN : char {
   /// 除了 Yes, 其餘皆為 No.
   Yes = 'Y'
};

template <class RevBuffer>
inline size_t ToBitv(RevBuffer& rbuf, EnabledYN value) {
   return ToBitv(rbuf, value == EnabledYN::Yes);
}

template <class RevBuffer>
inline void BitvTo(RevBuffer& rbuf, EnabledYN& value) {
   bool b = (value == EnabledYN::Yes);
   BitvTo(rbuf, b);
   value = b ? EnabledYN::Yes : EnabledYN{};
}

inline EnabledYN NormalizeEnabledYN(EnabledYN v) {
   return toupper(static_cast<char>(v)) == static_cast<char>(EnabledYN::Yes) ? EnabledYN::Yes : EnabledYN{};
}

} // namespaces
#endif//__fon9_ConfigUtils_hpp__
