// \file fon9/StrView_UT.cpp
// \author fonwinz@gmail.com
#include "fon9/StrView.hpp"
#include "fon9/TestTools.hpp"
#include <string.h> // strlen()

void TestStrViewSize(const char* cstrTestItem, size_t charCount, const fon9::StrView& StrView) {
   printf("[%-5s] %-25s [size=%2d]: \"%s\"\n"
          , StrView.size() == charCount ? "OK" : "ERR"
          , cstrTestItem
          , static_cast<int>(StrView.size())
          , StrView.ToString().c_str());
   if (StrView.size() != charCount)
      abort();
}
void TestStrViewCompare(const fon9::StrView s1, const fon9::StrView s2, int expectResult) {
   int cmp = s1.Compare(s2);
   printf("[%-5s] %7s.Compare(%-7s) = %2d\n"
          , cmp == expectResult ? "OK" : "ERR"
          , s1.begin() == nullptr ? "nullptr" : ("\"" + s1.ToString() + "\"").c_str()
          , s2.begin() == nullptr ? "nullptr" : ("\"" + s2.ToString() + "\"").c_str()
          , cmp);
   if (s1.Compare(s2) != expectResult) {
      printf("ExpectResult = %d\n", expectResult);
      abort();
   }
   #define TEST_COMPARE_OP(op)            \
      if ((s1 op s2) != (cmp op 0)) {     \
         puts("[s1 " #op " s2] fail!");   \
         abort();                         \
      }
   //----------------------------------
   TEST_COMPARE_OP( == );
   TEST_COMPARE_OP( != );
   TEST_COMPARE_OP( <  );
   TEST_COMPARE_OP( >  );
   TEST_COMPARE_OP( <= );
   TEST_COMPARE_OP( >= );
}

int main() {
   std::cout <<
      "#####################################################\n"
      "fon9 StrView test\n"
      "====================================================="
      << std::endl;

   // 測試建構: 長度是否正確.
   #define  cstr_Normal_C_Style_String   "Normal C-Style string."
   TestStrViewSize("\"...\"", sizeof(cstr_Normal_C_Style_String) - 1, cstr_Normal_C_Style_String);

   const char  cstrAry3[] = {'a', 'b', 'c'};
   TestStrViewSize("{'a', 'b', 'c'}",      sizeof(cstrAry3), cstrAry3);
   TestStrViewSize("StrView_all()",        sizeof(cstrAry3), fon9::StrView_all(cstrAry3));
   TestStrViewSize("StrView_eos_or_all()", sizeof(cstrAry3), fon9::StrView_eos_or_all(cstrAry3));

   const char  cstrAry3EOS[] = {'a', 'b', '\0'};
   TestStrViewSize("{'a', 'b', '\\0'}",    sizeof(cstrAry3EOS) - 1, cstrAry3EOS);
   TestStrViewSize("StrView_all()",        sizeof(cstrAry3EOS),     fon9::StrView_all(cstrAry3EOS)); // StrView_all() 長度包含EOS.
   TestStrViewSize("StrView_eos_or_all()", sizeof(cstrAry3EOS) - 1, fon9::StrView_eos_or_all(cstrAry3EOS));

   const char  cstrCharAry[] = "Normal C-Style char array.";
   const int   szCharAry = sizeof(cstrCharAry);
   TestStrViewSize("const char[]", szCharAry - 1, cstrCharAry);

   const char* cstr = "Normal <const char*>";
   TestStrViewSize("StrView_cstr()",        strlen(cstr), fon9::StrView_cstr(cstr));
   TestStrViewSize("StrView_cstr(nullptr)", 0,            fon9::StrView_cstr(nullptr));

   // 測試UTF-8.
   const char  cstrUTF8[] = "正體中文(許功蓋), 简体中文";
   TestStrViewSize("utf8 '%*s' 可能無法對齊", strlen(cstrUTF8), cstrUTF8);

   std::cout << "-----------------------------------------------------" << std::endl;
   // 測試字串比較.
   const char     cstrAryABC[] = {'a', 'b', 'c'};
   fon9::StrView  sABC3{cstrAryABC};
   fon9::StrView  sABC1{"abc"};
   TestStrViewCompare(sABC1, sABC3, 0);

   fon9::StrView  sABCDE{"abcde"};
   TestStrViewCompare(sABC1, sABCDE, -1);
   TestStrViewCompare(sABCDE, sABC1, 1);

   fon9::StrView sEmpty{};
   fon9::StrView sNull{nullptr, static_cast<size_t>(0)};

   TestStrViewCompare(sNull,  sABC1, -1);
   TestStrViewCompare(sEmpty, sABC1, -1);
   TestStrViewCompare(sABC1,  sNull,  1);
   TestStrViewCompare(sABC1,  sEmpty, 1);
   TestStrViewCompare(sEmpty, sNull,  0);
   TestStrViewCompare(sNull,  sEmpty, 0);

   std::cout <<
      "=====================================================\n"
      "fon9 StrView test # END #\n"
      "#####################################################\n"
      << std::endl;
}
