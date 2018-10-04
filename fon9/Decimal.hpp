/// \file fon9/Decimal.hpp
///
/// 用整數型別+指定小數長度(scale) => 固定小數位數字: fon9::Decimal
///
/// \author fonwinz@gmail.com
#ifndef __fon9_Decimal_hpp__
#define __fon9_Decimal_hpp__
#include "fon9/ToStrFmt.hpp"
#include "fon9/StrTo.hpp"

namespace fon9 {

template <typename IntTypeT>
struct DecimalAuxSInt {
   enum : IntTypeT {
      OrigNull = std::numeric_limits<IntTypeT>::min()
   };
   /// 移除整數部分, Divisor=1000: value=123456 => 456; value=-123.456 => 456;
   /// 傳回值一定是正數.
   constexpr static IntTypeT RemoveIntPart (IntTypeT value, IntTypeT Divisor) {
      return value >= 0 ? (value % Divisor) : ((-value) % Divisor);
   }
   /// 移小數數部分, Divisor=1000: value=123456 => 123000; value=-123.456 => -123000;
   /// 一律無條件捨去,不考慮四捨五入.
   constexpr static IntTypeT RemoveDecPart (IntTypeT value, IntTypeT Divisor) {
      return value >= 0 ? (value - value % Divisor) : (value + ((-value) % Divisor));
   }
};
template <typename IntTypeT>
struct DecimalAuxUInt {
   enum : IntTypeT {
      OrigNull = std::numeric_limits<IntTypeT>::max()
   };
   constexpr static IntTypeT RemoveIntPart (IntTypeT value, IntTypeT Divisor) {
      return value % Divisor;
   }
   constexpr static IntTypeT RemoveDecPart (IntTypeT value, IntTypeT Divisor) {
      return value - value % Divisor;
   }
};
template <typename IntTypeT>
using DecimalAux = conditional_t<std::is_signed<IntTypeT>::value, DecimalAuxSInt<IntTypeT>, DecimalAuxUInt<IntTypeT>>;

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 使用 [IntTypeT 整數儲存數字] 加上 [ScaleN 固定小數位], 組成的數字.
///
/// - Decimal 的四則運算等同於 IntTypeT 的四則運算，所有相關的 Overflow、Underflow 都與 IntTypeT 相同。
///
/// \tparam IntTypeT 整數型別, 例如: int, int64_t, uint64_t...
/// \tparam ScaleN   小數位數, 例如: 3, 表示固定3位小數.
template <typename IntTypeT, DecScaleT ScaleN>
class Decimal : public Comparable<Decimal<IntTypeT, ScaleN>> {
   using base = DecDivisor<IntTypeT, ScaleN>;
   IntTypeT  Value_;
   static_assert(std::is_integral<IntTypeT>::value, "Decimal<IntTypeT,ScaleN> IntTypeT必須是整數型別");
protected:
   /// 直接用原始整數建構, 您必須自行確定 value 的 小數位數=this->Scale.
   constexpr explicit Decimal(IntTypeT value) : Value_(value) {
   }

   template <typename F>
   static constexpr IntTypeT RoundToOrigInt (F value) {
      static_assert(std::is_floating_point<F>::value, "RoundToOrigInt(F value), F必須是浮點數型別");
      return static_cast<IntTypeT>(value < 0 ? (value * Divisor - 0.5) : (value * Divisor + 0.5));
   }
public:
   using OrigType = IntTypeT;

   enum : DecScaleT {
      /// 小數位數.
      Scale = ScaleN
   };
   enum : OrigType {
      /// 小數位數為 Scale 時的除數.
      Divisor = base::Divisor,
      OrigNull = DecimalAux<IntTypeT>::OrigNull,
   };

   constexpr Decimal() : Value_{} {
   }

   /// 使用指定小數位建構.
   /// 例如: 使用 `123.456` 建構: `Decimal<I,S> value(123456, 3);`
   template <typename SrcT, class AdjustDecScaleEx = AdjustDecScaleExDefault>
   constexpr Decimal(SrcT src, DecScaleT srcScale, AdjustDecScaleEx ex = AdjustDecScaleEx{})
      : Value_{AdjustDecScale<SrcT, OrigType>(src, srcScale, ScaleN, ex)} {
   }
   
   /// \copydoc Assign(Decimal<SrcI, srcScale> value)
   template <typename SrcI, DecScaleT srcScale, class AdjustDecScaleEx = AdjustDecScaleExDefault>
   explicit constexpr Decimal(Decimal<SrcI, srcScale> src, AdjustDecScaleEx ex = AdjustDecScaleEx{})
      : Value_(AdjustDecScale<srcScale, Scale>(src.GetOrigValue(), ex)) {
   }

