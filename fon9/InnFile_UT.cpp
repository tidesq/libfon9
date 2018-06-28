// \file fon9/InnFile_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/TestTools.hpp"
#include "fon9/InnFile.hpp"
#include "fon9/buffer/RevBuffer.hpp"
#include "fon9/buffer/FwdBufferList.hpp"
#include "fon9/Endian.hpp"
#include "fon9/RevPrint.hpp"

//--------------------------------------------------------------------------//

static const char                kInnFileName[] = "./test.inn";
constexpr fon9::InnFile::SizeT   kBlockSize = 32;

//--------------------------------------------------------------------------//

void TestInnOpen(fon9::InnFile& inn, fon9::InnFile::OpenArgs& args, fon9::InnFile::OpenResult resExpect) {
   inn.Close();
   auto res = inn.Open(args);
   if (res == resExpect)
      return;
   fon9::RevBufferFixedSize<1024> rmsg;
   rmsg.RewindEOS();
   fon9::RevPrint(rmsg, "|res=", res, "|expect=", resExpect);
   std::cout << "[ERROR] InnFile.Open|fname=" << args.FileName_ << rmsg.GetCurrent() << std::endl;
   abort();
}
size_t TestMakeNewRoom(fon9::InnFile& inn, const fon9::InnFile::OpenArgs& args, fon9::InnFile::SizeT exsz) {
   size_t blkcount = (exsz + inn.kRoomHeaderSize + args.BlockSize_ - 1) / args.BlockSize_;
   auto rkey = inn.MakeNewRoom(0, exsz);
   if (rkey.GetRoomSize() != blkcount * args.BlockSize_ - inn.kRoomHeaderSize) {
      std::cout << "[ERROR] InnFile.Open|fname=" << args.FileName_
         << "|roomSize=" << rkey.GetRoomSize()
         << "|expect=" << blkcount * args.BlockSize_ - inn.kRoomHeaderSize
         << std::endl;
      abort();
   }
   if (rkey.GetRoomSize() < exsz) {
      std::cout << "[ERROR] InnFile.Open|fname=" << args.FileName_
         << "|roomSize=" << rkey.GetRoomSize()
         << "|expect=" << exsz
         << std::endl;
      abort();
   }
   return blkcount;
}
void CheckRoomKey(const fon9::InnFile::RoomKey& rkey, fon9::InnFile::SizeT exsz) {
   if (rkey.GetRoomType() == 0 && rkey.GetDataSize() == 0 && rkey.GetRoomSize() >= exsz)
      return;
   std::cout << "[ERROR] RoomKey|roomType=" << static_cast<unsigned>(rkey.GetRoomType())
      << "|dataSize=" << rkey.GetDataSize()
      << "|roomSize=" << rkey.GetRoomSize()
      << "|expectRoomSize=" << exsz
      << std::endl;
   abort();
}
void TestErrorMakeRoomKey(fon9::InnFile& inn, fon9::InnFile::RoomPosT pos) {
   try {
      auto rkey = inn.MakeRoomKey(pos, nullptr, 0);
      std::cout << "[ERROR] MakeRoomKey|pos=" << pos
         << "|roomType=" << static_cast<unsigned>(rkey.GetRoomType())
         << "|dataSize=" << rkey.GetDataSize()
         << "|roomSize=" << rkey.GetRoomSize()
         << std::endl;
      abort();
   }
   catch (std::exception& ex) {
      std::cout << "[OK   ] MakeRoomKey|pos=" << pos << "|ex=" << ex.what() << std::endl;
   }
}
void TestInnOpen() {
   remove(kInnFileName);
   fon9::InnFile::OpenArgs args{kInnFileName, kBlockSize};
   fon9::InnFile           inn;
   TestInnOpen(inn, args, fon9::InnFile::OpenResult{0});
   // 測試: 當 inn 為空, 建立的 roomKey 應為 !roomKey.
   if (inn.MakeRoomKey(0, nullptr, 0)) {
      std::cout << "[ERROR] InnFile.Open|fname=" << args.FileName_
         << "|err=MakeRoomKey() from empty inn error."
         << std::endl;
      abort();
   }

   args.BlockSize_ = 64;
   TestInnOpen(inn, args, fon9::InnFile::OpenResult{0});
   // 測試: 重新開啟 inn 則 BlockSize 應為 inn 的 Header 的內容.
   if (args.BlockSize_ != kBlockSize) {
      std::cout << "[ERROR] InnFile.Open|fname=" << args.FileName_
         << "|blockSize=" << args.BlockSize_
         << "|expect=" << kBlockSize
         << std::endl;
      abort();
   }

   // 測試: 分配一個 room, 則重新開啟 inn 時, 則 inn 有該 room 的空間.
   size_t blkcount = TestMakeNewRoom(inn, args, 100);
   TestInnOpen(inn, args, fon9::InnFile::OpenResult{blkcount});
   blkcount += TestMakeNewRoom(inn, args, 64 - inn.kRoomHeaderSize);

   // 測試: 建立了 2 個 rooms 的 inn 並檢查 inn 空間是否正確:
   TestInnOpen(inn, args, fon9::InnFile::OpenResult{blkcount});

   // 測試: 依序取出 room key
   auto rkey = inn.MakeRoomKey(0, nullptr, 0);
   CheckRoomKey(rkey, 100);
   rkey = inn.MakeNextRoomKey(rkey, nullptr, 0);
   CheckRoomKey(rkey, 64 - inn.kRoomHeaderSize);
   rkey = inn.MakeNextRoomKey(rkey, nullptr, 0);
   if (rkey) {
      std::cout << "[ERROR] Last.RoomKey|roomType=" << static_cast<unsigned>(rkey.GetRoomType())
         << "|dataSize=" << rkey.GetDataSize()
         << "|roomSize=" << rkey.GetRoomSize()
         << std::endl;
      abort();
   }

   // 測試: 用不正確的 room pos 建立 roomkey
   TestErrorMakeRoomKey(inn, 1);
   TestErrorMakeRoomKey(inn, fon9::InnFile::kInnHeaderSize + 1);
   TestErrorMakeRoomKey(inn, fon9::InnFile::kInnHeaderSize + args.BlockSize_);

   inn.Close();
   remove(args.FileName_.c_str());
   std::cout << "[OK   ] InnFile.Open|fname=" << args.FileName_
      << "|blockSize=" << args.BlockSize_
      << "|res=" << blkcount << std::endl;
}

