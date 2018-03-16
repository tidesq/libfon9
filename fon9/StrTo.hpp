/// \file fon9/StrTo.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_StrTo_hpp__
#define __fon9_StrTo_hpp__
#include "fon9/StrView.hpp"
#include "fon9/StrTools.hpp"
#include "fon9/DecBase.hpp"

namespace fon9 {

/// \ingroup AlNum
/// StrToNum() 處理過程的相關資料.
struct StrToNumCursor {
   fon9_NON_COPY_NON_MOVE(StrToNumCursor);

   /// 移除「開頭正負號前後空白 & 正負號」之後的第一個字元位置.
   const char*       OrigBegin_;
   /// 原始 SetToNum(str) 的 str.end();
   const char* const OrigEnd_;
   /// 預期的結束位置.
   /// 例: Decimal<int,3> 則預期小數只需要3位, 在遇到小數點時調整 ExpectEnd_.
   const char*       ExpectEnd_;
   /// 正在處理的位置.
   const char*       Current_;

   constexpr StrToNumCursor(const char* origBegin, const char* origEnd)
      : OrigBegin_{origBegin}
      , OrigEnd_{origEnd}
      , ExpectEnd_{origEnd}
      , Current_{origBegin} {
   }
   /// 當在 AuxT::IsValidChar() 裡面呼叫 cur.Rollbacl() 並返回 false:
   /// - 中斷 StrToNum(), 並且在 StrToNum() 返回前 **不呼叫** AuxT.MakeResult(res, cur); 不改變 out 的值
   /// - 且 endptr = 移除開頭正負號及空白之後的位置.
   void Rollback() {
      this->Current_ = this->OrigBegin_;
   }
   /// 當在 AuxT::IsValidChar() 裡面呼叫 cur.FormatErrorAtCurrent() 並返回 false:
   /// - 中斷 StrToNum(), 並且在 StrToNum() 返回前 **不呼叫** AuxT.MakeResult(res, cur); 不改變 out 的值
   /// - 且 endptr = cur.Current_
   void FormatErrorAtCurrent() {
      this->OrigBegin_ = this->Current_;
   }
};

/// \ingroup AlNum
/// 指出 StrToNum() 目前正在解析的部分.
enum class StrToNumStep {
   /// 整數部分.
   Integer,
   /// 小數部分.
   Decimal,
};

/// \ingroup AlNum
/// 當 StrToNum() 呼叫 AuxT::OnOverflow() 時,
/// AuxT::OnOverflow() 返回: 決定接下來的處理方式.
enum class StrToNumOverflow {
   /// 從下一個字元繼續解析.
   ContinueParse,
   /// 中斷解析.
   /// 不會呼叫 AuxT::OnParseNormalEnd().
   BreakParse,
   /// 移除 [緊接著的數字 or AuxT::IsValidChar()==true] 後, 結束解析.
   /// 不會呼叫 AuxT::OnParseNormalEnd().
   RemoveFollowDigit,
};

fon9_WARN_DISABLE_PADDING;
/// \ingroup AlNum
/// 字串轉數字.
/// - 為何不用 `strtol()`、`strtod()`? 因為他們都必須用 EOS 字串!
///   - c++17 才開始支援: `std::from_chars()` 無 EOS 的轉換.
///   - 包含在結構裡的 PIC9 沒有EOS, 而(而根據經驗)這種資料又經常用到!
/// - 轉換的結果是: `typename AuxT::ResultT`
/// - 轉換過程使用: `typename AuxT::TempResultT res;`
/// - 正負號前後可以有空白.
/// - `bool AuxT::CheckSign(char ch1st);` 決定是否有正負號.
/// - `bool AuxT::IsValidChar(res, cur);` 決定非數字字元如何處理.
/// - 如果發生了溢位, 則會呼叫 `AuxT::OnOverflow(res, cur);` 此時可決定接下來的行為:
///   是否要繼續解析? 或丟出例外? 或在 aux 記錄情況後結束解析...
/// - 如果沒遇到無效字元, 沒遇到溢位, 則解析到最後會呼叫 `AuxT::OnParseNormalEnd(res, cur);`
/// - 解析結束後, str 移動到最後解析位置.
/// - 最後透過 `typename AuxT::ResultT AuxT::MakeResult(res, cur);` 返回結果.
///
/// \param str 要轉成數字的字串參數.
/// \param aux AuxT 可參考 `struct StrToSIntAux;`
/// \param out 字串轉出的結果:
///            - 若 str 為空字串、僅有空白字元、符號字元(`+`、`-`)前後僅有空白，則:
///              res 內容不變, 此時返回 str.end();
///
/// \return 第一個無法轉換的字元位置, 或 str.end();
template <class AuxT>
const char* StrToNum(StrView str, AuxT& aux, typename AuxT::ResultT& out) {
   (void)aux; // 避免 VC: warning C4100: 'aux': unreferenced formal parameter
   const char* pbeg = StrFindIf(str, isnotspace);
   if (fon9_UNLIKELY(pbeg == nullptr))
      return str.end();
   if (aux.CheckSign(*pbeg)) {
      if ((pbeg = StrFindIf(pbeg + 1, str.end(), isnotspace)) == nullptr)
         return str.end();
   }
   StrToNumCursor cur(pbeg, str.end());

   using TempResultT = typename AuxT::TempResultT;
   TempResultT             res = 0;
   constexpr TempResultT   maxA = static_cast<TempResultT>(std::numeric_limits<TempResultT>::max() / 10);
   constexpr unsigned char maxB = static_cast<unsigned char>(std::numeric_limits<TempResultT>::max() % 10);
   do {
      unsigned char digval = static_cast<unsigned char>(*cur.Current_ - '0');
      if (fon9_LIKELY(digval <= 9)) {
         if (fon9_LIKELY(res < maxA || (res == maxA && digval <= maxB)))
            res = static_cast<TempResultT>(res * 10 + digval);
         else { // overflow!
            switch (aux.OnOverflow(res, cur)) {
            case StrToNumOverflow::RemoveFollowDigit:
               while (++cur.Current_ < cur.OrigEnd_) {
                  if (static_cast<unsigned char>(*cur.Current_ - '0') > 9) {
                     if (!aux.IsValidChar(res, cur))
                        break;
                  }
               }
               goto __BREAK_PARSE;// 移除[緊接著的數字]後, 結束解析, 返回結果.
            case StrToNumOverflow::BreakParse:
               goto __BREAK_PARSE;
            case StrToNumOverflow::ContinueParse:
               break;
            }
         }
      }
      else {
         if (!aux.IsValidChar(res, cur))
            goto __BREAK_PARSE;
      }
   } while (++cur.Current_ < cur.ExpectEnd_);
   aux.OnParseNormalEnd(res, cur);

__BREAK_PARSE:
   if (fon9_LIKELY(cur.Current_ != cur.OrigBegin_))
      out = aux.MakeResult(res, cur);
   return cur.Current_;
}

/// \ingroup AlNum
/// 輔助 StrToNum() 轉成「整數」的類別基底.
template <typename IntType>
struct StrToIntAuxBase {
   /// 檢查型別是否正確.
   static_assert(std::numeric_limits<IntType>::is_integer, "StrToIntBase<IntType>, IntType必須是整數型別");
   /// 結果型別.
   using ResultT = IntType;
   /// 轉換過程用的型別.
   using TempResultT = typename std::make_unsigned<IntType>::type;
   /// 現在正在解析哪部分? 整數解析, 永遠只傳回 StrToNumStep::Integer.
   static constexpr StrToNumStep GetParseStep() {
      return StrToNumStep::Integer;
   }
   /// 是否為有效的非數字字元? 若要排除千位分隔, 可在此判斷.
   /// 當發現無效字元時, 也可在此改變預設結束位置, 例如: 超過小數位就不用再解析了.
   static constexpr bool IsValidChar(TempResultT& /*res*/, StrToNumCursor& /*cur*/) {
      return false;
   }
   /// 解析正常到字串尾端: 預設 do nothing.
   /// 通常會與 IsValidChar() 配合處理, 如果因遇到無效字元而結束解析, 則不會呼叫到這.
   static constexpr void OnParseNormalEnd(TempResultT& /*res*/, StrToNumCursor& /*cur*/) {
   }
};

/// \ingroup AlNum
/// 輔助 StrToNum() 轉成「有正負整數」的類別.
template <typename SignedIntType>
class StrToSIntAux : public StrToIntAuxBase<SignedIntType> {
   using base = StrToIntAuxBase<SignedIntType>;
   /// 檢查型別是否正確.
   static_assert(std::is_signed<SignedIntType>::value,
                 "StrToSInt<SignedIntType>, SignedIntType 必須是有正負的整數型別");
public:
   using typename base::TempResultT;
   using typename base::ResultT;
   /// 是否為負數, 在 CheckSign() 時設定.
   enum {
      /// 負數.
      IsNegative,
      /// overflow 之後應傳回最小負數.
      IsNegativeMin,
      /// 正數.
      IsPositive,
   }  Negative_;
   /// 檢查正負號: 傳回true=有正負號, false=無正負號.
   bool CheckSign(char ch1st) {
      switch (ch1st) {
      case '+':  this->Negative_ = IsPositive; return true;
      case '-':  this->Negative_ = IsNegative; return true;
      default:   this->Negative_ = IsPositive; return false;
      }
   }
   /// 處理 *cur.Current_ 的數字時發生了溢位.
   ///   - 正數: 將 res 設定成 IntType 最大值, 負數: 設定 Negative_ 旗標 = IsNegativeMin.
   ///   - 傳回: StrToNumOverflow::RemoveFollowDigit
   StrToNumOverflow OnOverflow(TempResultT& res, StrToNumCursor& /*cur*/) {
      if (this->Negative_ == IsNegative)
         this->Negative_ = IsNegativeMin;
      else
         res = static_cast<TempResultT>(std::numeric_limits<ResultT>::max());
      return StrToNumOverflow::RemoveFollowDigit;
   }
   /// 提供給 StrToDecimal::OnOverflow() 四捨五入進位使用.
   void OnOverflowInc(TempResultT& res) {
      if (res < static_cast<TempResultT>(std::numeric_limits<ResultT>::max()))
         ++res;
      else
         if (this->Negative_ == IsNegative)
            this->Negative_ = IsNegativeMin;
   }
   /// 取得最終結果.
   ResultT MakeResult(TempResultT res, StrToNumCursor& /*cur*/) const {
      switch (this->Negative_) {
      default:
      case IsPositive:
         if (res > static_cast<TempResultT>(std::numeric_limits<ResultT>::max()))
            return std::numeric_limits<ResultT>::max();
         return static_cast<ResultT>(res);
      case IsNegative:
         if (res > static_cast<TempResultT>(std::numeric_limits<ResultT>::max()))
            return std::numeric_limits<ResultT>::min();
         return static_cast<ResultT>(-static_cast<ResultT>(res));
      case IsNegativeMin:
         return std::numeric_limits<ResultT>::min();
      }
   }
};

/// \ingroup AlNum
/// 輔助 StrToNum() 轉成「無正負整數」的類別.
template <typename UnsignedIntType>
class StrToUIntAux : public StrToIntAuxBase<UnsignedIntType> {
   using base = StrToIntAuxBase<UnsignedIntType>;
   /// 檢查型別是否正確.
   static_assert(std::is_unsigned<UnsignedIntType>::value, "StrToUInt<UnsignedIntType>, UnsignedIntType必須是無正負的整數型別");
public:
   using typename base::TempResultT;
   using typename base::ResultT;
   /// 檢查正負號: 傳回true=有正號, false=無正號.
   static constexpr bool CheckSign(char ch1st) {
      return (ch1st == '+');
   }
   /// 處理 *cur.Current_ 的數字時發生了溢位.
   ///   - 將 res 設定成 IntType 最大值.
   ///   - 傳回: StrToNumOverflow::RemoveFollowDigit
   static StrToNumOverflow OnOverflow(TempResultT& res, StrToNumCursor& /*cur*/) {
      res = std::numeric_limits<TempResultT>::max();
      return StrToNumOverflow::RemoveFollowDigit;
   }
   /// 提供給 StrToDecimal::OnOverflow() 四捨五入進位使用.
   static void OnOverflowInc(TempResultT& res) {
      if (res < std::numeric_limits<ResultT>::max())
         ++res;
   }
   /// 取得最終結果.
   static constexpr ResultT MakeResult(TempResultT res, StrToNumCursor& /*cur*/) {
      return static_cast<ResultT>(res);
   }
};

/// \ingroup AlNum
/// 自動依照 IntType 是否有正負, 選擇 StrToSInt<IntType> 或 StrToUInt<IntType>.
template <class IntType>
using StrToIntAux = conditional_t<std::is_signed<IntType>::value, StrToSIntAux<IntType>, StrToUIntAux<IntType>>;

/// \ingroup AlNum
/// 輔助 StrToNum() 轉成「Decimal整數」的類別.
/// 例如: "123.45", 建構時指定 ExpectScale_=3, 則得到的結果=123450
template <class IntTypeT>
class StrToDecIntAux : public StrToIntAux<IntTypeT> {
   fon9_NON_COPY_NON_MOVE(StrToDecIntAux);
   using base = StrToIntAux<IntTypeT>;
public:
   using typename base::TempResultT;
   using typename base::ResultT;
   /// 小數點位置, 取得計算結果時, 計算小數位使用. 在 CheckSign() 時清除.
   const char*       PointPos_;
   /// 字串的小數位數. 在 CheckSign() 時清除(設為-1).
   int               Scale_;
   /// 結果的小數位數.
   const DecScaleT   ExpectScale_;
   /// 建構時需指定結果的小數位數.
   StrToDecIntAux(DecScaleT expectScale) : ExpectScale_{expectScale} {
   }
   /// 現在正在解析哪部分, 整數解析, 永遠只傳回 StrToNumStep::Integer.
   constexpr StrToNumStep GetParseStep() const {
      return this->PointPos_ ? StrToNumStep::Decimal : StrToNumStep::Integer;
   }
   /// 清除上次結果 & 呼叫底層的 CheckSign().
   bool CheckSign(char ch1st) {
      this->PointPos_ = nullptr;
      this->Scale_ = -1;
      return base::CheckSign(ch1st);
   }
   /// 是否為有效的非數字字元.
   bool IsValidChar(TempResultT& /*res*/, StrToNumCursor& cur) {
      if (this->PointPos_) {
         this->SetDecimalEnd(cur.Current_);
         return false;
      }
      /// 尚未開始解析小數, 只允許小數點.
      if (*cur.Current_ != NumPunct::GetCurrent().DecPoint_)
         return false;
      this->SetPointPos(cur);
      return true;
   }
   /// 記錄小數點位置 & 開始解析小數部分.
   void SetPointPos(StrToNumCursor& cur) {
      // +1 = 小數點佔位, (四捨五入: 若沒有overflow, 則在 OnParseNormalEnd() 處理).
      const char* icur = (this->PointPos_ = cur.Current_) + this->ExpectScale_ + 1;
      if (cur.ExpectEnd_ > icur)
         cur.ExpectEnd_ = icur;
   }
   /// 正在解析小數部分, 遇到非數字就表示小數解析結束, 此時可得知字串的小數位數.
   void SetDecimalEnd(const char* icur) {
      if (this->Scale_ < 0)
         this->Scale_ = static_cast<int>(icur - this->PointPos_ - 1);
   }
   /// 處理 *cur.Current_ 的數字時發生了溢位.
   ///   - 若正在處理整數: 透過 base::OnOverflow() 將 res 設定成最大(或最小)值.
   ///   - 若正在處理小數: 則四捨五入.
   ///   - 傳回: StrToNumOverflow::RemoveFollowDigit
   StrToNumOverflow OnOverflow(TempResultT& res, StrToNumCursor& cur) {
      if (this->PointPos_ == nullptr) {
         this->Scale_ = this->ExpectScale_;
         base::OnOverflow(res, cur);
      }
      else {
         this->Scale_ = static_cast<int>(cur.Current_ - this->PointPos_ - 1);
         if (*cur.Current_ >= '5')
            base::OnOverflowInc(res);
      }
      return StrToNumOverflow::RemoveFollowDigit;
   }
   /// 解析正常到字串尾端: 計算小數位置.
   void OnParseNormalEnd(TempResultT& res, StrToNumCursor& cur) {
      if (this->PointPos_ == nullptr)
         return;
      this->Scale_ = static_cast<int>(cur.Current_ - this->PointPos_ - 1);
      if (cur.Current_ >= cur.OrigEnd_)
         return;
      unsigned char digval = static_cast<unsigned char>(*cur.Current_ - '0');
      if (5 <= digval && digval <= 9)
         base::OnOverflowInc(res);
      for (;;) {
         if (digval > 9)
            break;
         if (++cur.Current_ >= cur.OrigEnd_)
            break;
         digval = static_cast<unsigned char>(*cur.Current_ - '0');
      }
   }
   /// 取得最終結果.
   ResultT MakeResult(TempResultT res, StrToNumCursor& cur) const {
      return AdjustDecScale<ResultT, ResultT>(this->GetOrigResult(res, cur),
                            static_cast<DecScaleT>(this->Scale_ < 0 ? 0 : this->Scale_),
                            this->ExpectScale_, AdjustDecScaleExLimit{});
   }
protected:
   /// 透過底層取得結果(不考慮小數位的整數).
   constexpr ResultT GetOrigResult(TempResultT res, StrToNumCursor& cur) const {
      return base::MakeResult(res, cur);
   }
};
/// \ingroup AlNum
/// 輔助 StrToNum() 轉成「Decimal<>」的類別.
template <class DecimalT>
class StrToDecimalAux : public StrToDecIntAux<typename DecimalT::OrigType> {
   using base = StrToDecIntAux<typename DecimalT::OrigType>;
   fon9_NON_COPY_NON_MOVE(StrToDecimalAux);
public:
   StrToDecimalAux() : base{DecimalT::Scale} {
   }
   /// 結果型別.
   using ResultT = DecimalT;
   /// 取得最終結果.
   ResultT MakeResult(typename base::TempResultT res, StrToNumCursor& cur) const {
      return ResultT{this->GetOrigResult(res, cur),
         static_cast<DecScaleT>(this->Scale_ < 0 ? 0 : this->Scale_),
         AdjustDecScaleExLimit{}};
   }
};

/// \ingroup AlNum
/// 可排除千位分隔字元 ',' 的 StrToNum() 輔助類別.
/// 不考慮分隔符號的位置是否正確.
template <class AuxT>
struct StrToSkipIntSepAux : public AuxT {
   fon9_NON_COPY_NON_MOVE(StrToSkipIntSepAux);

