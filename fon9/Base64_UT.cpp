// \file fon9/Base64_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/Base64.hpp"
#include "fon9/TestTools.hpp"

//--------------------------------------------------------------------------//

void TestB64(const std::string orig, const std::string b64) {
   char dst[1024];
   auto res = fon9::Base64Encode(dst, sizeof(dst), orig.c_str(), orig.size());
   fon9_CheckTestResult(("Base64Encode:" + orig).c_str(), b64 == std::string(dst, res.GetResult()));
   res = fon9::Base64Decode(dst, sizeof(dst), b64.c_str(), b64.size());
   fon9_CheckTestResult(("Base64Decode:" + b64).c_str(), orig == std::string(dst, res.GetResult()));
}

void TestAllB64() {
   TestB64("", "");
   TestB64("f", "Zg==");
   TestB64("fo", "Zm8=");
   TestB64("foo", "Zm9v");
   TestB64("foob", "Zm9vYg==");
   TestB64("fooba", "Zm9vYmE=");
   TestB64("foobar", "Zm9vYmFy");
}

//--------------------------------------------------------------------------//

int main(int argc, char** args) {
   (void)argc; (void)args;
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   //_CrtSetBreakAlloc(176);
#endif
   fon9::AutoPrintTestInfo utinfo{"Base64"};

   TestAllB64();
}
