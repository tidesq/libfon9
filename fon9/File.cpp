/// \file fon9/File.cpp
/// \author fonwinz@gmail.com
#include "fon9/File.hpp"
#include "fon9/Log.hpp"

#ifdef fon9_WINDOWS
#include "fon9/sys/FileWin32.hpp"
#else
#include "fon9/sys/FilePOSIX.hpp"
#endif

namespace fon9 {
using namespace impl;

fon9_WARN_DISABLE_PADDING;
struct FileModeDefine_impl {
   StrView  Name_;
   FileMode Mode_;
};
fon9_WARN_POP;

#define DEF_FILEMODE(m) { StrView{#m}, FileMode::m }
static const FileModeDefine_impl FileModeDef_[]{
   DEF_FILEMODE(Read),
   DEF_FILEMODE(Write),
   DEF_FILEMODE(Append),
   DEF_FILEMODE(WriteThrough),
   DEF_FILEMODE(OpenAlways),
   DEF_FILEMODE(CreatePath),
   DEF_FILEMODE(MustNew),
   DEF_FILEMODE(Trunc),
   DEF_FILEMODE(DenyRead),
   DEF_FILEMODE(DenyWrite),
};

FileMode StrToFileMode(StrView modes) {
   FileMode  fm = FileMode{};
   while (!modes.empty()) {
      StrView fld{StrFetchTrim(modes, '+')};
      if (islower(fld.Get1st())) {
         for (char ch : fld) {
            switch (ch) {
            case 'r':   fm |= FileMode::Read; break;
            case 'w':   fm |= FileMode::Write; break;
            case 'a':   fm |= FileMode::Append; break;
            case 'o':   fm |= FileMode::OpenAlways; break;
            case 'c':   fm |= FileMode::CreatePath; break;
            case 'n':   fm |= FileMode::MustNew; break;
            }
         }
         continue;
      }
      for (const FileModeDefine_impl& i : FileModeDef_) {
         if (i.Name_ == fld) {
            fm |= i.Mode_;
            break;
         }
      }
   }
   return fm;
}
void FileModeToStrAppend(FileMode fm, std::string& out) {
   for (const FileModeDefine_impl& i : FileModeDef_) {
      if (IsEnumContains(fm, i.Mode_)) {
         i.Name_.AppendTo(out);
         if ((fm -= i.Mode_) == FileMode{})
            break;
         out.push_back('+');
      }
   }
}

//--------------------------------------------------------------------------//

File::Result File::Open(std::string fn, FileMode fmode) {
   this->Close();
   this->OpenName_ = std::move(fn);
   this->OpenMode_ = fmode;
   if (IsEnumContains(fmode, FileMode::CreatePath)) {
      std::string      pathName = FilePath::ExtractPathName(&this->OpenName_);
      FilePath::Result rMakeTree = FilePath::MakePathTree(&pathName);
      if (!rMakeTree)
         return Result{rMakeTree.GetError()};
   }
   // 開啟一般檔案, 但該名稱其實是路徑, Windows, Linux 結果不同:
   //    - Windows: ERROR_ACCESS_DENIED
   //    - Linux:   成功開啟!
   // 所以在此處理這種情況.
   struct stat fst;
   if (stat(this->OpenName_.c_str(), &fst) == 0 && S_ISDIR(fst.st_mode))
      return std::make_error_condition(std::errc::is_a_directory);
   return OpenFileFD(this->Fdr_, this->OpenName_, fmode);
}

File File::Duplicate() const {
   File fdup;
   fdup.OpenName_ = this->OpenName_;
   fdup.OpenMode_ = this->OpenMode_;
   fdup.Fdr_.SetFD(DupHandle(this->Fdr_.GetFD()));
   return fdup;
}

void File::Sync() {
   FlushFileBuffers(this->Fdr_.GetFD());
}

File::Result File::GetFileSize() const {
   return impl::GetFileSize(this->Fdr_.GetFD());
}

File::Result File::SetFileSize(PosType newFileSize) {
   return impl::SetFileSize(this->Fdr_.GetFD(), newFileSize);
}

TimeStamp File::GetLastModifyTime() const {
#ifdef fon9_WINDOWS
   FILETIME   wtime;
   if (GetFileTime(this->Fdr_.GetFD(), 0, 0, &wtime)) {
      // The windows epoch starts 1601-01-01T00:00:00Z.
      // It's 11644473600 seconds before the UNIX/Linux epoch (1970-01-01T00:00:00Z).
      // The Windows ticks are in 100 nanoseconds.
      const uint64_t winTick = static_cast<uint64_t>(wtime.dwHighDateTime) << 32 | wtime.dwLowDateTime;
      return TimeStamp{TimeInterval::Make<7>(winTick - 116444736000000000ULL)};
   }
#else
   struct stat st;
   if (fstat(this->Fdr_.GetFD(), &st) == 0) {
      TimeInterval ti = TimeInterval::Make<0>(st.st_mtime) + TimeInterval::Make<9>(st.st_mtim.tv_nsec);
      return TimeStamp{ti};
   }
#endif
   return TimeStamp{};
}

//--------------------------------------------------------------------------//
#define fon9_File_CheckArgs(file, buf)\
   if (!file->IsOpened())\
      return std::make_error_condition(std::errc::bad_file_descriptor);\
   if (buf == nullptr)\
      return std::make_error_condition(std::errc::bad_address);
//----------------------------------------------------------------------
File::Result File::Read(PosType offset, void* buf, SizeType count) {
   fon9_File_CheckArgs(this, buf);
   return PosRead(this->Fdr_.GetFD(), buf, count, offset);
}
File::Result File::Write(PosType offset, const void* buf, size_t count) {
   fon9_File_CheckArgs(this, buf);
   return PosWrite(this->Fdr_.GetFD(), buf, count, offset);
}

#define fon9_File_CheckIsOpened(fptr, outbuf) \
   if (fon9_UNLIKELY(!fptr->IsOpened())) { \
      ErrC errc{std::make_error_condition(std::errc::bad_file_descriptor)}; \
      outbuf.ConsumeErr(errc); \
      return errc; \
   }
//------------------
File::Result File::Write(PosType offset, DcQueueList& outbuf) {
   fon9_File_CheckIsOpened(this, outbuf);
   return PosWrite(this->Fdr_, outbuf, offset);
}
//----------------------------------------------------------------------
#define fon9_File_CheckAppend(file)\
   if (!IsEnumContains(file->OpenMode_, FileMode::Append)) {\
      Result rToEnd = SeekToEnd(file->Fdr_.GetFD());\
      if (!rToEnd)\
         return rToEnd;\
   }
File::Result File::Append(const void* buf, size_t count) {
   fon9_File_CheckArgs(this, buf);
   fon9_File_CheckAppend(this);
   return AppWrite(this->Fdr_.GetFD(), buf, count);
}
File::Result File::Append(DcQueueList& outbuf) {
   fon9_File_CheckIsOpened(this, outbuf);
   // 避免在處理 outbuf 的過程中重新開檔(例: LogFile 透過特殊 BufferNode 處理重新開檔),
   // 造成 fd 的改變, 所以這裡要傳遞 this->Fdr_
   return AppWrite(this->Fdr_, outbuf);
}

} // namespaces
