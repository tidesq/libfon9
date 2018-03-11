/// \file fon9/DecBase.hpp
///
/// 輔助使用 [整數型別 + 指定小數長度(scale)] 處理浮點數。
///
/// \author fonwinz@gmail.com
#ifndef __fon9_DecBase_hpp__
#define __fon9_DecBase_hpp__
#include "fon9/Utility.hpp"
#include "fon9/Exception.hpp"
#include <limits>

namespace fon9 {

/// \ingroup AlNum
/// 小數位數型別: 0=沒有小數, 1=1位小數, 2=2位小數...
using DecScaleT = uint8_t;

enum : DecScaleT {
   /// fon9 能支援的最長小數位數.
   /// 若 intmax_t == int64_t 則可以有 [0..18] 位小數(包含DecScaleMax=18位).
   DecScaleMax = std::numeric_limits<intmax_t>::digits10
};

/// \ingroup AlNum
/// 小數位數為D時的除數.
/// \tparam IntType 整數型別, 例如: int, unsigned...
/// \tparam ScaleN  小數位數.
template <typename IntType, DecScaleT ScaleN>
struct DecDivisor {
   enum : DecScaleT {
      /// 小數位數.
      Scale = ScaleN
   };
   enum : IntType {
      /// 小數位數為 Scale 時的除數.
      Divisor = DecDivisor<IntType, Scale - 1>::Divisor * 10
   };
   static_assert(static_cast<IntType>(DecDivisor<IntType, Scale - 1>::Divisor) < static_cast<IntType>(Divisor),
                 "小數位數太大, 超過 IntType 可容納範圍!");
};

/// \ingroup AlNum
/// 固定小數位數為0時的除數 = 1.
template <typename IntType>
struct DecDivisor<IntType, 0> {
   enum : DecScaleT {
      Scale = 0,
   };
   enum : IntType {
      Divisor = 1,
   };
};

/// \ingroup AlNum
/// 用小數位取得10進位除數, 例:
///   - scale=0; retval=1;
///   - scale=1; retval=10;
///   - scale=2; retval=100;
///   - ...
/// exception:
///   - std::range_error("GetDecDivisor(scale > DecScaleMax)");
extern fon9_API uintmax_t GetDecDivisor(DecScaleT scale);

/// \ingroup AlNum
/// 使用除數四捨五入.
/// 例: num=123456; divisor=100;  =>  retval=1235
constexpr uintmax_t RoundByDivisor(uintmax_t num, uintmax_t divisor) {
   return (num / divisor) + ((num % divisor) >= (divisor / 2));
}
constexpr intmax_t RoundByDivisor(intmax_t num, uintmax_t divisor) {
   return num < 0
      ? static_cast<intmax_t>((num / static_cast<intmax_t>(divisor)) - static_cast<intmax_t>(((-num) % divisor) >= (divisor / 2)))
      : static_cast<intmax_t>(RoundByDivisor(unsigned_cast(num), divisor));
}

/// \ingroup AlNum
/// 使用小數位數四捨五入.
/// 例: num=123456; scale=2;  =>  retval=1235
inline uintmax_t Round(uintmax_t num, DecScaleT scale) {
   return RoundByDivisor(num, GetDecDivisor(scale));
}
inline intmax_t Round(intmax_t num, DecScaleT scale) {
   return RoundByDivisor(num, GetDecDivisor(scale));
}

/// \ingroup AlNum
/// 當 AdjustDecScale() 處理發生異常(Overflow, Underflow): 全都忽略.
struct AdjustDecScaleExIgnore {
   template <typename ResultT, typename SrcT>
   static constexpr ResultT OnAfterRound(SrcT src) {
      return static_cast<ResultT>(src);
   }
   template <typename ResultT, typename SrcT, typename MulT>
   static constexpr ResultT OnMultipleDivisor(SrcT src, MulT m) {
      return static_cast<ResultT>(static_cast<ToImax_t<SrcT>>(src) * m);
   }
};
/// \ingroup AlNum
/// 當 AdjustDecScale() 處理發生異常(Overflow, Underflow): 丟出例外.
struct AdjustDecScaleExThrow {
   template <typename ResultT, typename SrcT>
   static constexpr ResultT OnAfterRound(SrcT src) {
      return fon9_UNLIKELY(src < std::numeric_limits<ResultT>::min()) ? Raise<ResultT, std::underflow_error>("AdjustDecScale:Round.Underflow")
         : fon9_UNLIKELY(src > std::numeric_limits<ResultT>::max()) ? Raise<ResultT, std::overflow_error>("AdjustDecScale:Round.Overflow")
         : static_cast<ResultT>(src);
   }
   template <typename ResultT, typename SrcT, typename MulT>
   static ResultT OnMultipleDivisor(SrcT src, MulT m) {
      ResultT res = static_cast<ResultT>(static_cast<ToImax_t<SrcT>>(src) * m);
      if (fon9_UNLIKELY(src != static_cast<SrcT>(res / static_cast<ToImax_t<SrcT>>(m))))
         src < 0 ? Raise<std::underflow_error>("AdjustDecScale:Underflow") : Raise<std::overflow_error>("AdjustDecScale:Overflow");
      return res;
   }
};
/// \ingroup AlNum
/// 當 AdjustDecScale() 處理發生異常(Overflow, Underflow): 取可容納的: 最大值 or 最小值.
struct AdjustDecScaleExLimit {
   template <typename ResultT, typename SrcT>
   static constexpr ResultT OnAfterRound(SrcT src) {
      return fon9_UNLIKELY(src < std::numeric_limits<ResultT>::min()) ? std::numeric_limits<ResultT>::min()
         : fon9_UNLIKELY(src > std::numeric_limits<ResultT>::max()) ? std::numeric_limits<ResultT>::max()
         : static_cast<ResultT>(src);
   }
   template <typename ResultT, typename SrcT, typename MulT>
   static ResultT OnMultipleDivisor(SrcT src, MulT m) {
      const ResultT res = static_cast<ResultT>(static_cast<ToImax_t<SrcT>>(src) * m);
      if (fon9_UNLIKELY(src != static_cast<SrcT>(res / static_cast<ToImax_t<SrcT>>(m))))
         return src < 0 ? std::numeric_limits<ResultT>::min() : std::numeric_limits<ResultT>::max();
      return res;
   }
};
using AdjustDecScaleExDefault = AdjustDecScaleExIgnore;

fon9_MSC_WARN_DISABLE(4100);// warning C4100: 'ex': unreferenced formal parameter
/// \ingroup AlNum
/// src 轉成指定小數位數的數字.
/// - 不考慮 overflow、underflow.
/// - 若 srcScale, reScale 超過 DecScaleMax 則會丟出 exception
/// - 例: src = 12345; srcScale = 2; 實際數字 = 123.45;
///   - reScale = 1, 傳回的數字 = 1235   (小數1位,四捨五入: 123.5)
///   - reScale = 3, 傳回的數字 = 123450 (小數3位: 123.450)
template <typename SrcT, typename ResultT = ToImax_t<SrcT>, class AdjustDecScaleEx = AdjustDecScaleExDefault>
inline ResultT AdjustDecScale(SrcT src, DecScaleT srcScale, DecScaleT reScale, AdjustDecScaleEx ex = AdjustDecScaleEx{}) {
   if (reScale == srcScale)
      return src;
   using srcmax_t = ToImax_t<SrcT>;
   if (reScale < srcScale)
      return ex.template OnAfterRound<ResultT>(Round(static_cast<srcmax_t>(src), static_cast<DecScaleT>(srcScale - reScale)));
   return ex.template OnMultipleDivisor<ResultT>(src, GetDecDivisor(static_cast<DecScaleT>(reScale - srcScale)));
}
template <DecScaleT srcScale, DecScaleT reScale, typename SrcT, typename ResultT = ToImax_t<SrcT>, class AdjustDecScaleEx = AdjustDecScaleExDefault>
constexpr enable_if_t<(reScale < srcScale), ResultT> AdjustDecScale(SrcT src, AdjustDecScaleEx ex = AdjustDecScaleEx{}) {
   using srcmax_t = ToImax_t<SrcT>;
   return ex.template OnAfterRound<ResultT>(RoundByDivisor(static_cast<srcmax_t>(src), DecDivisor<SrcT, srcScale - reScale>::Divisor));
}
template <DecScaleT srcScale, DecScaleT reScale, typename SrcT, typename ResultT = ToImax_t<SrcT>, class AdjustDecScaleEx = AdjustDecScaleExDefault>
constexpr enable_if_t<(reScale >= srcScale), ResultT> AdjustDecScale(SrcT src, AdjustDecScaleEx ex = AdjustDecScaleEx{}) {
   return ex.template OnMultipleDivisor<ResultT>(src, DecDivisor<SrcT, reScale - srcScale>::Divisor);
}
fon9_MSC_WARN_POP;

} // namespace fon9
#endif//__fon9_DecBase_hpp__
