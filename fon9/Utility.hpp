/// \file fon9/Utility.hpp
///
/// libfon9 的簡易工具.
///
/// \author fonwinz@gmail.com
#ifndef __fon9_Utility_hpp__
#define __fon9_Utility_hpp__
#include "fon9/sys/Config.hpp"

fon9_BEFORE_INCLUDE_STD;
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <utility>
#include <new>
fon9_AFTER_INCLUDE_STD;

/// libfon9 除了巨集之外, 所有宣告都放在 namespace fon9 之中.
namespace fon9 {

/// \ingroup Misc
/// 傳回陣列元素數量 arysz.
template<class T, size_t arysz>
constexpr size_t numofele(T(&/*array*/)[arysz]) {
   return arysz;
}

/// \ingroup Misc
/// typename std::conditional<cond, TrueT, FalseT>::type
/// 同 C++17.
template <bool cond, typename TrueT, typename FalseT>
using conditional_t = typename std::conditional<cond, TrueT, FalseT>::type;

template <typename T>
using decay_t = typename std::decay<T>::type;

// 類似 C++14 的 integer_sequence<size_t, ...>
template<size_t... Indices>
struct integer_sequence {
   template<size_t N>
   using append = integer_sequence<Indices..., N>;
};

template<size_t N>
struct make_integer_sequence {
   using type = typename make_integer_sequence<N - 1>::type::template append<N - 1>;
};

template<>
struct make_integer_sequence<0u> {
   using type = integer_sequence<>;
};

template<size_t... Ints>
using index_sequence = integer_sequence<Ints...>;

template<size_t N>
using make_index_sequence = typename make_integer_sequence<N>::type;

/// \ingroup Misc
/// 檢查 EnumT 是否支援 bitwise 操作元(enum 可用 fon9_ENABLE_ENUM_BITWISE_OP(EnumT) 定義).
/// HasBitOpT<T>::value == true or false;
template <typename EnumT>
struct HasBitOpT {
   template <typename T>
   static auto HasBitOpChecker(const T&)->decltype(IsEnumContains(T{}, T{}));
   struct No { bool _[16]; };
   static No HasBitOpChecker(...);
   enum { value = (sizeof(HasBitOpChecker(*static_cast<typename std::remove_reference<EnumT>::type*>(nullptr))) != sizeof(No)) };
};

/// \ingroup Misc
/// 自動根據 IntType 是否有正負: 擴展成最大整數:
/// - signed:   則 type = intmax_t
/// - unsigned: 則 type = uintmax_t
template <typename IntType>
using ToImax_t = conditional_t<std::is_signed<IntType>::value, intmax_t, uintmax_t>;

/// \ingroup Misc
/// 同 C++14 的 std::enable_if_t<>
template<bool B, class T = void>
using enable_if_t = typename std::enable_if<B, T>::type;

template<typename EnumT>
using underlying_type_t = typename std::underlying_type<EnumT>::type;

/// \ingroup Misc
/// 同 C++14 的 std::enable_if_t<>
template<typename T>
using make_unsigned_t = typename std::make_unsigned<T>::type;

/// \ingroup Misc
/// 強制轉型為無正負整數.
template <typename T>
constexpr make_unsigned_t<T> unsigned_cast(T value) {
   return static_cast<make_unsigned_t<T>>(value);
}

template <typename T>
constexpr enable_if_t<std::is_signed<T>::value, make_unsigned_t<T>> abs_cast(T value) {
   return unsigned_cast(value < 0 ? -value : value);
}
template <typename T>
constexpr enable_if_t<std::is_unsigned<T>::value, make_unsigned_t<T>> abs_cast(T value) {
   return value;
}

/// \ingroup Misc
/// 將陣列值內容全部清為0.
template<class T, size_t arysz>
inline void ZeroAry(T(&ary)[arysz]) {
   static_assert(std::is_trivial<T>::value, "T must is_trivial");
   memset(ary, 0, sizeof(ary));
}
/// \ingroup Misc
/// 將資料結構內容全部清為0.
template<class T>
inline void ZeroStruct(T* r) {
   static_assert(std::is_trivial<T>::value, "T must is_trivial");
   memset(r, 0, sizeof(*r));
}
/// \ingroup Misc
/// 將資料結構內容全部清為0.
template<class T>
inline void ZeroStruct(T& r) {
   ZeroStruct(&r);
}

/// \ingroup Misc
/// 取得包含此 data member 的物件.
/// \code
/// struct MyRec {
///   MyField  Field_;
/// };
/// ...
/// MyField* fld;
/// ...
/// MyRec* myrec = fon9::ContainerOf(*fld, &MyRec::Field_);
/// \endcode
template <class T, typename DataMemberT>
inline T* ContainerOf(DataMemberT& m, DataMemberT T::*pDataMember) {
   return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(&m) - (reinterpret_cast<std::ptrdiff_t>(&(reinterpret_cast<const T*>(1)->*pDataMember)) - 1));
}

