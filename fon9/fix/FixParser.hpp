/// \file fon9/fix/FixParser.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_FixParser_hpp__
#define __fon9_fix_FixParser_hpp__
#include "fon9/fix/FixBase.hpp"
#include "fon9/CharVector.hpp"
#include "fon9/LevelArray.hpp"
#include <vector>

namespace fon9 { namespace fix {

/// \ingroup fix
/// Fix Message 解析器.
/// - 解析後的結果, 透過 GetField() 取得,
///   - 結果的 Values_[] 字串, 直接指向原始 fixmsg 的內容(沒有任何複製及改變).
///   - 所以在還需要 GetValue() 之前, fixmsg 必須保持不變.
/// - 使用 fon9::LevelArray 儲存解析後的欄位物件
///   - 保留使用過的欄位, 不會經常分配記憶體, 因此記憶體用量較大.
///   - 所以: 請重複使用此物件, 才能發揮最大效益.
///   - 解析完畢後需立即處理: 例如: 填入「應用層」的處理物件(例如:下單要求).
///   - 通常一個 FixSession 擁有一個 FixParser 處理解析後的結果.
///   - 直到 FixSession 結束後釋放 FixParser.
/// - 不解析 "8=BeginString" 及 "9=BodyLength", 所以 GetField(8) 及 GetField(9) 都傳回 nullptr.
class fon9_API FixParser {
   fon9_NON_COPY_NON_MOVE(FixParser);
public:
   using ExpectSize = uint16_t;
   using FixValue = StrView;
   enum : uint16_t {
      /// 允許欄位重複次數, 1 = 允許重複1次, 也就是相同欄位最多可出現2次.
      kMaxDupFieldCount = 4
   };

   struct FixField {
      FixTag      Tag_;
      /// 當同一個欄位重複出現, 則透過此處找後續出現的 values.
      /// 然後透過 `StrView FixParser::GetValue(const FixField& fld, unsigned index);` 取得後續出現的 value.
      uint16_t    MIndex_;
      /// 欄位在 FIX Message 出現的次數.
      uint16_t    ValueCount_{0};
      /// 欄位在 FIX Message 首次出現的值.
      FixValue    Value_;
   };

   FixParser();

   /// 清除上次解析的欄位 & expect size.
   /// 之後的 GetField() 都會傳回 nullptr.
   void Clear();
   /// 設定期望的 header("8=FIX.m.n|9=").
   /// 不考慮 expectHeader 的合理性, 一旦設定為非 empty():
   /// - 後續的 Parse() 就會判斷 fixmsg 的前方是否為 expectHeader:
   ///   - 如果不是, 就直接返回 EInvalidHeader
   ///   - 如果是, 則直接取得 bodyLength
   void ResetExpectHeader(const StrView& expectHeader = StrView{}) {
      this->ExpectHeader_.assign(expectHeader);
   }
   const CharVector& GetExpectHeader() const {
      return this->ExpectHeader_;
   }
   /// 期望的訊息長度, 包含 header & body & tailer.
   /// - 在 Clear() 時清為 0.
   /// - 在 Verify() 時設定正確的長度.
   ExpectSize GetExpectSize() {
      return this->ExpectSize_;
   }

   /// FixParser 處理的結果.
   /// >NeedsMore 表示解析成功的字元用量.
   enum Result {
      EInvalidHeader = -1,
      /// 資料格式錯誤, 可從 fixmsg.begin() 取得錯誤位置.
      EFormat = -2,
      /// 重複欄位超過可儲存的大小, 可從 fixmsg.begin() 取得錯誤位置.
      EDupField = -3,
      /// 錯誤的 RawData or RawDataLength
      ERawData = -4,
      /// 當資料大小超過 kFixMaxBodyLength 則視為錯誤.
      EOverFixMaxBodyLength = -5,
      /// check sum 錯誤, 可從 GetExpectSize() 取得此筆訊息長度.
      ECheckSum = -10,
      /// 資料長度不足, 無法解析.
      /// 還需要收到更多的資料後重新解析.
      /// 此時可以從 GetExpectSize() 取得期望的資料長度.
      NeedsMore = 0,
      ParseEnd = 1,
   };

   /// Verify(): 要檢查的項目.
   enum VerifyItem {
      /// 僅取出 bodyLength & 計算訊息全長, 不判斷 header & check sum.
      VerifyLengthOnly = 0x00,
      /// 如果 !GetExpectHeader().empty() 表示要檢查 header 是否==GetExpectHeader()?
      VerifyHeader = 0x01,
      VerifyCheckSum = 0x02,
      VerifyAll = VerifyHeader | VerifyCheckSum,
   };
   /// \retval <NeedsMore 表示訊息有問題, fixmsg.begin() 為問題發生的地方.
   /// \retval =ECheckSum 表示 check sum 有錯, GetExpcetSize() 仍可取得此筆訊息的長度.
   /// \retval =NeedsMore 表示目前長度不足, 請長度 >=GetExpcetSize() 時再來檢查一次.
   /// \retval >NeedsMore 則表示 fixmsg 長度足夠 & check sum 正確, 返回值 = 完整訊息的長度.
   ///                    此時 **不會** 設定 ExpcetSize.
   ///   \code
   ///                      ________________________________ 
   ///      return length: /                                \ 包含頭尾的長度.
   ///      fixmsg input:  8=FIX.4.4|9=123|FIX Fields|10=xxx|
   ///      fixmsg output:                 \________/ 不含頭尾的分隔符號.
   ///      expect header: \__________/ = "8=FIX.4.4|9="
   ///      若呼叫前 GetExpectHeader().empty(), 則會設定 expect header.
   ///   \endcode
   Result Verify(StrView& fixmsg, VerifyItem vitem);

