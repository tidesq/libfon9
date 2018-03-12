// \file fon9/buffer/RevPrint_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/TestTools.hpp"
#include "fon9/buffer/RevPrint.hpp"
#include "fon9/Decimal.hpp"

//--------------------------------------------------------------------------//

#include <set>
using strlist = std::set<std::string>;
strlist operator*(const strlist& lhs, const strlist& rhs) {
   strlist res = lhs;
   for (const std::string& rs : rhs) {
      if (!rs.empty()) {
         for (const std::string& ls : lhs)
            res.insert(ls + rs);
      }
   }
   return res;
}
// width: "0" .. "20"
// .precision: ".0" .. ".25"
void AddWidthPrecision(strlist& fmtlist) {
   strlist wlist;
   fon9::NumOutBuf nbuf;
   for (unsigned L = 0; L <= 20; ++L)
      wlist.insert(std::string{fon9::ToStrRev(nbuf.end(), L), nbuf.end()});
   fmtlist = fmtlist * wlist;
   wlist.clear();
   for (unsigned L = 0; L <= 25; ++L) {
      char* pout = fon9::ToStrRev(nbuf.end(), L);
      *--pout = '.';
      wlist.insert(std::string{pout, nbuf.end()});
   }
   fmtlist = fmtlist * wlist;
   std::cout << "fmtlist.size=" << fmtlist.size() << std::endl;
#if 0
   for (const std::string& s : fmtlist)
      std::cout << std::setw(10) << s;
#endif
}

namespace fon9 {
template <class RevBuffer>
void RevPutStr(RevBuffer& rbuf, double value, FmtDef fmt) {
   using Dec = fon9::Decimal<int64_t, 6>;
   if (!IsEnumContains(fmt.Flags_, FmtFlag::HasPrecision)) {
      // %lf 的預設為 ".6"，fon9 預設沒有 FmtDef::Precision_
      // 此處調整成: 與 %lf 有相同的輸出.
      fmt.Flags_ |= FmtFlag::HasPrecision;
      fmt.Precision_ = 6;
   }
   RevPutStr(rbuf, Dec{value}, fmt);
}
} // namespace fon9

std::ostream& operator<<(std::ostream& os, fon9::BaseFmt b) {
   return os << b.Value_;
}

// 測試單一格式: fmt = "%..."; 測試 sprintf(fmt) 與 fon9::RevPutStr(fmt+1) 的結果是否相同.
template <typename ValueT>
void CheckFormat(const char* fmt, ValueT value) {
   std::cout << "[TEST ] fmt=\"" << fmt << "\"" << std::setw(static_cast<int>(10 - strlen(fmt))) << "";

   fon9::RevBufferFixedSize<1024> rbuf;
   rbuf.RewindEOS();
   fon9::RevPutStr(rbuf, value, fon9::FmtDef{fon9::ToStrView(fmt + 1)});
   std::cout << "|result=\"" << rbuf.GetCurrent() << "\"";

   char  sbuf[1024];
   fon9_MSC_WARN_DISABLE(4774)//C4774: 'sprintf' : format string expected in argument 2 is not a string literal
      sprintf(sbuf, fmt, value);
   fon9_MSC_WARN_POP;

   if (strcmp(sbuf, rbuf.GetCurrent()) == 0) {
      std::cout << "\r[OK   ]" << std::endl;
      return;
   }
   std::cout << "|sprintf=\"" << sbuf << "\""
      << "\r[ERROR]" << std::endl;
   abort();
}