template <class Base, class Derived>
constexpr Base* CastToBasePointer(Derived* p, Base*) {
   return p;
}
/// \ingroup Misc
/// 計算 data member pointer 在 Base 的 offset.
/// 有可能為負值, 例:
///   - `class MyData : public Base1, public Base {};`
///   - 如果 pDataMember 在 Base1 裡面, 則可能為負值!
/// 應該要用 constexpr, 但是 MSVC, gcc 都不成功, 所以只好使用 inline 了.
template <typename MemberT, class Derived, class Base>
inline int32_t DataMemberOffset(MemberT Derived::*pDataMember) {
   Derived* const derived = reinterpret_cast<Derived*>(0x1000);
   MemberT* const ptrMemberT = &(derived->*pDataMember);
   Base*    const base = CastToBasePointer(derived, static_cast<Base*>(nullptr));
   return static_cast<int32_t>(reinterpret_cast<uintptr_t>(ptrMemberT) - reinterpret_cast<uintptr_t>(base));
   //底下這樣寫, g++ 可以用 constexpr, 但是 MSVC(19.00.23506) 仍然不行.
   //return static_cast<int32_t>(
   //   reinterpret_cast<uintptr_t>(&(reinterpret_cast<Derived*>(0x1000)->*pDataMember))
   //   - reinterpret_cast<uintptr_t>(static_cast<Base*>(reinterpret_cast<Derived*>(0x1000)))
   //   );
}

/// \ingroup Misc
/// 直接就地建構. 避免 DBG_NEW 的影響.
template<class T, class... ArgsT>
inline T* InplaceNew(void* p, ArgsT&&... args) {
   #ifdef new//取消 debug new.
   #undef new
   #endif
      return ::new (p) T(std::forward<ArgsT>(args)...);
   #ifdef DBG_NEW
   #define new DBG_NEW
   #endif
}
// 直接呼叫解構, 同 C++17 所提供的.
template<class T>
inline void destroy_at(T* p) {
   (void)p;
   p->~T();
}

/// \ingroup fon9_MACRO
/// - 讓 enum_t 可以使用 (a | b) 等同 static_cast<enum_t>(a | b)
/// - 讓 enum_t 可以使用 (a - b) 等同 static_cast<enum_t>(a & ~b)
/// - 讓 enum_t 可以使用 (a & b) 等同 static_cast<enum_t>(a & b)
/// - IsEnumContains(a,b): 傳回 ((a & b) == b)
/// - IsEnumContainsAny(a,b): 傳回 ((a & b) != 0)
#define fon9_ENABLE_ENUM_BITWISE_OP(enum_t)        \
constexpr enum_t operator| (enum_t a, enum_t b) { \
   return static_cast<enum_t>( static_cast<fon9::underlying_type_t<enum_t>>(a)   \
                              | static_cast<fon9::underlying_type_t<enum_t>>(b)); \
} \
inline enum_t& operator|= (enum_t& a, enum_t b) { \
   return a = static_cast<enum_t>( static_cast<fon9::underlying_type_t<enum_t>>(a)   \
                                 | static_cast<fon9::underlying_type_t<enum_t>>(b)); \
} \
constexpr enum_t operator- (enum_t a, enum_t b) { \
   return static_cast<enum_t>(  static_cast<fon9::underlying_type_t<enum_t>>(a)   \
                              & ~static_cast<fon9::underlying_type_t<enum_t>>(b)); \
} \
inline enum_t& operator-= (enum_t& a, enum_t b) { \
   return a = static_cast<enum_t>(  static_cast<fon9::underlying_type_t<enum_t>>(a)   \
                                 & ~static_cast<fon9::underlying_type_t<enum_t>>(b)); \
} \
constexpr enum_t operator& (enum_t a, enum_t b) { \
   return static_cast<enum_t>( static_cast<fon9::underlying_type_t<enum_t>>(a)   \
                              & static_cast<fon9::underlying_type_t<enum_t>>(b)); \
} \
inline enum_t& operator&= (enum_t& a, enum_t b) { \
   return a = static_cast<enum_t>( static_cast<fon9::underlying_type_t<enum_t>>(a)   \
                                 & static_cast<fon9::underlying_type_t<enum_t>>(b)); \
} \
constexpr bool IsEnumContains(enum_t a, enum_t b) { \
   return static_cast<enum_t>(static_cast<fon9::underlying_type_t<enum_t>>(a)   \
                              & static_cast<fon9::underlying_type_t<enum_t>>(b))  \
         == b; \
} \
constexpr bool IsEnumContainsAny(enum_t a, enum_t b) { \
   return (static_cast<fon9::underlying_type_t<enum_t>>(a)    \
         & static_cast<fon9::underlying_type_t<enum_t>>(b))   \
            != 0; \
} \
//----- fon9_ENABLE_ENUM_BITWISE_OP();

} // namespace
#endif//__fon9_Utility_hpp__
