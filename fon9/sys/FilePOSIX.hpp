/// \file fon9/sys/FilePOSIX.hpp
/// \author fonwinz@gmail.com
#include "fon9/File.hpp"
#include "fon9/FilePath.hpp"
#include "fon9/buffer/DcQueueList.hpp"

namespace fon9 { namespace impl {

inline static File::Result CheckRWResult(ssize_t sz) {
   if (sz < 0)
      return File::Result{GetSysErrC()};
   return File::Result{static_cast<size_t>(sz)};
}
inline static File::Result PosWrite(int fd, const void* buf, size_t count, File::PosType offset) {
   return CheckRWResult(pwrite(fd, buf, count, offset));
}
inline static File::Result PosWrite(FdrAuto& fd, DcQueueList& outbuf, File::PosType offset) {
   return DeviceOutputIovec(outbuf, [&fd, &offset](struct iovec* iov, size_t iovcnt)->File::Result {
      File::Result res = CheckRWResult(pwritev(fd.GetFD(), iov, static_cast<int>(iovcnt), offset));
      if (res)
         offset += res.GetResult();
      return res;
   });
}
inline static File::Result AppWrite(int fd, const void* buf, size_t count) {
   return CheckRWResult(write(fd, buf, count));
}
inline static File::Result AppWrite(FdrAuto& fd, DcQueueList& outbuf) {
   return DeviceOutputIovec(outbuf, [&fd](struct iovec* iov, size_t iovcnt)->File::Result {
      return CheckRWResult(writev(fd.GetFD(), iov, static_cast<int>(iovcnt)));
   });
}
inline static File::Result PosRead(int fd, void* buf, File::SizeType count, File::PosType offset) {
   return CheckRWResult(pread(fd, buf, count, offset));
}
inline static File::Result SeekToEnd(int fd) {
   off_t fp = lseek(fd, 0, SEEK_END);
   if (fp < 0)
      return File::Result{GetSysErrC()};
   return File::Result{static_cast<File::PosType>(fp)};
}
inline static int DupHandle(int fd) {
   return dup(fd);
}
inline static File::Result GetFileSize(int fd) {
   struct stat fst;
   if (fstat(fd, &fst) == 0)
      return File::Result{static_cast<File::PosType>(fst.st_size)};
   return File::Result{GetSysErrC()};
}
inline static File::Result SetFileSize(int fd, File::PosType newFileSize) {
   if (ftruncate(fd, newFileSize) == 0)
      return File::Result{newFileSize};
   return File::Result{GetSysErrC()};
}
inline static void FlushFileBuffers(int fd) {
   fdatasync(fd);
}
//------------------------------------------------
static File::Result OpenFileFD(FdrAuto& fd, const std::string& fname, FileMode fmode) {
   int oflag = IsEnumContainsAny(fmode, FileMode::Read | FileMode::DenyRead)
      ? (IsEnumContainsAny(fmode, FileMode::Append | FileMode::Write | FileMode::DenyWrite)
         ? O_RDWR : O_RDONLY)
      : O_WRONLY;
   if (IsEnumContains(fmode, FileMode::Append))
      oflag |= O_APPEND;
   if (IsEnumContains(fmode, FileMode::Trunc))
      oflag |= O_TRUNC;
   if (IsEnumContains(fmode, FileMode::WriteThrough))
      oflag |= O_DSYNC;
   if (IsEnumContains(fmode, FileMode::MustNew))
      oflag |= O_CREAT | O_EXCL;
   else
      if (IsEnumContainsAny(fmode, FileMode::OpenAlways | FileMode::CreatePath))
         oflag |= O_CREAT;
   FdrAuto  fdn{open(fname.c_str(), oflag, FilePath::DefaultFileAccessMode_)};
   while (fdn.IsReadyFD()) {
      if (IsEnumContainsAny(fmode, FileMode::DenyRead | FileMode::DenyWrite)) {
         struct flock lck;
         ZeroStruct(lck);
         lck.l_whence = SEEK_SET;
         if (IsEnumContains(fmode, FileMode::DenyRead)) {
            lck.l_type = F_RDLCK;
            if (fcntl(fdn.GetFD(), F_SETLK, &lck) == -1)
               break;
         }
         if (IsEnumContains(fmode, FileMode::DenyWrite)) {
            lck.l_type = F_WRLCK;
            if (fcntl(fdn.GetFD(), F_SETLK, &lck) == -1)
               break;
         }
      }
      fd = std::move(fdn);
      return File::Result{0};
   }
   File::Result res{GetSysErrC()};//避免 fdn.Close(); 改變了 errno, 所以先取出.
   fdn.Close();
   return res;
}

} } // namespaces