   /// 使用 浮點數 四捨五入建構.
   template <typename F>
   explicit constexpr Decimal(F value) : Value_{RoundToOrigInt(value)} {
   }

   /// 可使用 `Decimal<int,3>::Make<5>(12345678);` 方式建立 Decimal:
   /// 將 `123.45678` 轉成小數 3 位: `123.457` (四捨五入)
   template <DecScaleT srcScale, typename SrcT, class AdjustDecScaleEx = AdjustDecScaleExDefault>
   static constexpr Decimal Make(SrcT src, AdjustDecScaleEx ex = AdjustDecScaleEx{}) {
      return Decimal{AdjustDecScale<srcScale, ScaleN, SrcT, OrigType>(src, ex)};
   }

   static constexpr Decimal min() {
      return Decimal{std::numeric_limits<IntTypeT>::min()};
   }
   static constexpr Decimal max() {
      return Decimal{std::numeric_limits<IntTypeT>::max()};
   }
   static constexpr Decimal Null() {
      return Decimal{static_cast<OrigType>(OrigNull)};
   }

   constexpr bool IsNull() const {
      return this->Value_ == OrigNull;
   }
   void AssignNull() {
      this->Value_ = OrigNull;
   }
   void Assign0() {
      this->Value_ = 0;
   }

   /// 由另一種 Decimal<> 填入值.
   /// - 若 sizeof(OrigType) < sizeof(SrcI) 可能會有 compile warning: conversion from 'SrcI=__int64' to 'OrigType=short', possible loss of data.
   /// - 若 this->Scale < srcScale 則四捨五入.
   template <typename SrcI, DecScaleT srcScale, class AdjustDecScaleEx = AdjustDecScaleExDefault>
   void Assign(Decimal<SrcI, srcScale> src, AdjustDecScaleEx ex = AdjustDecScaleEx{}) {
      this->Value_ = static_cast<OrigType>(AdjustDecScale<srcScale, Scale>(src.GetOrigValue(), ex));
   }
   /// 填入指定小數位數的值.
   /// 例如:
   /// \code 
   ///   DecScaleT srcScale;
   ///   ...取得srcScale...
   ///   fdnum.Assign(123456, srcScale);
   /// \endcode
   template <typename SrcI, class AdjustDecScaleEx = AdjustDecScaleExDefault>
   void Assign(SrcI src, DecScaleT srcScale, AdjustDecScaleEx ex = AdjustDecScaleEx{}) {
      this->Value_ = static_cast<OrigType>(AdjustDecScale(src, srcScale, this->Scale, ex));
   }
   /// 填入指定小數位數的值.
   /// 例如: Assign<3>(123456); 表示填入 123.456
   template <DecScaleT srcScale, typename SrcI, class AdjustDecScaleEx = AdjustDecScaleExDefault>
   void Assign(SrcI src, AdjustDecScaleEx ex = AdjustDecScaleEx{}) {
      this->Value_ = static_cast<OrigType>(AdjustDecScale<srcScale, Scale>(src, ex));
   }

   /// 用 F(float,double,long double...) 四捨五入後填入.
   /// **不檢查** 結果是否 underflow 或 overflow.
   template <typename F>
   void AssignRound (F value) {
      this->Value_ = this->RoundToOrgI(value);
   }

   /// 取出整數部分數字, **沒有** 四捨五入.
   /// 例: `123.45` 傳回 `123`
   constexpr OrigType GetIntPart() const {
      return this->Value_ / this->Divisor;
   }
   constexpr OrigType GetDecPart() const {
      return DecimalAux<IntTypeT>::RemoveIntPart(this->Value_, Divisor);
   }

   constexpr friend Decimal RemoveIntPart (Decimal value) {
      return Decimal{DecimalAux<IntTypeT>::RemoveIntPart(value.Value_, Divisor)};
   }
   /// 移除小數部分, 沒有四捨五入.
   constexpr friend Decimal RemoveDecPart (Decimal value) {
      return Decimal{DecimalAux<IntTypeT>::RemoveDecPart(value.Value_, Divisor)};
   }
   constexpr OrigType GetOrigValue() const {
      return this->Value_;
   }
   /// 設定原始資料, 不考慮 src 的小數位, 使用上請注意.
   void SetOrigValue(OrigType src) {
      this->Value_ = src;
   }

   /// 轉成 F(float,double,long double...).
   /// 請注意: 可能遺失精確度, 也可能超過 F 的最大、最小值!
   /// 用法: `v.To<double>();` 在多層的 template 裡面可能需要 `v.template To<double>();`
   template <typename F>
   constexpr F To() const {
      static_assert(std::is_floating_point<F>::value, "To<F>(), F必須是浮點數型別");
      return static_cast<F>(this->Value_) / this->Divisor;
   }

