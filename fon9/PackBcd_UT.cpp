// \file fon9/PackBcd_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/PackBcd.hpp"
#include "fon9/TestTools.hpp"
#include "fon9/DecBase.hpp"

template <unsigned kPackedWidth>
void TestPackBcd() {
   const unsigned kTimes = fon9::DecDivisor<unsigned, kPackedWidth>::Divisor;
   const unsigned kSize = fon9::PackBcdWidthToSize(kPackedWidth);
   unsigned char  buf[kSize * 3];
   unsigned char  xff[kSize];
   memset(buf, 0xff, sizeof(buf));
   memset(xff, 0xff, sizeof(xff));
   std::cout << "[TEST ] PackedWidth=" << kPackedWidth << '|' << std::flush;
   for (unsigned L = 0; L < kTimes; ++L) {
      fon9::ToPackBcd<kPackedWidth>(buf + kSize, L);
      if (memcmp(buf, xff, sizeof(xff)) != 0 || memcmp(buf + kSize * 2, xff, sizeof(xff)) != 0) {
         std::cout << "Buffer override." << "\r[ERROR]" << std::endl;
         abort();
      }
      unsigned char* pbuf = buf + kSize;
      unsigned       v = 0;
      for (unsigned i = 0; i < kPackedWidth; i += 2) {
         v = (v * 100) + (((*pbuf >> 4) * 10) + (*pbuf & 0x0f));
         ++pbuf;
      }
      if (v != L) {
         std::cout << "L=" << L << "|err=ToPackBcd()=" << v << "\r[ERROR]" << std::endl;
         abort();
      }
      v = fon9::PackBcdTo<kPackedWidth, unsigned>(buf + kSize);
      if (v != L) {
         std::cout << "L=" << L << "|err=PackBcdTo()=" << v << "\r[ERROR]" << std::endl;
         abort();
      }
      fon9::PackBcd<kPackedWidth> pbcd;
      fon9::ToPackBcd(pbcd, L);
      v = fon9::PackBcdTo<unsigned>(pbcd);
      if (v != L) {
         std::cout << "L=" << L << "|err=PackBcd<>=" << v << "\r[ERROR]" << std::endl;
         abort();
      }
   }
   std::cout << "\r[OK   ]" << std::endl;
}

int main() {
   fon9::AutoPrintTestInfo utinfo{"PackBcd"};
   TestPackBcd<1>();
   TestPackBcd<2>();
   TestPackBcd<3>();
   TestPackBcd<4>();
   TestPackBcd<5>();
   TestPackBcd<6>();
   TestPackBcd<7>();
   // 底下要花比較多的測試時間, 而且測試 PackedWidth=1..7 應該已經足夠了, 所以就拿掉囉.
   // TestPackBcd<8>();
   // TestPackBcd<9>();
   // TestPackBcd<10>();
}
