/// \file fon9/fix/FixBuilder.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_FixBuilder_hpp__
#define __fon9_fix_FixBuilder_hpp__
#include "fon9/fix/FixBase.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 { namespace fix {

/// \ingroup fix
/// FIX Message 產生器.
/// - 緩衝區在 Final() 之後就會被取出, 所以此物件沒有額外的負擔,
///   需要時建立, 不用時拋棄, 可以不用考慮重複使用.
/// - 直接將要加入的 FIX 欄位填入 buffer
/// - 欄位加入順序=由後往前:
///   - 早加入的靠近訊息尾端.
///   - 晚加入的靠近訊息前端.
/// - 直接用 fon9::RevPrint() 加入訊息片段, 格式為: "|tag=value"
///   - 這裡不檢查格式是否正確!
///   - 例如:
/// \code
///   FixBuilder  fbuf;
///   ...先填入交易所需欄位...
///   RevPrint(fbuf.GetBuffer(),
///        f9fix_SPLFLDMSGTYPE(MsgType)   "A"   // 直接使用 "|35=" "A" 常數字串合併, 所以中間不用加「,」分隔參數
///        f9fix_SPLTAGEQ(MsgSeqNum),   789,  // "34=", 789(數字)
///        f9fix_SPLTAGEQ(TargetCompID) "Client"
///        f9fix_SPLTAGEQ(SenderCompID) "Server");
///   auto fixmsg = fbuf.Final(f9fix_BEGIN_HEADER_V44);
/// \endcode
class fon9_API FixBuilder {
   fon9_NON_COPYABLE(FixBuilder);
   RevBufferList  Buffer_{512};
   char*          CheckSumPos_;
   void Start() {
      this->CheckSumPos_ = this->Buffer_.AllocPrefix(kFixTailWidth);
      this->Buffer_.SetPrefixUsed(this->CheckSumPos_ -= kFixTailWidth);
   }
public:
   FixBuilder() {
      this->Start();
   }
   FixBuilder(bool isManualStart) {
      if (isManualStart)
         this->CheckSumPos_ = nullptr;
      else
         this->Start();
   }
   FixBuilder(FixBuilder&& rhs) : Buffer_{std::move(rhs.Buffer_)}, CheckSumPos_{rhs.CheckSumPos_} {
      rhs.CheckSumPos_ = nullptr;
   }
   FixBuilder& operator=(FixBuilder&&) = delete;

   RevBuffer& GetBuffer() {
      return this->Buffer_;
   }
   /// 開始建立 FIX Message.
   /// - 呼叫時機: Finish()之後, 再次使用之前.
   /// - 尾端保留 CheckSum 的空間.
   void Restart() {
      assert(this->CheckSumPos_ == nullptr);
      return this->Start();
   }
   /// 訊息建立完畢, 最終填入 BeginString, BodyLength, 計算 CheckSum.
   /// \param beginHeader "8=FIX.4.x|9=" 這裡不檢查 header 是否正確!
   /// \return 傳回建立好的 FIX Message: 包含 beginHeader + body + checksum.
   BufferList Final(const StrView& beginHeader);

   /// 填入 CheckSum: "|10=xxx|"  xxx=CheckSum(cks).
   static void PutCheckSumField(char psum[kFixTailWidth], byte cks) {
      static_assert(kFixTailWidth == 8 && f9fix_kTAG_CheckSum == 10, "FIX CheckSum.Tag# || TailSize error.");
      psum[0] = f9fix_kCHAR_SPL;
      psum[1] = '1';
      psum[2] = '0';
      psum[3] = '=';
      Pic9ToStrRev<3>(psum + kFixTailWidth - 1, cks);
      psum[7] = f9fix_kCHAR_SPL;
   }
};

} } // namespace
#endif//__fon9_fix_FixBuilder_hpp__
