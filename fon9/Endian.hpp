/// \file fon9/Endian.hpp
/// 存取特定 byte order(big-endian or little-endian) 的資料.
/// \author fonwinz@gmail.com
#ifndef __fon9_Endian_hpp__
#define __fon9_Endian_hpp__
#include "fon9/Unaligned.hpp"
#include "fon9/Utility.hpp"

#if CHAR_BIT != 8
#error "Platforms with CHAR_BIT != 8 are not supported"
#endif

namespace fon9 {

/// \ingroup Misc
/// 類似 c++20 的 enum class endian;
enum class endian {
#ifdef fon9_WINDOWS
   little = REG_DWORD_LITTLE_ENDIAN,
   big = REG_DWORD_BIG_ENDIAN,
   native = REG_DWORD
#else
   little = __ORDER_LITTLE_ENDIAN__,
   big = __ORDER_BIG_ENDIAN__,
   native = __BYTE_ORDER__
#endif
};

//--------------------------------------------------------------------------//

namespace impl {
// 這是從 boost library: boost/endian/buffers.hpp 修改來的.
enum class IntStyle {
   Unknown,
   Enum,
   Signed,
   Unsigned,
};

template <typename T>
constexpr IntStyle GetIntStyle() {
   return std::is_enum<T>::value ? IntStyle::Enum
      : std::is_signed<T>::value ? IntStyle::Signed
      : std::is_unsigned<T>::value ? IntStyle::Unsigned
      : IntStyle::Unknown;
}
template <typename T>
constexpr bool IsEndianSupported() {
   return GetIntStyle<T>() != IntStyle::Unknown;
}

template <typename T, size_t n_bytes, IntStyle isel = GetIntStyle<T>()>
struct unrolled_byte_loops {
   typedef unrolled_byte_loops<T, n_bytes - 1, isel> next;

   static T load_big(const unsigned char* bytes) noexcept {
      return static_cast<T>(*(bytes - 1) | (next::load_big(bytes - 1) << 8));
   }
   static T load_little(const unsigned char* bytes) noexcept {
      return static_cast<T>(*bytes | (next::load_little(bytes + 1) << 8));
   }

   static void store_big(char* bytes, T value) noexcept {
      *(bytes - 1) = static_cast<char>(value);
      next::store_big(bytes - 1, static_cast<T>(value >> 8));
   }
   static void store_little(char* bytes, T value) noexcept {
      *bytes = static_cast<char>(value);
      next::store_little(bytes + 1, static_cast<T>(value >> 8));
   }
};

template <typename T>
struct unrolled_byte_loops<T, 1, IntStyle::Unsigned> {
   static T load_big(const unsigned char* bytes) noexcept {
      return static_cast<T>(*(bytes - 1));
   }
   static T load_little(const unsigned char* bytes) noexcept {
      return static_cast<T>(*bytes);
   }
   static void store_big(char* bytes, T value) noexcept {
      *(bytes - 1) = static_cast<char>(value);
   }
   static void store_little(char* bytes, T value) noexcept {
      *bytes = static_cast<char>(value);
   }
};

template <typename T>
struct unrolled_byte_loops<T, 1, IntStyle::Signed> {
   static T load_big(const unsigned char* bytes) noexcept {
      return *reinterpret_cast<const signed char*>(bytes - 1);
   }
   static T load_little(const unsigned char* bytes) noexcept {
      return *reinterpret_cast<const signed char*>(bytes);
   }
   static void store_big(char* bytes, T value)  noexcept {
      *(bytes - 1) = static_cast<char>(value);
   }
   static void store_little(char* bytes, T value) noexcept {
      *bytes = static_cast<char>(value);
   }
};

template <typename T, size_t n_bytes>
struct unrolled_byte_loops<T, n_bytes, IntStyle::Enum> : public unrolled_byte_loops<typename std::underlying_type<T>::type, n_bytes> {
   using UnderlyingT = typename std::underlying_type<T>::type;
   using base = unrolled_byte_loops<UnderlyingT, n_bytes>;

   static T load_big(const unsigned char* bytes) noexcept {
      return static_cast<T>(base::load_big(bytes));
   }
   static T load_little(const unsigned char* bytes) noexcept {
      return static_cast<T>(base::load_little(bytes));
   }

