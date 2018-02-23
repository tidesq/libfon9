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

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 建立 value 的 16 進位字串.
/// - 從 (*--pout) 往前填入.
/// - 傳回最後一次 pout 位置: 字串開始位置.
fon9_API char* HexToStrRev(char* pout, uintmax_t value);

template <typename IntT>
inline auto HexToStrRev(char* pout, IntT value) -> decltype(HexToStrRev(pout, static_cast<uintmax_t>(unsigned_cast(value)))) {
   return HexToStrRev(pout, static_cast<uintmax_t>(unsigned_cast(value)));
}

inline char* PtrToStrRev(char* pout, void* value) {
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
/// 建立 value 的 10 進位字串.
/// - 從 (*--pout) 往前填入.
/// - 傳回最後一次 pout 位置: 字串開始位置.
inline char* SIntToStrRev(char* pout, intmax_t value) {
   pout = UIntToStrRev(pout, (value < 0) ? static_cast<uintmax_t>(-value) : static_cast<uintmax_t>(value));
   if (value < 0)
      *(--pout) = '-';
   return pout;
}

template <typename IntT>
inline auto IntToStrRev(char* pout, IntT value) -> enable_if_t<std::is_signed<IntT>::value, char*> {
   return SIntToStrRev(pout, static_cast<intmax_t>(value));
}

template <typename IntT>
inline auto IntToStrRev(char* pout, IntT value) -> enable_if_t<std::is_unsigned<IntT>::value, char*> {
   return UIntToStrRev(pout, static_cast<uintmax_t>(value));
}

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 自動小數位數: 移除小數點後無用的0, 若小數部分為0, 則一併移除小數點.
/// - 從 (*--pout) 往前填入.
/// - 傳回最後一次 pout 位置: 字串開始位置.
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

} // namespace fon9
#endif//__fon9_ToStr_hpp__