//--------------------------------------------------------------------------//

char membuf[1024];

// 檢查全部的 room:
// room# = 0 .. sizeof(membuf)-1;
// room.DataSize = room#;
// room.內容 = 填滿 static_cast<byte>(room#);
// room.Type = static_cast<byte>(room#);
void CheckRooms(fon9::InnFile& inn, bool useBufferList) {
   std::cout << "CheckRooms...";
   auto rkey = inn.MakeRoomKey(0, nullptr, 0);
   for (size_t L = 0; L < sizeof(membuf); ++L) {
      if (rkey.GetDataSize() != L) {
         std::cout << "\r" "[ERROR] InnFile.Read|err=Invalid data size|"
            "room#=" << L << "|dataSize=" << rkey.GetDataSize() << std::endl;
         abort();
      }
      if (rkey.GetRoomType() != static_cast<fon9::InnRoomType>(L)) {
         std::cout << "\r" "[ERROR] InnFile.Read|err=Invalid room type|"
            "room#=" << L << "|roomType=" << static_cast<unsigned>(rkey.GetRoomType()) << std::endl;
         abort();
      }
      fon9::InnFile::SizeT res;
      if (!useBufferList)
         res = inn.ReadAll(rkey, membuf, static_cast<fon9::InnFile::SizeT>(L));
      else {
         fon9::BufferList buf;
         buf.push_back(fon9::FwdBufferNode::Alloc(1));
         res = inn.ReadAll(rkey, buf);
         memcpy(membuf, fon9::BufferTo<std::string>(buf).c_str(), res);
      }
      if (res != L) {
         std::cout << "\r" "[ERROR] InnFile.Read|res=" << res << "|request=" << L << std::endl;
         abort();
      }
      auto pne = std::find_if(membuf, membuf + L, [L](fon9::byte b) { return b != static_cast<fon9::byte>(L); });
      if (pne != membuf + L) {
         std::cout << "[ERROR] InnFile.Read|room#=" << L << "|ctx err at=" << (pne - membuf) << std::endl;
         abort();
      }
      rkey = inn.MakeNextRoomKey(rkey, nullptr, 0);
   }
}