   static void store_big(char* bytes, T value) noexcept {
      base::store_big(bytes, static_cast<UnderlyingT>(value));
   }
   static void store_little(char* bytes, T value) noexcept {
      base::store_little(bytes, static_cast<UnderlyingT>(value));
   }
};
} // namespace impl;

template <typename T>
struct IsBigEndianT : public conditional_t<impl::IsEndianSupported<T>() && (endian::native == endian::big), std::true_type, std::false_type> {
};

template <typename T>
struct IsLittleEndianT : public conditional_t<impl::IsEndianSupported<T>() && (endian::native == endian::little), std::true_type, std::false_type> {
};

//--------------------------------------------------------------------------//

template <typename T>
static inline auto GetBigEndian(const void* address) -> enable_if_t<IsBigEndianT<T>::value, T> {
   return GetUnaligned(reinterpret_cast<const T*>(address));
}

template <typename T>
static inline auto GetBigEndian(const void* address) -> enable_if_t<IsLittleEndianT<T>::value, T> {
   return impl::unrolled_byte_loops<T, sizeof(T)>::load_big(static_cast<const unsigned char*>(address) + sizeof(T));
}

template <typename T>
static inline auto PutBigEndian(void* address, T value) -> enable_if_t<IsBigEndianT<T>::value> {
   PutUnaligned(reinterpret_cast<T*>(address), value);
}

template <typename T>
static inline auto PutBigEndian(void* address, T value) -> enable_if_t<IsLittleEndianT<T>::value> {
   impl::unrolled_byte_loops<T, sizeof(T)>::store_big(static_cast<char*>(address) + sizeof(T), value);
}

//--------------------------------------------------------------------------//

template <typename T>
static inline auto GetLittleEndian(const void* address) -> enable_if_t<IsLittleEndianT<T>::value, T> {
   return GetUnaligned(reinterpret_cast<const T*>(address));
}

template <typename T>
static inline auto GetLittleEndian(const void* address) -> enable_if_t<IsBigEndianT<T>::value, T> {
   return impl::unrolled_byte_loops<T, sizeof(T)>::load_little(static_cast<const unsigned char*>(address));
}

template <typename T>
static inline auto PutLittleEndian(void* address, T value) -> enable_if_t<IsLittleEndianT<T>::value> {
   PutUnaligned(reinterpret_cast<T*>(address), value);
}

template <typename T>
static inline auto PutLittleEndian(void* address, T value) -> enable_if_t<IsBigEndianT<T>::value> {
   impl::unrolled_byte_loops<T, sizeof(T)>::store_little(static_cast<char*>(address), value);
}

//--------------------------------------------------------------------------//

template <typename T>
static inline T GetPackedLittleEndian(const byte* address, byte count) {
   assert(count <= sizeof(T));
   if (count <= 0)
      return 0;
   T value = static_cast<T>(*(address += count - 1));
   while (--count > 0)
      value = (value << 8) | static_cast<T>(*--address);
   return value;
}

template <typename T, bool sign = std::is_signed<T>::value>
struct PackLittleEndianAux {
};
template <typename T>
struct PackLittleEndianAux<T, false> {
   static byte Put(byte* address, T value) {
      byte count = 0;
      do {
         *address++ = static_cast<byte>(value);
         ++count;
      } while (value >>= 8);
      return count;
   }
};
template <typename T>
struct PackLittleEndianAux<T, true> {
   static byte Put(byte* address, T value) {
      if (value >= 0) {
         using UT = typename std::make_unsigned<T>::type;
         return PackLittleEndianAux<UT>::Put(address, static_cast<UT>(value));
      }
      PutLittleEndian(address, value);
      return sizeof(T);
   }
};

/// \ingroup Misc
/// 資料由 *address++ 往後除儲存.
/// \return 用了多少 bytes 儲存 value, 最少 1 byte, 最多 sizeof(value);
template <typename T>
static inline byte PackLittleEndian(void* address, T value) {
   return PackLittleEndianAux<T>::Put(reinterpret_cast<byte*>(address), value);
}

//--------------------------------------------------------------------------//

template <typename T>
static inline T GetPackedBigEndian(const byte* address, byte count) {
   assert(count <= sizeof(T));
   if (count <= 0)
      return 0;
   T value = static_cast<T>(*address);
   while (--count > 0)
      value = (value << 8) | static_cast<T>(*++address);
   return value;
}

template <typename T, bool sign = std::is_signed<T>::value>
struct PackBigEndianAuxRev {
};
template <typename T>
struct PackBigEndianAuxRev<T, false> {
   static byte Put(byte* address, T value) {
      byte count = 0;
      do {
         *--address = static_cast<byte>(value);
         ++count;
      } while (value >>= 8);
      return count;
   }
};
template <typename T>
struct PackBigEndianAuxRev<T, true> {
   static byte Put(byte* address, T value) {
      if (value >= 0) {
         using UT = typename std::make_unsigned<T>::type;
         return PackBigEndianAuxRev<UT>::Put(address, static_cast<UT>(value));
      }
      PutBigEndian(address, value);
      return sizeof(T);
   }
};

/// \ingroup Misc
/// 資料由 *--address 往前除儲存.
/// \return 用了多少 bytes 儲存 value, 最少 1 byte, 最多 sizeof(value);
template <typename T>
static inline byte PackBigEndianRev(void* address, T value) {
   return PackBigEndianAuxRev<T>::Put(reinterpret_cast<byte*>(address), value);
}

} // namespace
#endif//__fon9_Endian_hpp__
