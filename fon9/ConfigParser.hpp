/// \file fon9/ConfigParser.cpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ConfigParser_hpp__
#define __fon9_ConfigParser_hpp__
#include "fon9/StrTools.hpp"
#include "fon9/buffer/RevBuffer.hpp"

namespace fon9 {

/// \ingroup Misc
/// 解析設定內容.
/// - 格式內容為: tag=value|tag=value|tag=value|...
/// - 可自訂分隔符號 '=', '|'
/// - tag 不可有括號、引號, 忽略 tag 前後空白
/// - value 可以有括號、引號 忽略 value 前後空白
class fon9_API ConfigParser {
   fon9_NON_COPY_NON_MOVE(ConfigParser);
public:
   ConfigParser() = default;
   virtual ~ConfigParser();

   enum class Result {
      Success,
      EUnknownTag,
      EValueTooLarge,
      EValueTooSmall,
      /// 無效的 value: value 的格式有誤?
      EInvalidValue,
   };

   fon9_WARN_DISABLE_PADDING;
   struct ErrorEventArgs {
      StrView     Tag_;
      StrView     Value_;
      const char* ErrPos_;
      Result      Result_;
   };
   fon9_WARN_POP;

   /// - 透過 OnTagValue() 解析訊息.
   /// - 若返回值不是 Result::Success, 則 cfgstr.begin() 為發現錯誤的位置.
   /// - 若 OnTagValue() 返回錯誤, 則會透過 OnErrorBreak() 處理.
   Result Parse(StrView& cfgstr, char chFieldDelim = '|', char chEqual = '=');

   /// 處理取出的欄位, 若 value 有錯, 則返回時 value.begin() 應指向錯誤的位置.
   /// 若欄位沒有 chEqual, 則 value.begin() == nullptr;
   virtual Result OnTagValue(StrView tag, StrView& value) = 0;

   /// 預設傳回 true;
   /// \retval true  發現錯誤, 中斷解析.
   /// \retval false 錯誤已處理, 繼續解析.
   virtual bool OnErrorBreak(ErrorEventArgs& e);
};

/// \ingroup Misc
/// "e.Result_:EMsg @X [e.Tag_ = e.Value_]"
/// X = position from tag.begin().
fon9_API void RevPrint(RevBuffer& rbuf, const ConfigParser::ErrorEventArgs& e);

class fon9_API ConfigParserMsg : public ConfigParser {
   fon9_NON_COPY_NON_MOVE(ConfigParserMsg);
   RevBuffer&  RBuf_;
public:
   ConfigParserMsg(RevBuffer& rbuf) : RBuf_(rbuf) {
   }
   /// 將錯誤訊息寫入 RBuf_;
   /// 返回 false;
   bool OnErrorBreak(ErrorEventArgs& e) override;
};

/// \ingroup Misc
/// 使用 T::OnTagValue() 解析設定值, 若有錯誤訊息則填入 rbuf.
/// \retval true  rbuf 為空, 解析沒有問題.
/// \retval false rbuf 有錯誤訊息.
template <class T>
bool ParseConfig(T& dst, StrView cfgstr, RevBuffer& rbuf) {
   struct Parser : ConfigParserMsg {
      fon9_NON_COPY_NON_MOVE(Parser);
      T& Dst_;
      Parser(T& dst, RevBuffer& rbuf) : ConfigParserMsg{rbuf}, Dst_(dst) {
      }
      Result OnTagValue(StrView tag, StrView& value) override {
         return this->Dst_.OnTagValue(tag, value);
      }
   };
   const char* rbufprev = rbuf.GetCurrent();
   Parser{dst, rbuf}.Parse(cfgstr);
   return rbuf.GetCurrent() == rbufprev;
}

} // namespace
#endif//__fon9_ConfigParser_hpp__
