// \file fon9/CharVector_UT.cpp
// \author fonwinz@gmail.com
#include "fon9/CharVector.hpp"
#include "fon9/TestTools.hpp"

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

   fon9::AutoPrintTestInfo utinfo{"CharVector"};

   const char cstrTest[] = "Test";
   fon9::CharVector skey = fon9::CharVector::MakeRef(cstrTest, sizeof(cstrTest)-1);
   fon9_CheckTestResult("MakeRef()",
                        reinterpret_cast<const char*>(skey.begin()) == cstrTest
                        && skey.size() == sizeof(cstrTest) - 1);

   skey = fon9::CharVector::MakeRef(fon9::StrView{cstrTest});
   fon9_CheckTestResult("opreator=(MakeRef())",
                        reinterpret_cast<const char*>(skey.begin()) != cstrTest
                        && skey.size() == sizeof(cstrTest) - 1);

   std::string stds{cstrTest};
   const char cstrFiller[] = ".abcdefghijklmnopqrstuvwxyz-ABCDEFGHIJKLMNOPQRSTUVWXYZ";
   for (char ch : cstrFiller) {
      skey.append(&ch, 1);
      skey.push_back(ch);

      stds.append(&ch, 1);
      stds.push_back(ch);

      if (fon9::ToStrView(skey) != fon9::ToStrView(stds))
         fon9_CheckTestResult("append()", false);
   }

   // 測試 CharVector建構(複製cstrTest).
   fon9_CheckTestResult("ctor(copy str)",
                        fon9::CharVector{fon9::StrView{cstrTest}}.begin() != cstrTest);

   // 測試 CharVector建構(複製cstrTest), 然後move()到 skey:
   skey = fon9::CharVector{fon9::StrView{cstrTest}};
   fon9_CheckTestResult("opreator=(ctor(copy str))",
                        reinterpret_cast<const char*>(skey.begin()) != cstrTest
                        && skey.size() == sizeof(cstrTest) - 1);

   // 測試 reserve();
   stds = cstrTest;
   for (char ch : cstrFiller) {
      skey.reserve(skey.size() + 1);
      skey.push_back(ch);
      stds.push_back(ch);
      if (fon9::ToStrView(skey) != fon9::ToStrView(stds))
         fon9_CheckTestResult("reserve(+1)", false);
   }

   // 測試 erase();
   skey.assign(cstrFiller);
   skey.erase(0, 0);
   fon9_CheckTestResult("erase(0,0)", fon9::ToStrView(skey) == fon9::ToStrView(cstrFiller));

   skey.erase(0, sizeof(cstrFiller));
   fon9_CheckTestResult("erase(0,fullsize)", skey.empty());

   skey.assign(cstrFiller);
   skey.erase(10, sizeof(cstrFiller));
   fon9_CheckTestResult("erase(10,fullsize)", fon9::ToStrView(skey) == fon9::StrView(cstrFiller,10));

   skey.assign(cstrFiller);
   skey.erase(0, 10);
   fon9_CheckTestResult("erase(0,10)", fon9::ToStrView(skey) == fon9::StrView(cstrFiller+10, sizeof(cstrFiller)-11));

   stds.assign(cstrFiller);
   skey.assign(cstrFiller);
   stds.erase(10, 5);
   skey.erase(10, 5);
   fon9_CheckTestResult("erase(10,5)", fon9::ToStrView(skey) == fon9::ToStrView(stds));

   // 測試填入空白.
   const size_t zero = 0;
   skey.Free();
   skey.assign("xxx", 3);
   skey.reserve(zero);
   fon9_CheckTestResult("reserve(0)", fon9::ToStrView(skey) == "xxx");

   skey.append(nullptr, zero);
   skey.append("yyy", 3);
   fon9_CheckTestResult("append(nullptr,0)", fon9::ToStrView(skey) == "xxxyyy");

   skey.assign(nullptr, zero);
   skey.assign("zzz", 3);
   fon9_CheckTestResult("assign(nullptr,0)", fon9::ToStrView(skey) == "zzz");
}