template <typename ValueT>
void CheckFormat(const strlist& fmtlist, const char* spec, ValueT value) {
   std::cout << "[TEST ] value=" << value;
   fon9::RevBufferFixedSize<1024> rbuf;
   std::string fmt;
   for (const std::string& f : fmtlist) {
      rbuf.RewindEOS();
      fon9::FmtDef fmtdef{fon9::ToStrView(f)};
      fon9::RevPutStr(rbuf, value, fmtdef);

      fmt.assign(1, '%');
      fmt += f;
      fmt += spec;

      char  sbuf[1024];
      fon9_MSC_WARN_DISABLE(4774)//C4774: 'sprintf' : format string expected in argument 2 is not a string literal
         sprintf(sbuf, fmt.c_str(), value);
      fon9_MSC_WARN_POP;

      if (strcmp(sbuf, rbuf.GetCurrent()) == 0)
         continue;

      std::cout << "|fmt=\"" << fmt << "\""
         << "|result=\"" << rbuf.GetCurrent() << "\""
         << "|sprintf=\"" << sbuf << "\""
         << "\r[ERROR]" << std::endl;
      abort();
   }
   std::cout << "\r[OK   ]" << std::endl;
}

void TestFormatOutput() {
   // CheckFormat("% #.0x", fon9::ToHex{0});

   std::cout << "fon9::RevPutStr(FmtDef)|sprintf()\n" << std::endl;
   // flags: ("-" or "") * ("+" or " " or "") * ("#" or "") * ("0" or "")
   strlist fmtlist = strlist{"-", ""} *strlist{"+", " "} *strlist{"#"} *strlist{"0"};
   AddWidthPrecision(fmtlist);
   CheckFormat(fmtlist, "d", 0);
   CheckFormat(fmtlist, "d", 123);
   CheckFormat(fmtlist, "d", -123);

   CheckFormat(fmtlist, "x", fon9::ToHex{0});
   CheckFormat(fmtlist, "x", fon9::ToHex{123});
   CheckFormat(fmtlist, "x", fon9::ToHex{-123});

   // 為了 [精確度] 及 [四捨五入] 問題, 所以請謹慎選擇測試用的數字.
   // * 例如: 123.456 => "% #.15lf" => " 123.456000000000003"
   //   最後的 3 是 double 無法精確表示 123.456 所造成的誤差.
   // * 小數部分為 (1/2 + 1/4 + 1/8) 之類, 也就是 [合計 2^(-n)] 的數字, 才不會有精確度的誤差問題.
   // * 有一篇四捨五入的說明: http://www.exploringbinary.com/inconsistent-rounding-of-printed-floating-point-numbers/
   //   * VC 採用 [round-half-away-from-zero](https://en.wikipedia.org/wiki/Rounding#Round_half_away_from_zero)
   //   * glibc 採用 [round-half-to-even](https://en.wikipedia.org/wiki/Rounding#Round_half_to_even)
   //   * 為了避免以上的爭議, 所以底下選的數字, 小數的尾數如果是5(例: 0.xxx5), 則尾數5的前一位為奇數, 這樣以上2種進位法都會有相同的結果.
   //   * 否則 0.125 在 VC 為 0.13; glibc 則為 0.12; 測試就會失敗!
   // * fon9 採用的是 [round-half-away-from-zero].
   CheckFormat(fmtlist, "lf", 0.0);
   CheckFormat(fmtlist, "lf", 123.875);
   CheckFormat(fmtlist, "lf", 123.375);
   CheckFormat(fmtlist, "lf", 123.1875);
   CheckFormat(fmtlist, "lf", -123.875);
   CheckFormat(fmtlist, "lf", -123.375);
   CheckFormat(fmtlist, "lf", -123.1875);

   std::cout << std::endl;
   fmtlist = strlist{"-", ""}; // 字串型別輸出, 只有 "-" 旗標, 沒有 "+"、" "、"#"、"0" 這些控制數字輸出的旗標.
   AddWidthPrecision(fmtlist);
   CheckFormat(fmtlist, "s", "Hello");
}

//--------------------------------------------------------------------------//