   constexpr friend bool operator==(const Decimal& lhs, const Decimal& rhs) {
      return lhs.Value_ == rhs.Value_;
   }
   constexpr friend bool operator<(const Decimal& lhs, const Decimal& rhs) {
      return lhs.Value_ < rhs.Value_;
   }
   constexpr int Compare(Decimal rhs) const {
      return (this->Value_ < rhs.Value_) ? -1 : (this->Value_ == rhs.Value_) ? 0 : 1;
   }

   /// 四則運算說明: 為了降低精確度轉換的複雜及不確定性，所以僅支援底下運算.
   /// 其餘運算(乘除、其他小數位)則建議使用 To<double>() 轉成 double 之後計算.
   /// - 加減運算僅支援相同 IntTypeT、ScaleN 的 Decimal
   /// - 乘法運算僅支援 `Decimal<IntTypeT,ScaleN> * IntTypeT` 或 `IntTypeT * Decimal<IntTypeT,ScaleN>`
   /// - 除法運算僅支援 `Decimal<IntTypeT,ScaleN> / IntTypeT`
   constexpr friend Decimal operator+ (Decimal lhs, Decimal rhs) {
      return Decimal{lhs.Value_ + rhs.Value_};
   }
   Decimal& operator+=(Decimal rhs) {
      this->Value_ += rhs.Value_;
      return *this;
   }

   constexpr friend Decimal operator- (Decimal lhs, Decimal rhs) {
      return Decimal{lhs.Value_ - rhs.Value_};
   }
   Decimal& operator-=(Decimal rhs) {
      this->Value_ -= rhs.Value_;
      return *this;
   }

   constexpr friend Decimal operator* (Decimal lhs, IntTypeT rhs) {
      return Decimal{lhs.Value_ * rhs};
   }
   constexpr friend Decimal operator* (IntTypeT lhs, Decimal rhs) {
      return Decimal{lhs * rhs.Value_};
   }
   Decimal& operator*=(IntTypeT rhs) {
      this->Value_ *= rhs;
      return *this;
   }

   constexpr friend Decimal operator/ (Decimal lhs, IntTypeT rhs) {
      return Decimal{lhs.Value_ / rhs};
   }
   Decimal& operator/=(IntTypeT rhs) {
      this->Value_ /= rhs;
      return *this;
   }

   constexpr friend Decimal operator% (Decimal lhs, Decimal rhs) {
      return Decimal{lhs.Value_ % rhs.Value_};
   }
   Decimal& operator%=(Decimal rhs) {
      this->Value_ %= rhs.Value_;
      return *this;
   }
};

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// Decimal數字轉成浮點數.
template <typename F, typename IntTypeT, DecScaleT ScaleN>
constexpr F To(const Decimal<IntTypeT, ScaleN>& value) {
   return value.To<double>();
}

/// \ingroup AlNum
/// Decimal數字轉成字串.
template <typename IntTypeT, DecScaleT ScaleN>
inline char* ToStrRev(char* pout, const Decimal<IntTypeT, ScaleN>& value) {
   return DecToStrRev(pout, value.GetOrigValue(), ScaleN);
}

template <typename IntTypeT, DecScaleT ScaleN>
inline char* ToStrRev(char* pout, const Decimal<IntTypeT, ScaleN>& value, const FmtDef& fmt) {
   return DecToStrRev(pout, abs_cast(value.GetOrigValue()), value.GetOrigValue() < 0, ScaleN, fmt);
}

template <typename IntTypeT, DecScaleT ScaleN, class AuxT = StrToDecIntAux<IntTypeT>, class ResultT = Decimal<IntTypeT, ScaleN>>
inline ResultT StrTo(const StrView& str, ResultT value = ResultT::Null(), const char** endptr = nullptr) {
   return ResultT::Make<ScaleN>(StrToDec(str, ScaleN, value.GetOrigValue(), endptr));
}

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
template <typename IntTypeT>
struct IntScale {
   IntTypeT    OrigValue_;
   DecScaleT   Scale_;
};
fon9_WARN_POP;

template <typename IntTypeT>
inline char* ToStrRev(char* pout, const IntScale<IntTypeT>& value) {
   return DecToStrRev(pout, value.OrigValue_, value.Scale_);
}

template <typename IntTypeT>
inline char* ToStrRev(char* pout, const IntScale<IntTypeT>& value, const FmtDef& fmt) {
   return DecToStrRev(pout, abs_cast(value.OrigValue_), value.OrigValue_ < 0, value.Scale_, fmt);
}

} // namespaces
#endif//__fon9_Decimal_hpp__