   const char  IntSepChar_;
   StrToSkipIntSepAux(char chIntSep = NumPunct::GetCurrent().IntSep_) : IntSepChar_(chIntSep) {
   }
   /// 是否為有效的非數字字元: 若 *cur.Current_==',' 則傳回 true, 有效字元, 需要繼續解析.
   bool IsValidChar(typename AuxT::TempResultT& res, StrToNumCursor& cur) {
      if (AuxT::IsValidChar(res, cur))
         return true;
      if (this->GetParseStep() == StrToNumStep::Integer)
         return (*cur.Current_ == this->IntSepChar_);
      return false;
   }
};

template <typename NumT>
using AutoStrToAux = conditional_t<std::numeric_limits<NumT>::is_integer, StrToIntAux<NumT>, StrToDecimalAux<NumT>>;
fon9_WARN_POP;

// 把 輔助類別 放進 libfon9.dll, 減少程式碼的大小.
// StrToUIntAux 裡面的 member functions 很單純, 直接 inline 呼叫會比較快, 所以不需要放到 dll.
template class fon9_API StrToSIntAux<int32_t>;
template class fon9_API StrToSIntAux<int64_t>;

template class fon9_API StrToDecIntAux<int32_t>;
template class fon9_API StrToDecIntAux<uint32_t>;
template class fon9_API StrToDecIntAux<int64_t>;
template class fon9_API StrToDecIntAux<uint64_t>;

/// \ingroup AlNum
/// 字串轉數字的簡單版: 自動選擇輔助類別.
template <typename NumT, class AuxT = AutoStrToAux<NumT>>
inline NumT StrTo(const StrView& str, NumT value = NumT{}, const char** endptr = nullptr) {
   AuxT        aux{};
   const char* pend = StrToNum(str, aux, value);
   if (endptr)
      *endptr = pend;
   return value;
}

template <typename NumT, class AuxT = StrToDecIntAux<NumT>>
inline NumT StrToDec(const StrView& str, DecScaleT decScale, NumT value = NumT{}, const char** endptr = nullptr) {
   AuxT        aux{decScale};
   const char* pend = StrToNum(str, aux, value);
   if (endptr)
      *endptr = pend;
   return value;
}

/// \ingroup AlNum
/// 移除前方空白, 及空白後的第一個 'x' or 'X', 沒有 'x' 也沒關係.
/// \return hexstr 的16進位數值.
fon9_API uintmax_t HexStrTo(StrView hexstr, const char** endptr = nullptr);
/// \ingroup AlNum
/// - 如果第一碼為 'x' or 'X', 表示 value 為 16 進為字串, 轉成16進位數值返回.
/// - 如果為 "0x" or "0X" 開頭, 表示 value 為 16 進為字串, 轉成16進位數值返回.
/// - 其他使用10進位轉出.
fon9_API uintmax_t HIntStrTo(StrView str, const char** endptr = nullptr);

/// \ingroup AlNum
/// 單純數字字串轉成正整數, 一開始就必須是數字(開頭不可有: 正負號、空白字元), 處理到 pend 或非數字字元為止.
/// 不考慮 Overflow.
fon9_API uintmax_t NaiveStrToUInt(const char* pbeg, const char* pend, const char** endptr = nullptr);
inline uintmax_t NaiveStrToUInt(const StrView& str, const char** endptr = nullptr) {
   return NaiveStrToUInt(str.begin(), str.end(), endptr);
}
/// \ingroup AlNum
/// 單純數字字串轉成有號整數, 第一個字元可以是正負號, 其餘限制與 NaiveStrToUInt() 相同.
fon9_API intmax_t NaiveStrToSInt(const char* pbeg, const char* pend, const char** endptr = nullptr);
inline intmax_t NaiveStrToSInt(const StrView& str, const char** endptr = nullptr) {
   return NaiveStrToSInt(str.begin(), str.end(), endptr);
}

} // namespace fon9
#endif//__fon9_StrTo_hpp__
