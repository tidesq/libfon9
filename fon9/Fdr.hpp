/// \file fon9/Fdr.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_Fdr_hpp__
#define __fon9_Fdr_hpp__
#include "fon9/sys/Config.hpp"

#ifdef fon9_POSIX
#include <fcntl.h>
#include <errno.h>
#endif

namespace fon9 {

/// \ingroup Misc
/// 協助不同 OS 保留 fd 的地方, 應自行管理: 開啟及關閉, 例如參考: FdrAuto.
/// - Linux 的 file descriptor.
/// - Windows 的 HANDLE.
/// - 這裡不提供解構(因為無法判斷要如何關閉: close()? closesocket()? CloseHandle()? ...),
///   您應該自行處理 FD_ 的關閉作業.
class Fdr final {
   fon9_NON_COPY_NON_MOVE(Fdr);
public:
#ifdef fon9_WINDOWS
   using fdr_t = HANDLE;
   /// 無效的 file handle 值.
   static constexpr fdr_t  kInvalidValue = INVALID_HANDLE_VALUE;
#else
   using fdr_t = int;
   /// 無效的 file handle 值.
   enum { kInvalidValue = -1 };
#endif

   /// 實際被管理的 fd 或 HANDLE.
   fdr_t FD_;

   /// 建構時指定 fd.
   explicit Fdr(fdr_t fd = kInvalidValue) : FD_{fd} {
   }

   /// fd 是否為有效值.
   bool IsReadyFD() const {
      return this->FD_ != kInvalidValue;
   }

   #ifdef fon9_POSIX
   /// 設定此 fd 為 non block 操作模式.
   bool SetNonBlock() const {
      int fflags = fcntl(this->FD_, F_GETFL);
      fflags |= O_NONBLOCK;
      return fcntl(this->FD_, F_SETFL, fflags) != -1;
   }
   #endif
};

/// \ingroup Misc
/// 針對 O_NONBLOCK 的 read/write 操作,
/// 有些錯誤代碼表示還要再試一次, 並不是「錯誤」此時傳回 0.
inline int ErrorCannotRetry(int eno) {
   switch (eno) {
   case EAGAIN:
   case EINTR:
      return 0;
   default:
      return eno;
   }
}

/// \ingroup Misc
/// 解構時會自動呼叫 close() 或 CloseHandle()
class FdrAuto {
   fon9_NON_COPYABLE(FdrAuto);
   Fdr   Fdr_;
   void MoveIn(FdrAuto& r) {
      this->Fdr_.FD_ = r.Fdr_.FD_;
      r.Fdr_.FD_ = Fdr::kInvalidValue;
   }

public:
   FdrAuto() = default;

   /// 使用 fdr_t 建構, 建構後擁有 fd, 解構時自動關閉.
   explicit FdrAuto(Fdr::fdr_t fd) : Fdr_(fd) {
   }

   FdrAuto(FdrAuto&& r) {
      MoveIn(r);
   }
   FdrAuto& operator=(FdrAuto&& r) {
      this->Close();
      this->MoveIn(r);
      return *this;
   }

   /// 解構時會自動呼叫 close(this->FD_);
   /// Windows 會呼叫 CloseHandle(this->FD_);
   ~FdrAuto() {
      this->Close();
   }

   Fdr::fdr_t GetFD() const {
      return this->Fdr_.FD_;
   }

   /// 傳回 fd, 並放棄擁有權, 您必須自行關閉取得的fd.
   Fdr::fdr_t ReleaseFD() {
      Fdr::fdr_t fd = this->Fdr_.FD_;
      this->Fdr_.FD_ = Fdr::kInvalidValue;
      return fd;
   }

   /// 關閉 fd:
   ///   - Windows: CloseHandle(this->FD_);
   ///   - POSIX:   close(this->FD_);
   void Close() {
      if (this->Fdr_.FD_ == Fdr::kInvalidValue)
         return;
      #ifdef fon9_WINDOWS
         CloseHandle(this->Fdr_.FD_);
      #else
         ::close(this->Fdr_.FD_);
      #endif
      this->Fdr_.FD_ = Fdr::kInvalidValue;
   }

   /// 關閉舊的 fd, 設定新的 fd, this將擁有 fd, this解構時會關閉 fd.
   void SetFD(Fdr::fdr_t fd) {
      if (fd == this->Fdr_.FD_)
         return;
      this->Close();
      this->Fdr_.FD_ = fd;
   }

   bool IsReadyFD() const { return this->Fdr_.IsReadyFD(); }

#ifdef fon9_POSIX
   bool SetNonBlock() { return this->Fdr_.SetNonBlock(); }
#endif
};

} // namespaces
#endif//__fon9_Fdr_hpp__
