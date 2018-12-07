/// \file fon9/fix/FixFeeder.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_FixFeeder_hpp__
#define __fon9_fix_FixFeeder_hpp__
#include "fon9/fix/FixParser.hpp"
#include "fon9/buffer/DcQueue.hpp"

namespace fon9 { namespace fix {

/// \ingroup fix
// 從 Device 收到的資料, 因為不保證每次可收足完整訊息,
/// 所以透過餵食的方式提供給 FixFeeder, 透過這裡擷取出一筆一筆的 FIX Message.
class fon9_API FixFeeder {
   fon9_NON_COPY_NON_MOVE(FixFeeder);
   std::string RxBuf_;
public:
   FixParser   FixParser_;

   FixFeeder() = default;
   virtual ~FixFeeder();

   /// Device 收到的訊息透過這裡處理.
   /// 返回時機: rxbuf 用完, 或解析有錯.
   /// \retval <FixParser::NeedsMore 表示訊息有問題, 應該中斷連線.
   FixParser::Result FeedBuffer(DcQueue& rxbuf);

   void ClearFeedBuffer() {
      this->FixParser_.Clear();
      this->RxBuf_.clear();
   }

   /// 解析後的訊息從 this->FixParser_ 取得.
   virtual void OnFixMessageParsed(StrView fixmsg) = 0;

protected:
   /// 預設直接傳回 res.
   /// \retval <FixParser::NeedsMore  表示訊息有問題, 應該中斷連線.
   /// \retval >=FixParser::NeedsMore 已移除無效資料, 應該繼續解析.
   virtual FixParser::Result OnFixMessageError(FixParser::Result res, StrView& fixmsgStream, const char* perr);

   FixParser::Result OnFixMessageReceived(StrView fixmsgStream);
};

} } // namespaces
#endif//__fon9_fix_FixFeeder_hpp__
