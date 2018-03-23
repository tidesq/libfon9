/// \file fon9/sys/FileWin32.hpp
/// \author fonwinz@gmail.com
#include "fon9/File.hpp"
#include "fon9/FilePath.hpp"
#include "fon9/buffer/DcQueueList.hpp"

namespace fon9 { namespace impl {

fon9_MSC_WARN_DISABLE(4820);
struct FileRWOverlapped : public OVERLAPPED {
   File::PosType RequestOffset_;
   FileRWOverlapped(File::PosType offset) {
      memset(this, 0, sizeof(*this));
      this->RequestOffset_ = offset;
   }
   void OnBeforeRW() {
      this->Offset = static_cast<DWORD>(this->RequestOffset_);
      this->OffsetHigh = static_cast<DWORD>(this->RequestOffset_ >> 32);
   }
   void OnAfterRW(DWORD rwsz) {
      this->RequestOffset_ += rwsz;
   }
};
fon9_MSC_WARN_POP;

template <class FnRW, typename BufT>
inline static File::Result WindowsRW(HANDLE fd, BufT buf, size_t count, FnRW&& fnRW) {
   constexpr DWORD maxRW = std::numeric_limits<DWORD>::max();
   size_t totsz = 0;
   while (count > 0) {
      const DWORD reqsz = (count < maxRW ? static_cast<DWORD>(count) : maxRW);
      DWORD rwsz;
      if (!fnRW(fd, buf, reqsz, &rwsz)) {
         if (totsz > 0)
            break;
         // wrong read/write mode should return EBADF, not EACCES. <= 調整成與 linux 有相同的行為.
         DWORD eno = ::GetLastError();
         return File::Result{eno == ERROR_ACCESS_DENIED ? std::make_error_condition(std::errc::bad_file_descriptor) : GetSysErrC(eno)};
      }
      totsz += rwsz;
      if (rwsz < reqsz)
         break;
      if (count -= reqsz <= 0)
         break;
      buf = reinterpret_cast<BufT>(reinterpret_cast<uintptr_t>(buf) + rwsz);
   }
   return File::Result{totsz};
}
inline static File::Result PosWrite(HANDLE fd, const void* buf, size_t count, File::PosType offset) {
   struct PosWr : public FileRWOverlapped {
      using FileRWOverlapped::FileRWOverlapped;
      PosWr() = delete;
      bool operator()(HANDLE fd, const void* buf, DWORD reqsz, DWORD* rwsz) {
         this->OnBeforeRW();
         if (!::WriteFile(fd, buf, reqsz, rwsz, this))
            return false;
         this->OnAfterRW(*rwsz);
         return true;
      }
   } poswr{offset};
   return WindowsRW(fd, const_cast<void*>(buf), count, poswr);
}
inline static File::Result PosWrite(FdrAuto& fd, DcQueueList& outbuf, File::PosType offset) {
   return DeviceOutputBlock(outbuf, [&fd, &offset](const void* buf, size_t count) -> File::Result {
      File::Result res = PosWrite(fd.GetFD(), buf, count, offset);
      if (res)
         offset += res.GetResult();
      return res;
   });
}
inline static File::Result AppWrite(HANDLE fd, const void* buf, size_t count) {
   return WindowsRW(fd, buf, count, [](HANDLE fd, const void* buf, DWORD reqsz, DWORD* rwsz) -> BOOL {
      return ::WriteFile(fd, buf, reqsz, rwsz, nullptr);
   });
}
inline static File::Result AppWrite(FdrAuto& fd, DcQueueList& outbuf) {
   return DeviceOutputBlock(outbuf, [&fd](const void* buf, size_t count) -> File::Result {
      return AppWrite(fd.GetFD(), buf, count);
   });
}
inline static File::Result PosRead(HANDLE fd, void* buf, File::SizeType count, File::PosType offset) {
   struct PosRd : public FileRWOverlapped {
      using FileRWOverlapped::FileRWOverlapped;
      PosRd() = delete;
      bool operator()(HANDLE fd, void* buf, DWORD reqsz, DWORD* rwsz) {
         this->OnBeforeRW();
         if (::ReadFile(fd, buf, reqsz, rwsz, this)) {
            this->OnAfterRW(*rwsz);
            return true;
         }
         if (::GetLastError() != ERROR_HANDLE_EOF)
            return false;
         *rwsz = 0;
         return true;
      }
   } posrd{offset};
   return WindowsRW(fd, const_cast<void*>(buf), count, posrd);
}
inline static File::Result SeekToEnd(HANDLE fd) {
   LARGE_INTEGER fp{0};
   if (::SetFilePointerEx(fd, fp, &fp, FILE_END))
      return File::Result{static_cast<File::PosType>(fp.QuadPart)};
   return File::Result{GetSysErrC()};
}
inline static File::Result GetFileSize(HANDLE fd) {
   LARGE_INTEGER  fsz;
   if (::GetFileSizeEx(fd, &fsz))
      return File::Result{static_cast<File::PosType>(fsz.QuadPart)};
   return File::Result{GetSysErrC()};
}
inline static File::Result SetFileSize(HANDLE fd, File::PosType newFileSize) {
   FILE_END_OF_FILE_INFO fpos;
   fpos.EndOfFile.QuadPart = static_cast<LONGLONG>(newFileSize);
   if (SetFileInformationByHandle(fd, FileEndOfFileInfo, &fpos, sizeof(fpos)))
      return File::Result{newFileSize};
   return File::Result{GetSysErrC()};
}
inline static HANDLE DupHandle(HANDLE fd) {
   HANDLE hProcessHandle = GetCurrentProcess();
   HANDLE hDupResult;
   if (::DuplicateHandle(hProcessHandle
                         , fd
                         , hProcessHandle//hTargetProcessHandle
                         , &hDupResult
                         , 0//dwDesiredAccess, ignore
                         , FALSE//bInheritHandle
                         , DUPLICATE_SAME_ACCESS//dwOptions
                         ))
      return hDupResult;
   return Fdr::kInvalidValue;
}

//--------------------------------------------------------------------------//

static File::Result OpenFileFD(FdrAuto& fd, const std::string& fname, FileMode fmode) {
   DWORD desiredAccess = 0;
   if (IsEnumContains(fmode, FileMode::Read))
      desiredAccess |= GENERIC_READ;
   if (IsEnumContains(fmode, FileMode::Append))
      desiredAccess |= (fname == "CON" ? GENERIC_WRITE : FILE_APPEND_DATA);
   else
      if (IsEnumContains(fmode, FileMode::Write))
         desiredAccess |= GENERIC_WRITE;

   DWORD shareMode = (FILE_SHARE_READ | FILE_SHARE_WRITE);
   if (IsEnumContains(fmode, FileMode::DenyRead))
      shareMode = (shareMode & ~FILE_SHARE_READ);
   if (IsEnumContains(fmode, FileMode::DenyWrite))
      shareMode = (shareMode & ~FILE_SHARE_WRITE);

   DWORD creationDisposition = static_cast<DWORD>(
      IsEnumContains(fmode, FileMode::MustNew) ? CREATE_NEW
      : IsEnumContainsAny(fmode, FileMode::OpenAlways | FileMode::CreatePath)
         ? (IsEnumContains(fmode, FileMode::Trunc) ? CREATE_ALWAYS : OPEN_ALWAYS)
         : (IsEnumContains(fmode, FileMode::Trunc) ? TRUNCATE_EXISTING : OPEN_EXISTING));
   DWORD flagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
   if (IsEnumContains(fmode, FileMode::WriteThrough))
      flagsAndAttributes |= FILE_FLAG_WRITE_THROUGH;

   struct stat fst;
   if (creationDisposition == TRUNCATE_EXISTING && desiredAccess == FILE_APPEND_DATA) {
      if (stat(fname.c_str(), &fst) != 0)
         return std::make_error_condition(std::errc::no_such_file_or_directory);
      creationDisposition = CREATE_ALWAYS;
   }
   fd.SetFD(CreateFile(fname.c_str()
                       , desiredAccess
                       , shareMode
                       , nullptr//Security
                       , creationDisposition
                       , flagsAndAttributes
                       , nullptr//hTemplateFile
                       ));
   if (fd.IsReadyFD())
      return File::Result{0};
   DWORD eno = ::GetLastError();
   if (eno == ERROR_PATH_NOT_FOUND) {
      std::string fpath = FilePath::ExtractPathName(&fname);
      if (!fpath.empty() && stat(fpath.c_str(), &fst) != 0)
         return std::make_error_condition(std::errc::no_such_file_or_directory);
      return std::make_error_condition(std::errc::not_a_directory);
   }
   return File::Result{GetSysErrC(eno)};
}

} } // namespaces