   /// Parse(): 需要的欄位都解析了, 就可以結束.
   enum class Until {
      FullMessage = 0x0,
      MsgSeqNum = 0x01,
      MsgType = 0x02,
      SendingTime = 0x04,
   };
   /// - 解析訊息的步驟:
   ///   - 不排除開頭的空白, 所以開頭有空白會造成解析錯誤.
   ///   - header & bodyLength:
   ///     - 解析 header: "8=FIX.n.m|9=";
   ///     - 取得 bodyLength.
   ///     - 若之前解析過訊息:
   ///       - 則此筆訊息的 header 必須與上次相同
   ///       - 除非呼叫過 ResetExpectHeader()
   ///     - 若預期的 header 為空白, 則將 header 保留, 下次檢查使用.
   ///       - 可透過 ResetExpectHeader() 重設.
   ///   - 若提供的訊息長度不足:
   ///     - 則傳回 NeedsMore.
   ///     - 可從 GetExpcetSize() 取得期望的資料長度.
   ///   - 訊息長度 >= 需要的長度: 解析完整內容.
   ///   - 解析成功, 應立即取用 GetField() 處理結果.
   ///     - 一旦 fixmsg 緩衝內容變動, GetField() 取得的結果將會失效!
   /// - until == Until::FullMessage 表示驗證後, 透過 ParseFields() 解析全部欄位.
   /// - until != Until::FullMessage 表示只解析需要的欄位就結束
   ///   - 不檢查 check sum.
   ///   - 不檢查 BeginString 是否正確.
   ///   - 通常在重新載入時, 重建索引之用.
   ///
   /// \return >NeedMore fixmsg用掉的長度.
   Result Parse(StrView& fixmsg, Until until = Until::FullMessage);

   /// 解析整個 FIX Message.
   /// - header, check sum 視為一般欄位, 不檢查是否正確.
   /// - 不可由分隔符號開頭, 尾端最後一個分隔符號可有可無.
   /// - 在 Parse() 若 until != Until::FullMessage 且成功解析了所需的欄位,
   ///   接下來如果決定需要全部解析, 則可呼叫此處.
   /// - 解析前不會清除上次的欄位,
   ///   所以如果不是 Parse() 內的呼叫, 應該要先用 Clear() 清除上次結果.
   ///
   /// \retval =ParseEnd  表示解析完畢.
   /// \retval <NeedsMore 表示訊息有誤.
   /// \retval =ParseEnd  表示解析完畢.
   /// \retval <NeedsMore 表示訊息有誤.
   Result ParseFields(StrView& fixmsg, Until until);

   /// 取得 ParseFields() 解析之後的欄位內容.
   /// \retval nullptr  欄位不存在
   /// \retval !nullptr 在 Clear() 之後的 ParseFields() 欄位至少出現過一次.
   const FixField* GetField(FixTag tag) const {
      const FixField& fld = this->FieldArray_[tag];
      return(fld.ValueCount_ > 0 ? &fld : nullptr);
   }
   /// 當同一個欄位重複出現, 則透過此處找後續出現的 values.
   /// - 若 index >= fld.ValueCount_ 則返回 nullptr;
   /// - 若 index == 0 則返回 fld.Value_;
   StrView GetValue(const FixField& fld, unsigned index) const;

   /// 解析成功後, 取得此筆訊息的 MsgSeqNum.
   FixSeqNum GetMsgSeqNum() const {
      return this->MsgSeqNum_;
   }

   /// 依解析順序儲存, 包含重複欄位.
   using FieldList = std::vector<FixField*>;
   using ListIterator = FieldList::const_iterator;
   ListIterator begin() const {
      return this->FieldList_.begin();
   }
   ListIterator end() const {
      return this->FieldList_.end();
   }
   /// ParseFields() 解析的欄位數量.
   /// 不包含 Header(Tag#8, Tag#9), Tail.CheckSum(Tag#10)
   size_t count() const {
      return this->FieldList_.size();
   }
private:
   using FieldArray = LevelArray<FixTag, FixField>;
   CharVector  ExpectHeader_;
   FieldArray  FieldArray_;
   FieldList   FieldList_;
   FixSeqNum   MsgSeqNum_;
   ExpectSize  ExpectSize_{0};
   uint16_t    MIndexNext_{0};
   using MFields = std::vector<StrView>;
   MFields     MFields_;
};
fon9_ENABLE_ENUM_BITWISE_OP(FixParser::Until);
fon9_ENABLE_ENUM_BITWISE_OP(FixParser::VerifyItem);

} } // namespaces
#endif//__fon9_fix_FixParser_hpp__