int main() {
   fon9::AutoPrintTestInfo utinfo{"StrTo"};
   TestFormatOutput();

   utinfo.PrintSplitter();
   fon9::RevBufferFixedSize<1024> rbuf;

#define CHECK_RevPrint(expectResult, expression) \
   std::cout << "[TEST ] " #expression ";"; \
   rbuf.RewindEOS(); \
   expression; \
   std::cout << "|result=\"" << rbuf.GetCurrent() << '"'; \
   if (strcmp(rbuf.GetCurrent(), expectResult) != 0) { \
      std::cout << "|expect=\"" expectResult "\""; \
      std::cout << "\r[ERROR]" << std::endl; \
      abort(); \
   } \
   std::cout << "\r[OK   ]" << std::endl;
   //---------------------
   char strHello[128]{"Hello"};
   const char cstrWorld[]{"World"};
   CHECK_RevPrint("Hello World!", fon9::RevPrint(rbuf, strHello, ' ', cstrWorld, "!"));
   CHECK_RevPrint("1|2|3",        fon9::RevPrint(rbuf, 1, '|', 2, '|', 3));
   
   CHECK_RevPrint("ffff",     fon9::RevPrint(rbuf, fon9::ToHex{static_cast<int16_t>(-1)}));
   CHECK_RevPrint("0xffff",   fon9::RevPrint(rbuf, fon9::ToHex{static_cast<int16_t>(-1)}, fon9::FmtDef{"#"}));
   CHECK_RevPrint("0x001234", fon9::RevPrint(rbuf, fon9::ToHex{0x1234}, fon9::FmtDef{"#08x"}));
   CHECK_RevPrint("00001234", fon9::RevPrint(rbuf, fon9::ToHex{0x1234}, fon9::FmtDef{"08x"}));
   CHECK_RevPrint("  0x5678", fon9::RevPrint(rbuf, fon9::ToPtr{reinterpret_cast<char*>(0x5678)}, fon9::FmtDef{"#8x"}));
   CHECK_RevPrint("    5678", fon9::RevPrint(rbuf, fon9::ToPtr{reinterpret_cast<char*>(0x5678)}, fon9::FmtDef{"8x"}));

   using Dec = fon9::Decimal<int64_t, 6>;
   CHECK_RevPrint("   +9,876,543.210000", fon9::RevPrint(rbuf, Dec{9876543210,3}, fon9::FmtDef{",+20.6"}));
   CHECK_RevPrint("    9,876,543.210000", fon9::RevPrint(rbuf, Dec{9876543210,3}, fon9::FmtDef{", 20.6"}));
   CHECK_RevPrint("-9,876,543.21",        fon9::RevPrint(rbuf, Dec{-9876543210,3}, fon9::FmtDef{","}));
   CHECK_RevPrint("    +9,876,543210000", fon9::RevPrint(rbuf, Dec{9876543210,3}, fon9::FmtDef{",+20_6"}));
   CHECK_RevPrint("     9,876,543210000", fon9::RevPrint(rbuf, Dec{9876543210,3}, fon9::FmtDef{", 20_6"}));

   std::cout << "\n" "UTF8-Trunc:" "\n";
   CHECK_RevPrint("Hello ",      fon9::RevPrint(rbuf, "Hello 您好!", fon9::FmtDef{".8"}));
   CHECK_RevPrint("Hello 您",    fon9::RevPrint(rbuf, "Hello 您好!", fon9::FmtDef{".9"}));
   CHECK_RevPrint("Hello 您",    fon9::RevPrint(rbuf, "Hello 您好!", fon9::FmtDef{".10"}));
   CHECK_RevPrint("Hello 您",    fon9::RevPrint(rbuf, "Hello 您好!", fon9::FmtDef{".11"}));
   CHECK_RevPrint("Hello 您好",  fon9::RevPrint(rbuf, "Hello 您好!", fon9::FmtDef{".12"}));
   CHECK_RevPrint("Hello 您好!", fon9::RevPrint(rbuf, "Hello 您好!", fon9::FmtDef{".13"}));

   CHECK_RevPrint("Hello 您           ", fon9::RevPrint(rbuf, "Hello 您好!", fon9::FmtDef{"-20.10"}));
   CHECK_RevPrint("           Hello 您", fon9::RevPrint(rbuf, "Hello 您好!", fon9::FmtDef{"20.10"}));
}
