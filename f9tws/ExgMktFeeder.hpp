// \file f9tws/ExgMktFeeder.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMktFeedert_hpp__
#define __f9tws_ExgMktFeedert_hpp__
#include "f9tws/ExgMktFmt.hpp"
#include "fon9/buffer/DcQueue.hpp"

namespace f9tws {

fon9_WARN_DISABLE_PADDING;
/// 解析ExgMkt(交易所行情格式)的框架: Esc ... CheckSum + TerminalCode(0x0d,0x0a).
/// - 每個資訊來源對應一個 ExgMktFeeder.
/// - 將收到的資訊丟到 ExgMktFeeder::FeedBuffer()
/// - 當收到一個封包時透過 ExgMktFeeder::ExgMktOnReceived() 通知衍生者處理.
class f9tws_API ExgMktFeeder {
public:
   virtual ~ExgMktFeeder();

   /// 收到的訊息透過這裡處理.
   /// 返回時機: rxbuf 用完, 或 rxbuf 剩餘資料不足一個封包.
   void FeedBuffer(fon9::DcQueue& rxbuf);

   void ClearStatus() {
      this->ReceivedCount_ = 0;
      this->ChkSumErrCount_ = 0;
      this->DroppedBytes_ = 0;
   }

   uint64_t GetReceivedCount() const { return this->ReceivedCount_; }
   uint64_t GetChkSumErrCount() const { return this->ChkSumErrCount_; }
   uint64_t GetDroppedBytes() const { return this->DroppedBytes_; }

protected:
   uint64_t ReceivedCount_{0};
   uint64_t ChkSumErrCount_{0};
   uint64_t DroppedBytes_{0};
   /// 若使用 IsDgram_ 則 FeedBuffer(rxbuf) 每次都只會收到一個完整封包,
   /// 若返回前 rxbuf 有剩餘, 表示資料有誤, 直接拋棄剩餘資料.
   bool     IsDgram_{false};

   /// 當收到封包時透過這裡通知衍生者.
   virtual void ExgMktOnReceived(const ExgMktHeader& pk, unsigned pksz) = 0;
};
fon9_WARN_POP;

} // namespaces
#endif//__f9tws_ExgMktFeedert_hpp__
