/// \file fon9/Endian_UT.cpp
/// \author fonwinz@gmail.com
#include "fon9/Endian.hpp"
#include "fon9/TestTools.hpp"

int main(int argc, char** args) {
   (void)argc; (void)args;
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   //_CrtSetBreakAlloc(176);
#endif
   fon9::AutoPrintTestInfo utinfo{"Endian"};

   static const uint64_t val64 = 0x1234567890abcdef;
   fon9::byte buf[sizeof(uint64_t) * 2];
   fon9::PutBigEndian(buf + 1, val64);
   fon9_CheckTestResult("BigEndian", fon9::GetBigEndian<uint64_t>(buf + 1) == val64);
   fon9_CheckTestResult("BigEndian",
                        buf[1] == 0x12 && buf[2] == 0x34 && buf[3] == 0x56 && buf[4] == 0x78 &&
                        buf[5] == 0x90 && buf[6] == 0xab && buf[7] == 0xcd && buf[8] == 0xef);

   static const uint64_t   valpack64 = 0x12345;
   static const fon9::byte countPacked = 3;
   fon9::byte c = fon9::PackBigEndianRev(buf + sizeof(buf), valpack64);
   fon9_CheckTestResult("PackBigEndianRev",
                        c == countPacked && fon9::GetPackedBigEndian<uint64_t>(buf + sizeof(buf) - c, c) == valpack64);

   fon9::PutLittleEndian(buf + 1, val64);
   fon9_CheckTestResult("LittleEndian", fon9::GetLittleEndian<uint64_t>(buf + 1) == val64);
   fon9_CheckTestResult("LittleEndian",
                        buf[8] == 0x12 && buf[7] == 0x34 && buf[6] == 0x56 && buf[5] == 0x78 &&
                        buf[4] == 0x90 && buf[3] == 0xab && buf[2] == 0xcd && buf[1] == 0xef);
   c = fon9::PackLittleEndian(buf, valpack64);
   fon9_CheckTestResult("PackLittleEndian",
                        c == countPacked && fon9::GetPackedLittleEndian<uint64_t>(buf, c) == valpack64);
}
