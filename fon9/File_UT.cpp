// \file fon9/File_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/File.hpp"
#include "fon9/FilePath.hpp"
#include "fon9/TestTools.hpp"
#include "fon9/RevFormat.hpp"

#ifdef fon9_POSIX
#include <unistd.h>
#include <sys/stat.h>
inline int _rmdir(const char* path) {
   return rmdir(path);
}
#else
#include <direct.h>//_rmdir()
#endif

//--------------------------------------------------------------------------//

void TestStrToFileMode(fon9::StrView s, fon9::FileMode m) {
   if (fon9::StrToFileMode(s) != m) {
      fon9::RevBufferFixedSize<1024> rbuf;
      fon9::RevPrint(rbuf, "|str=", s,
                     "|res=", fon9::ToHex(m),
                     "|FileMode=", fon9::FileModeToStr(m),
                     '\0');//EOS.
      std::cout << rbuf.GetCurrent() << "\r[ERROR]" << std::endl;
      abort();
   }
}
void TestStrToFileMode() {
   std::cout << "[TEST ] StrToFileMode()";
   fon9_WARN_DISABLE_PADDING;
   struct TestItem {
      const char*    ModeStr_;
      fon9::FileMode FileMode_;
   };
   fon9_WARN_POP;
   static const TestItem   testItems[]{
      {"Read", fon9::FileMode::Read},
      {"Write", fon9::FileMode::Write},
      {"Append", fon9::FileMode::Append},
      {"WriteThrough", fon9::FileMode::WriteThrough},
      {"OpenAlways", fon9::FileMode::OpenAlways},
      {"CreatePath", fon9::FileMode::CreatePath},
      {"MustNew", fon9::FileMode::MustNew},
      {"Trunc", fon9::FileMode::Trunc},
      {"DenyRead", fon9::FileMode::DenyRead},
      {"DenyWrite",fon9::FileMode::DenyWrite},
      {"Read+Write+MustNew", fon9::FileMode::Read | fon9::FileMode::Write | fon9::FileMode::MustNew},
   };
   for (const TestItem& i : testItems) {
      TestStrToFileMode(fon9::StrView_cstr(i.ModeStr_), i.FileMode_);
      if (fon9::FileModeToStr(i.FileMode_) != i.ModeStr_) {
         fon9::RevBufferFixedSize<1024> rbuf;
         fon9::RevPrint(rbuf, "|FileMode=", fon9::ToHex(i.FileMode_),
                        "|FileModeToStr()=", fon9::FileModeToStr(i.FileMode_),
                        '\0');//EOS.
         std::cout << rbuf.GetCurrent() << "\r[ERROR]" << std::endl;
         abort();
      }
   }
   static const TestItem   testItems2[]{
      {"r", fon9::FileMode::Read},
      {"w", fon9::FileMode::Write},
      {"a", fon9::FileMode::Append},
      {"o", fon9::FileMode::OpenAlways},
      {"c", fon9::FileMode::CreatePath},
      {"n", fon9::FileMode::MustNew},
      {"rwa", fon9::FileMode::Read | fon9::FileMode::Write | fon9::FileMode::Append},
      {"rwa + c", fon9::FileMode::Read | fon9::FileMode::Write | fon9::FileMode::Append | fon9::FileMode::CreatePath},
      {"n + Read + Write", fon9::FileMode::MustNew | fon9::FileMode::Read | fon9::FileMode::Write},
   };
   for (const TestItem& i : testItems2) {
      TestStrToFileMode(fon9::StrView_cstr(i.ModeStr_), i.FileMode_);
   }
   std::cout << "\r[OK   ]" << std::endl;
}

//--------------------------------------------------------------------------//

void RemoveTestFiles() {
#ifdef fon9_WINDOWS
   // Windows: 不能 remove ReadOnly 的檔案.
   SetFileAttributes("TestRd", FILE_ATTRIBUTE_NORMAL);
#endif
   remove("TestRd");
   remove("TestWr");
   _rmdir("TestDir");
}

//--------------------------------------------------------------------------//

std::ostream& operator<<(std::ostream& os, const fon9::ErrC& errc) {
   fon9::RevBufferFixedSize<1024> rbuf;
   fon9::RevPrint(rbuf, errc, '\0');
   return os << rbuf.GetCurrent();
}

void PrintFileResult(const char* msg, const fon9::File& fd, fon9::File::Result fr, fon9::File::Result expect) {
   std::cout << (fr == expect ? "[OK   ] " : "[ERROR] ")
      << msg << "|mode=" << fon9::FileModeToStr(fd.GetOpenMode());

   if (fd.GetOpenName().size() < 128)
      std::cout << "|fn=" << fd.GetOpenName();

   if (!fr)
      std::cout << "|errc=" << fr.GetError();
   else
      std::cout << "|OK=" << fr.GetResult();

   if (fr == expect) {
      std::cout << std::endl;
      return;
   }

   std::cout << "|expect=";
   if (expect)
      std::cout << expect.GetResult();
   else
      std::cout << expect.GetError();
   std::cout << "|***** not expect *****" << std::endl;
   abort();
}

