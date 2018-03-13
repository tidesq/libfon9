/// \file fon9/ToStr.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ToStr_hpp__
#define __fon9_ToStr_hpp__
#include "fon9/StrTools.hpp"
#include "fon9/DecBase.hpp"

namespace fon9 {

/// \ingroup AlNum
/// 一般數字輸出的緩衝.
/// 讓數字輸出函式, 不用考慮緩衝區不足的問題.
struct NumOutBuf {
   fon9_NON_COPY_NON_MOVE(NumOutBuf);
   NumOutBuf() = default;

   enum {
      /// 最大可能的輸出: 使用2進位, 每 4 bits 填入一個空白, 例: "1111 0000 1111 ....".
      MaxBinOutputWidth = ((sizeof(uintmax_t) * CHAR_BIT + 3) / 4) * 5,

      /// 多加一點(16 bytes)緩衝, 扣除可能需要 EOS(1 byte).
      /// 在 uintmax_t == 64bits 的情況下, 此值為 95.
      BufferSize = (((MaxBinOutputWidth + 15) / 16) + 1) * 16 - 1
   };
   union {
      char  Buffer_[BufferSize];
      char  BufferEOS_[BufferSize + 1];
   };
   void SetEOS() {
      this->BufferEOS_[BufferSize] = 0;
   }
   char* end() {
      return this->Buffer_ + BufferSize;
   }
   constexpr const char* end() const {
      return this->Buffer_ + BufferSize;
   }
   /// 數字輸出都是從 buffer 尾端開始往前填入,
   /// 當數字輸出完畢時會得到「字串開始位置」,
   /// 可用此計算輸出的字串長度.
   constexpr size_t GetLength(const char* pout) const {
      return
      #ifndef __GNUC__
         assert(static_cast<size_t>(this->end() - pout) <= BufferSize),
      #endif
         static_cast<size_t>(this->end() - pout);
   }
};

/// \ingroup AlNum
/// 一般數字型別, 使用 `ToStr...(char* pout)` 函式時, pout 需要多少緩衝?
/// - 預設為 sizeof(NumOutBuf);
/// - 如果您有自訂的 `ToStr...(char* pout, const MyObj& value);` 則應自訂此函式:
///   - `constexpr size_t ToStrMaxWidth(const MyObj&);`
template <typename ValueT>
constexpr size_t ToStrMaxWidth(const ValueT&) {
   return sizeof(NumOutBuf);
}

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 建立 value 的 2 進位字串.
/// - 從 (*--pout) 往前填入.
/// - 傳回最後一次 pout 位置: 字串開始位置.
inline char* BinToStrRev(char* pout, uintmax_t value) {
   do {
      *(--pout) = static_cast<char>((value & 1) + '0');
   } while ((value >>= 1) != 0);
   return pout;
}

template <typename IntT>
inline auto BinToStrRev(char* pout, IntT value) -> decltype(BinToStrRev(pout, static_cast<uintmax_t>(unsigned_cast(value)))) {
   return BinToStrRev(pout, static_cast<uintmax_t>(unsigned_cast(value)));
}

inline char* OctToStrRev(char* pout, uintmax_t value) {
   do {
      *--pout = static_cast<char>(static_cast<char>(value & 0x07) + '0');
   } while ((value >>= 3) != 0);
   return pout;
}

template <typename IntT>
inline auto OctToStrRev(char* pout, IntT value) -> decltype(OctToStrRev(pout, static_cast<uintmax_t>(unsigned_cast(value)))) {
   return OctToStrRev(pout, static_cast<uintmax_t>(unsigned_cast(value)));
}

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 建立 value 的 16 進位字串(小寫).
/// - 從 (*--pout) 往前填入.
/// - 傳回最後一次 pout 位置: 字串開始位置.
fon9_API char* HexToStrRev(char* pout, uintmax_t value);
fon9_API char* HEXToStrRev(char* pout, uintmax_t value);

template <typename IntT>
inline auto HexToStrRev(char* pout, IntT value) -> decltype(HexToStrRev(pout, static_cast<uintmax_t>(unsigned_cast(value)))) {
   return HexToStrRev(pout, static_cast<uintmax_t>(unsigned_cast(value)));
}
template <typename IntT>
inline auto HEXToStrRev(char* pout, IntT value) -> decltype(HEXToStrRev(pout, static_cast<uintmax_t>(unsigned_cast(value)))) {
   return HEXToStrRev(pout, static_cast<uintmax_t>(unsigned_cast(value)));
}

inline char* PtrToStrRev(char* pout, const void* value) {
   return HexToStrRev(pout, reinterpret_cast<uintmax_t>(value));
}

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 建立 value 的 10 進位字串.
/// - 從 (*--pout) 往前填入.
/// - 傳回最後一次 pout 位置: 字串開始位置.
inline char* UIntToStrRev(char* pout, uintmax_t value) {
#if 0
   do {
      *--pout = static_cast<char>(static_cast<char>(value % 10) + '0');
   } while ((value /= 10) != 0);
#else
   while (value >= 100) {
      pout = Put2Digs(pout, static_cast<uint8_t>(value % 100));
      value /= 100;
   }
   if (value < 10)
      *--pout = static_cast<char>('0' + value);
   else
      pout = Put2Digs(pout, static_cast<uint8_t>(value));
#endif
   return pout;
}

/// \ingroup AlNum
/// 建立 value 的 10 進位字串: 加上千位分隔, 分隔符號、分隔位數由 NumPunct 控制.
/// - 從 (*--pout) 往前填入.
/// - 傳回最後一次 pout 位置: 字串開始位置.
fon9_API char* UIntToStrRev_IntSep(char* pout, uintmax_t value);
inline char* SIntToStrRev_IntSep(char* pout, intmax_t value) {
   pout = UIntToStrRev_IntSep(pout, abs_cast(value));
   if (value < 0)
      *(--pout) = '-';
   return pout;
}

/// \ingroup AlNum
/// 建立 value 的 10 進位字串.
/// - 從 (*--pout) 往前填入.
/// - 傳回最後一次 pout 位置: 字串開始位置.
inline char* SIntToStrRev(char* pout, intmax_t value) {
   pout = UIntToStrRev(pout, abs_cast(value));
   if (value < 0)
      *(--pout) = '-';
   return pout;
}

template <typename IntT>
inline auto ToStrRev(char* pout, IntT value) -> enable_if_t<std::is_signed<IntT>::value, char*> {
   return SIntToStrRev(pout, static_cast<intmax_t>(value));
}

template <typename IntT>
inline auto ToStrRev(char* pout, IntT value) -> enable_if_t<std::is_unsigned<IntT>::value, char*> {
   return UIntToStrRev(pout, static_cast<uintmax_t>(value));
}

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 自動小數位數: 移除小數點後無用的0, 若小數部分為0, 則一併移除小數點.
/// - 從 (*--pout) 往前填入.
/// - 傳回最後一次 pout 位置: 字串開始位置.
/// - 返回時 value = 移除小數位之後的整數部分.
fon9_API char* PutAutoPrecision(char* pout, uintmax_t& value, DecScaleT scale);

inline char* UDecToStrRev(char* pout, uintmax_t value, DecScaleT scale) {
   pout = PutAutoPrecision(pout, value, scale);
   return UIntToStrRev(pout, value);
}

inline char* SDecToStrRev(char* pout, intmax_t value, DecScaleT scale) {
   pout = UDecToStrRev(pout, (value < 0) ? static_cast<uintmax_t>(-value) : static_cast<uintmax_t>(value), scale);
   if (value < 0)
      *(--pout) = '-';
   return pout;
}

template <typename IntT>
inline auto DecToStrRev(char* pout, IntT value, DecScaleT scale) -> enable_if_t<std::is_signed<IntT>::value, char*> {
   return SDecToStrRev(pout, static_cast<intmax_t>(value), scale);
}

template <typename IntT>
inline auto DecToStrRev(char* pout, IntT value, DecScaleT scale) -> enable_if_t<std::is_unsigned<IntT>::value, char*> {
   return UDecToStrRev(pout, static_cast<uintmax_t>(value), scale);
}

//--------------------------------------------------------------------------//

namespace impl {
template <typename IntT, size_t Width>
struct AuxPic9ToStr {
   static void ToStr(char* pout, IntT value) noexcept {
      Put2Digs(pout, static_cast<uint8_t>(value % 100));
      using leftchars = AuxPic9ToStr<IntT, Width - 2>;
      leftchars::ToStr(pout - 2, static_cast<IntT>(value / 100));
   }
};
template <typename IntT>
struct AuxPic9ToStr<IntT, 1> {
   static void ToStr(char* pout, IntT value) noexcept {
      assert(value <= 9);
      *(pout - 1) = fon9_LIKELY(value <= 9) ? static_cast<char>(value + '0') : '#';
   }
};
template <typename IntT>
struct AuxPic9ToStr<IntT, 2> {
   static void ToStr(char* pout, IntT value) noexcept {
      assert(value <= 99);
      if (fon9_LIKELY(value <= 99))
         Put2Digs(pout, static_cast<uint8_t>(value));
      else {
         *(pout - 1) = static_cast<char>((value % 10) + '0');
         *(pout - 2) = '#';
      }
   }
};
} // namespace impl

/// \ingroup AlNum
/// 將數字轉固定寬度輸出.
/// 例: `Pic9ToStrRev<3>(pout, 123u);`
template <unsigned Width, typename IntT>
inline char* Pic9ToStrRev(char* pout, IntT value) {
   static_assert(std::is_unsigned<IntT>::value, "Pic9ToStrRev() only support unsigned.");
   static_assert(Width > 0, "Pic9ToStrRev() Width must > 0.");
   impl::AuxPic9ToStr<IntT, Width>::ToStr(pout, value);
   return pout - Width;
}

template <unsigned Width, typename IntT>
inline char* SPic9ToStrRev(char* pout, IntT value) {
   using UIntT = typename std::make_unsigned<IntT>::type;
   impl::AuxPic9ToStr<UIntT, Width>::ToStr(pout, static_cast<UIntT>(value < 0 ? -value : value));
   *(pout -= (Width + 1)) = (value < 0 ? '-' : '+');
   return pout;
}

} // namespace fon9
#endif//__fon9_ToStr_hpp__