void TestInnFunc() {
   remove(kInnFileName);
   fon9::InnFile::OpenArgs args{kInnFileName, kBlockSize};
   fon9::InnFile           inn;
   TestInnOpen(inn, args, fon9::InnFile::OpenResult{0});

   std::cout << "[TEST ] InnFile.Rewrite...";
   for (size_t L = 0; L < sizeof(membuf); ++L) {
      memset(membuf, static_cast<fon9::byte>(L), L);
      auto rkey = inn.MakeNewRoom(static_cast<fon9::InnRoomType>(L), static_cast<fon9::InnFile::SizeT>(L));
      auto res = inn.Rewrite(rkey, membuf, static_cast<fon9::InnFile::SizeT>(L));
      if (res != L) {
         std::cout << "\r" "[ERROR] InnFile.Rewrite|res=" << res << "|request=" << L << std::endl;
         abort();
      }
   }
   CheckRooms(inn, false);
   std::cout << "\r" "[OK   ] InnFile.Rewrite & CheckRooms success." << std::endl;

   // 測試 FreeRoom()
   fon9::InnRoomType kRoomTypeFreed = 0xff;
   std::cout << "[TEST ] InnFile.ClearRoom";
   auto rkey = inn.MakeRoomKey(0, nullptr, 0);
   for (size_t L = 0; L < sizeof(membuf); ++L) {
      auto nkey = inn.MakeNextRoomKey(rkey, nullptr, 0);
      inn.FreeRoom(rkey, kRoomTypeFreed);
      rkey = std::move(nkey);
   }
   // 檢查 FreeRoom() 的結果.
   rkey = inn.MakeRoomKey(0, nullptr, 0);
   for (size_t L = 0; L < sizeof(membuf); ++L) {
      if (rkey.GetDataSize() != 0 || rkey.GetRoomType() != kRoomTypeFreed) {
         std::cout << "\r" "[ERROR] InnFile.ClearRoom"
            "|room#=" << L <<
            "|roomType=" << static_cast<unsigned>(rkey.GetRoomType()) <<
            "|dataSize=" << rkey.GetDataSize() << std::endl;
         abort();
      }
      rkey = inn.MakeNextRoomKey(rkey, nullptr, 0);
   }
   std::cout << "\r" "[OK   ]" << std::endl;

   // 測試使用 Write() 寫入資料.
   std::cout << "[TEST ] InnFile.Write...";
   rkey = inn.MakeRoomKey(0, nullptr, 0);
   for (size_t L = 0; L < sizeof(membuf); ++L) {
      rkey = inn.ReallocRoom(rkey.GetRoomPos(), static_cast<fon9::InnRoomType>(L), static_cast<fon9::InnFile::SizeT>(L));
      memset(membuf, static_cast<fon9::byte>(L), L);
      auto res = inn.Write(rkey, 0, membuf, static_cast<fon9::InnFile::SizeT>(L));
      if (res != L) {
         std::cout << "\r" "[ERROR] InnFile.Write|res=" << res << "|request=" << L << std::endl;
         abort();
      }
      rkey = inn.MakeNextRoomKey(rkey, nullptr, 0);
   }
   CheckRooms(inn, true);
   std::cout << "\r" "[OK   ] InnFile.Write & CheckRooms success." << std::endl;

   // 測試 Reduce().
   std::cout << "[TEST ] InnFile.Reduce...";
   rkey = inn.MakeRoomKey(0, nullptr, 0);
   for (size_t L = 0; L < sizeof(membuf); ++L) {
      if (rkey.GetDataSize() < sizeof(L))
         inn.Rewrite(rkey, &L, sizeof(L));
      else
         inn.Reduce(rkey, sizeof(L), &L, sizeof(L));
      if (rkey.GetDataSize() != sizeof(L)) {
         std::cout << "\r" "[ERROR] InnFile.Reduce"
            "|room#=" << L << "|afterDataSize=" << rkey.GetDataSize() << std::endl;
         abort();
      }
      rkey = inn.MakeNextRoomKey(rkey, nullptr, 0);
   }
   // 檢查 Reduce() 的結果.
   size_t exRoomHeader;
   rkey = inn.MakeRoomKey(0, &exRoomHeader, sizeof(exRoomHeader));
   for (size_t L = 0; L < sizeof(membuf); ++L) {
      if (rkey.GetDataSize() != sizeof(L)
          || rkey.GetRoomType() != static_cast<fon9::InnRoomType>(L)
          || exRoomHeader != L) {
         std::cout << "\r" "[ERROR] InnFile.Reduce"
            "|room#=" << L <<
            "|roomType=" << static_cast<unsigned>(rkey.GetRoomType()) <<
            "|dataSize=" << rkey.GetDataSize() <<
            "|exRoomHeader=" << exRoomHeader <<
            std::endl;
         abort();
      }
      rkey = inn.MakeNextRoomKey(rkey, &exRoomHeader, sizeof(exRoomHeader));
   }
   std::cout << "\r" "[OK   ] InnFile.Reduce & CheckRooms success." << std::endl;
}

//--------------------------------------------------------------------------//

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   //_CrtSetBreakAlloc(176);
#endif
   fon9::AutoPrintTestInfo utinfo("InnFile");
   TestInnOpen();
   TestInnFunc();
   remove(kInnFileName);
}
