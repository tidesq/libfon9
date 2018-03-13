/// \file fon9/ToStrFmt.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ToStrFmt_hpp__
#define __fon9_ToStrFmt_hpp__
#include "fon9/FmtDef.hpp"
#include "fon9/ToStr.hpp"

namespace fon9 {

fon9_API char* ToStrRev(char* pout, StrView str, FmtDef fmt);

/// 若有指定 fmt.Width_ 則處理 [靠右、靠左] 調整.
fon9_API char* ToStrRev_LastJustify(char* pout, char* pstart, FmtDef fmt);

//--------------------------------------------------------------------------//

/// 當要輸出的數字為0, 且有設定 FmtFlag::Hide0 則透過此處根據 fmt 填入輸出內容:
/// - 是否要填空白?
/// - 是否要填 '+'? '+' 要靠左? 還是靠右?
fon9_API char* ToStrRev_Hide0(char* pout, FmtDef fmt);

/// 根據 [已輸出的資料] 及 [fmt] 做最後的調整.
/// szout = 已輸出的資料量.
fon9_API char* IntToStrRev_LastJustify(char* pout, char* pstart, bool isNeg, FmtDef fmt);

inline char* UIntToStrRev_CheckIntSep(char* pout, uintmax_t value, FmtFlag fmtflags) {
   return fon9_UNLIKELY(IsEnumContains(fmtflags, FmtFlag::ShowIntSep))
      ? UIntToStrRev_IntSep(pout, value)
      : UIntToStrRev(pout, value);
}

/// \ingroup AlNum
/// 整數的格式化輸出.
fon9_API char* IntToStrRev(char* pout, uintmax_t value, bool isNeg, FmtDef fmt);

template <typename IntT>
inline auto ToStrRev(char* pout, IntT value, const FmtDef& fmt) -> enable_if_t<std::is_integral<IntT>::value, char*> {
   return IntToStrRev(pout, abs_cast(value), value < 0, fmt);
}

//--------------------------------------------------------------------------//

/// 輸出指定[精確度precision]的小數位.
/// - 如果需要裁切, 則會呼叫 fnRound(val,div):
///   - 四捨五入:   [](uintmax_t val, uintmax_t div) { return (val + (div / 2)) / div; }
///   - 無條件捨去: [](uintmax_t val, uintmax_t div) { return val / div; }
fon9_API char* ToStrRev_DecScalePrecision(char* pout, uintmax_t& value, DecScaleT scale, FmtDef::WidthType precision,
                                          uintmax_t (*fnRound)(uintmax_t val, uintmax_t div));

/// 填入小數部分.
/// 填完後 value 會剩下整數部分.
fon9_API char* ToStrRev_DecScale(char* pout, uintmax_t& value, DecScaleT scale, FmtDef fmt);

fon9_API char* DecToStrRev(char* pout, uintmax_t value, bool isNeg, DecScaleT scale, FmtDef fmt);

//--------------------------------------------------------------------------//

struct BaseFmt {
   uintmax_t Value_;
   BaseFmt() = delete;
   template <typename T, typename = enable_if_t<std::is_integral<T>::value, T>>
   explicit BaseFmt(T v) : Value_{unsigned_cast(v)} {
   }
   template <typename T>
   explicit BaseFmt(const T* v) : Value_{reinterpret_cast<uintmax_t>(v)} {
   }
};
inline char* ToStrRev(char* pout, BaseFmt value, FmtDef fmt) {
   return IntToStrRev(pout, value.Value_, false, fmt);
}

template <FmtFlag baseFlag, char* (*FnToStrRev)(char* pout, uintmax_t value)>
struct BaseDefine : public BaseFmt {
   BaseDefine() = delete;
   using BaseFmt::BaseFmt;
   static constexpr FmtFlag BaseFlag() { return baseFlag; }
   static char* ToStrRev(char* pout, uintmax_t value) {
      return FnToStrRev(pout, value);
   }
};
using ToHEX = BaseDefine<FmtFlag::BaseHEX, HEXToStrRev>;
using ToHex = BaseDefine<FmtFlag::BaseHex, HexToStrRev>;
using ToPtr = BaseDefine<FmtFlag::BaseHex, HexToStrRev>;
using ToOct = BaseDefine<FmtFlag::BaseOct, OctToStrRev>;
using ToBin = BaseDefine<FmtFlag::BaseBin, BinToStrRev>;

template <class BaseT>
inline enable_if_t<std::is_base_of<BaseFmt, BaseT>::value, char*> ToStrRev(char* pout, BaseT value) {
   return BaseT::ToStrRev(pout, value.Value_);
}

template <class BaseT>
inline enable_if_t<std::is_base_of<BaseFmt, BaseT>::value, char*> ToStrRev(char* pout, BaseT value, FmtDef fmt) {
   fmt.Flags_ = (fmt.Flags_ - FmtFlag::MaskBase) | BaseT::BaseFlag();
   return IntToStrRev(pout, value.Value_, false, fmt);
}

} // namespace
#endif//__fon9_ToStrFmt_hpp__