//--------------------------------------------------------------------------//

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

   fon9::AutoPrintTestInfo utinfo{"File/FilePath"};
   TestStrToFileMode();

   utinfo.PrintSplitter();
   RemoveTestFiles();
   
   fon9::File  fd;
   char        fname[1024*64];
   memset(fname, 'a', sizeof(fname));
   fname[sizeof(fname) - 1] = 0;

   using FM = fon9::FileMode;
   using FR = fon9::File::Result;

   PrintFileResult("LongFileName:   ", fd, fd.Open(fname,          FM::Write), FR{std::errc::filename_too_long});
   PrintFileResult("FileNotExist:   ", fd, fd.Open("NotExistFile", FM::Read),  FR{std::errc::no_such_file_or_directory});
   // 如果路徑(TestRd)不存在: no_such_file_or_directory
   PrintFileResult("PathNotExist:   ", fd, fd.Open("TestRd/Test",  FM::Read | FM::OpenAlways), FR{std::errc::no_such_file_or_directory});
   PrintFileResult("Open   'TestRd':", fd, fd.Open("TestRd",       FM::Read | FM::OpenAlways), FR{0});
   // 如果 TestRd 存在, 但不是路徑: not_a_directory (Windows: ERROR_PATH_NOT_FOUND)
   PrintFileResult("Open   'file/f':", fd, fd.Open("TestRd/Test",  FM::Read), FR{std::errc::not_a_directory});
   PrintFileResult("Create 'file/f':", fd, fd.Open("TestRd/Test",  FM::Read | FM::CreatePath), FR{std::errc::not_a_directory});

#ifdef fon9_WINDOWS
   SetFileAttributes("TestRd", FILE_ATTRIBUTE_READONLY);
#else
   chmod("TestRd", S_IRUSR);
#endif
   std::cout << "set readonly 'TestRd'\n";
   // ERR: 開啟唯獨檔 for Write.
   PrintFileResult("OpenWr 'TestRd':", fd, fd.Open("TestRd", FM::Write), FR{std::errc::permission_denied});

   // ERR: 開啟檔案, 但名稱是路徑.
   fon9::FilePath::MakePathTree("TestDir/");
   std::cout << "mkdir 'TestDir/'\n";
   PrintFileResult("OpenDir:        ", fd, fd.Open("TestDir", FM::Read | FM::CreatePath), FR{std::errc::is_a_directory});
   // ERR: 檔案沒開啟, 讀取失敗.
   char buf[128] = "aaa\n";
   PrintFileResult("Read EBADF:     ", fd, fd.Read(0, buf, 4), FR{std::errc::bad_file_descriptor});
   std::cout << std::endl;

   // 開啟檔案讀入.
   PrintFileResult("Open MustNew:   ", fd, fd.Open("TestRd", FM::Read | FM::MustNew), FR{std::errc::file_exists});
   PrintFileResult("Open Read:      ", fd, fd.Open("TestRd", FM::Read),               FR{0});
   std::string rname = fon9::FilePath::GetFullName(&fd.GetOpenName());
   std::cout << "RealFileName=" << rname << std::endl;
   // 從超過檔尾的地方讀入: 結果=成功 0 bytes.
   PrintFileResult("Read BadPos:    ", fd, fd.Read(999, buf, 4), FR{0});
   // ERR: 寫入 fon9::FileMode::Read 的檔案
   PrintFileResult("Write RdFile:   ", fd, fd.Write(0, buf, 4),  FR{std::errc::bad_file_descriptor});
   std::cout << std::endl;

   // 開啟檔案寫入.
   PrintFileResult("Open Write:     ", fd, fd.Open("TestWr", FM::Write | FM::MustNew), FR{0});
   PrintFileResult("Read WrFile:    ", fd, fd.Read(0, buf, 4),                         FR{std::errc::bad_file_descriptor});
   PrintFileResult("Write 4 bytes:  ", fd, fd.Write(0, buf, 4),                        FR{4});
   // ERR: 直接移到很遠的地方寫入.
   PrintFileResult("Write BadPos:   ", fd, fd.Write(std::numeric_limits<fon9::File::PosType>::max() - 4, buf, 4), FR{std::errc::invalid_argument});
   std::cout << std::endl;

#if 0
   fd.Open("E:/bigfile", FM::Write | FM::OpenAlways);
   PrintFileResult("Write LargePos: ", fd, fd.Write(static_cast<fon9::File::PosType>(1024*1024*1024)*4, buf, 4), FR{std::errc::file_too_large});
#endif

#if 0
   fon9::File::PosType pos = 0;
   for (int L = 0; L < 1000 * 1000; ++L) {
      FR fr{fd.Write(pos, fname, sizeof(fname))};
      if (fr) {
         pos += fr.GetResult();
         if (fr.GetResult() == sizeof(fname))
            continue;
         PrintFileResult("Write LargeData:", fd, fr, fr);
         fr = fd.Write(pos, fname, sizeof(fname));
      }
      PrintFileResult("Write LargeData:", fd, fr, FR{std::errc::no_space_on_device});
      break;
   }
#endif

#if 0
   // fat32: /dir/0..32766: ERROR_CANNOT_MAKE(82) => too_many_files_open_in_system
   for (int dL = 0; dL < 5; ++dL)
      for (int L = 0; L < 1000 * 1000; ++L) {
         fon9::StrView fn{fname, static_cast<size_t>(sprintf(fname, "E:/dir/%d/TestWrX_%d", dL, L))};
         FR fr = fd.Open(fn.ToString(), FM::Write | FM::CreatePath);
         if (!fr) {
            PrintFileResult("Open many files:", fd, fr, FR{std::errc::too_many_files_open_in_system});
            break;
         }
         // fd.Write(0, "aaa", 3);
         fd.ReleaseFD();
      }
#endif

   fd.Close();
   RemoveTestFiles();
}
