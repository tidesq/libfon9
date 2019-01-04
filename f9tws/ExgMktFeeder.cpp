// \file f9tws/ExgMktFeeder.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMktFeeder.hpp"

namespace f9tws {

ExgMktFeeder::~ExgMktFeeder() {
}

//--------------------------------------------------------------------------//
#define kEscCode   27

void ExgMktFeeder::FeedBuffer(fon9::DcQueue& rxbuf) {
   char  pkbuf[kExgMktMaxPacketSize];
   while (const void* pkptr = rxbuf.Peek(pkbuf, sizeof(ExgMktHeader))) {
      if (fon9_LIKELY(reinterpret_cast<const ExgMktHeader*>(pkptr)->Esc_ == kEscCode)) {
         const auto pksz = reinterpret_cast<const ExgMktHeader*>(pkptr)->GetLength();
         if (fon9_LIKELY(pksz < kExgMktMaxPacketSize)) {
            pkptr = rxbuf.Peek(pkbuf, pksz);
            if (fon9_UNLIKELY(pkptr == nullptr)) // rxbuf 資料不足 Length_
               break;
            // rxbuf 的資料長度足夠一個封包.
            const char* pk = reinterpret_cast<const char*>(pkptr);
            if (fon9_LIKELY(pk[pksz - 2] == 0x0d && pk[pksz - 1] == 0x0a)) {
               // 尾碼正確, 計算 CheckSum.
               const char* pkend = pk + pksz - 3; // -3 = 排除 0x0d, 0x0a. 但包含 CheckSum.
               char  cks = *++pk; // ++pk = 排除 Esc.
               while (pk != pkend) {
                  fon9_GCC_WARN_DISABLE("-Wconversion");
                  cks ^= *++pk;
                  fon9_GCC_WARN_POP;
               }
               if (fon9_LIKELY(cks == 0)) {
                  // CheckSum 正確, 解析封包內容.
                  ++this->ReceivedCount_;
                  this->ExgMktOnReceived(*reinterpret_cast<const ExgMktHeader*>(pkptr), pksz);
               }
               else {
                  ++this->ChkSumErrCount_;
               }
               rxbuf.PopConsumed(pksz);
               continue;
            } // else: 尾碼不是 0x0d, 0x0a.
         } // else: 長度不合理.
         // 繼續執行: 搜尋下一個 EscCode.
         rxbuf.PopConsumed(1);
         ++this->DroppedBytes_;
      } // else: first code is not EscCode.
      this->DroppedBytes_ += rxbuf.PopUnknownChar(kEscCode);
   }
   if (this->IsDgram_ && rxbuf.Peek1()) {
      if (size_t remain = rxbuf.CalcSize()) {
         rxbuf.PopConsumed(remain);
         this->DroppedBytes_ += remain;
      }
   }
}

} // namespaces
