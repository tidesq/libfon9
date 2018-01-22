/// \file fon9/Comparable.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_Comparable_hpp__
#define __fon9_Comparable_hpp__

namespace fon9 {

/// \ingroup AlNum
/// - 您的 T 必須有定義:
///   - bool operator==(const T& lhs, const T& rhs);
///   - bool operator< (const T& lhs, const T& rhs);
/// - 然後可透過這裡, 增加:
///   - bool operator!=(const T& lhs, const T& rhs);
///   - bool operator> (const T& lhs, const T& rhs);
///   - bool operator>=(const T& lhs, const T& rhs);
///   - bool operator<=(const T& lhs, const T& rhs);
template <class T>
struct Comparable {
   inline friend bool operator!=(const T& lhs, const T& rhs) {
      return !(lhs == rhs);
   }
   inline friend bool operator> (const T& lhs, const T& rhs) {
      return (rhs < lhs);
   }
   inline friend bool operator<=(const T& lhs, const T& rhs) {
      return !(rhs < lhs);
   }
   inline friend bool operator>=(const T& lhs, const T& rhs) {
      return !(lhs < rhs);
   }
};

} // namespaces
#endif//__fon9_Comparable_hpp__
