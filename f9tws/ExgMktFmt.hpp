// \file f9tws/ExgMktFmt.hpp
//
// TSE/OTC 行情格式解析, 參考文件:
// - 臺灣證券交易所 / 資訊傳輸作業手冊 (IP 行情網路)
// - 中華民國證券櫃檯買賣中心 / 資訊傳輸作業手冊 （上櫃股票 IP 行情網路）
//
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgMktFmt_hpp__
#define __f9tws_ExgMktFmt_hpp__
#include "f9tws/ExgTypes.hpp"
#include "fon9/PackBcd.hpp"
#include "fon9/fmkt/FmktTypes.hpp"

namespace f9tws {
fon9_PACK(1);

/// 每個封包的基本框架:
/// - Esc: 1 byte = 27;
/// - fon9::PackBcd<4> Length_; 包含: Esc...結束碼.
/// - 檢查碼 1 byte; (Esc...檢查碼)之間(不含Esc、檢查碼)的 XOR 值.
/// - 結束碼 2 bytes; [0]=0x0d; [1]=0x0a;
struct ExgMktHeader {
   fon9::byte        Esc_;
   fon9::PackBcd<4>  Length_;
   /// 0x01=TSE, 0x02=OTC
   fon9::PackBcd<2>  Market_;
   fon9::PackBcd<2>  FmtNo_;
   fon9::PackBcd<2>  VerNo_;
   fon9::PackBcd<8>  SeqNo_;

   unsigned GetLength() const { return fon9::PackBcdTo<unsigned>(this->Length_); }
   uint8_t  GetFmtNo() const { return fon9::PackBcdTo<uint8_t>(this->FmtNo_); }
   uint8_t  GetVerNo() const { return fon9::PackBcdTo<uint8_t>(this->VerNo_); }
   uint32_t GetSeqNo() const { return fon9::PackBcdTo<uint32_t>(this->SeqNo_); }
};
static_assert(sizeof(ExgMktHeader) == 10, "ExgMktHeader 沒有 pack?");
enum : size_t {
   kExgMktMaxPacketSize = fon9::DecDivisor<size_t, sizeof(ExgMktHeader::Length_) * 2>::Divisor,
   kExgMktMaxFmtNoSize = fon9::DecDivisor<size_t, sizeof(ExgMktHeader::FmtNo_) * 2>::Divisor,
};

struct ExgMktPriQty {
   /// 小數2位.
   fon9::PackBcd<6>  PriV2_;
   fon9::PackBcd<8>  Qty_;

   void AssignTo(fon9::fmkt::PriQty& dst) const {
      dst.Pri_.Assign<2>(fon9::PackBcdTo<fon9::fmkt::Qty>(this->PriV2_));
      dst.Qty_ = fon9::PackBcdTo<fon9::fmkt::Qty>(this->Qty_);
   }
};
static_assert(sizeof(ExgMktPriQty) == 7, "ExgMktPriQty 沒有 pack?");

fon9_MSC_WARN_DISABLE(4201); // nonstandard extension used: nameless struct/union
/// HHMMSSuuuuuu 後6位為 us.
union TimeHHMMSSu6 {
   fon9::PackBcd<12>    HHMMSSu6_;
   fon9::PackBcd<6>     HHMMSS_;
   struct {
      fon9::PackBcd<2>  HH_;
      fon9::PackBcd<2>  MM_;
      fon9::PackBcd<2>  SS_;
      fon9::PackBcd<6>  U6_;
   };
};
fon9_MSC_WARN_POP;

fon9_PACK_POP;
} // namespaces
#endif//__f9tws_ExgMktFmt_hpp__
