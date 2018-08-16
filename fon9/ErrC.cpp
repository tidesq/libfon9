/// \file fon9/ErrC.cpp
/// \author fonwinz@gmail.com
#include "fon9/ErrC.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 {

#ifdef fon9_WINDOWS
static const std::error_category& WinSystemErrorCategory() noexcept;
using WinSystemErrorBase = std::error_category;
class WinSystemError : public WinSystemErrorBase {
   fon9_NON_COPY_NON_MOVE(WinSystemError);
public:
   WinSystemError() = default;

   virtual const char *name() const noexcept override {
      return ("WinSys");
   }

   virtual std::string message(int _Errcode) const override {
      // 來源: "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\crt\src\stl\syserror.cpp"
      // _Winerror_message(); CP_ACP 改成 CP_UTF8
      // _Wide, _Narrow 使用 stack buffer, 不使用 wstring, string;
      const unsigned long  _Size = 32767;
      wchar_t              _Wide[_Size];
      if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                         0, static_cast<unsigned long>(_Errcode), 0, &_Wide[0], _Size, 0) != 0) {
         char      _Narrow[_Size];
         const int _Bytes = WideCharToMultiByte(CP_UTF8, 0, &_Wide[0], -1, _Narrow, _Size, 0, 0) - 1;
         if (_Bytes > 0) {
            const char* pend = StrFindTrimTail(_Narrow, _Narrow + _Bytes);
            if (_Narrow != pend)
               return std::string(static_cast<const char*>(_Narrow), pend);
         }
      }
      RevBufferFixedSize<1024> rbuf;
      RevPrint(rbuf, "WinERR=", static_cast<unsigned long>(_Errcode));
      return rbuf.ToStrT<std::string>();
   }

   virtual std::error_condition default_error_condition(int _Errval) const noexcept {
      int _Posv;
      switch (_Errval) {
      case ERROR_FILENAME_EXCED_RANGE: _Posv = static_cast<int>(std::errc::filename_too_long); break;
      case ERROR_DIRECTORY_NOT_SUPPORTED: _Posv = static_cast<int>(std::errc::is_a_directory); break;
      case ERROR_INVALID_PARAMETER: _Posv = static_cast<int>(std::errc::invalid_argument); break;
      default: _Posv = std::_Winerror_map(_Errval); break;
      }
      if (_Posv != 0)
         return (std::error_condition(_Posv, std::generic_category()));
      else
         return (std::error_condition(_Errval, WinSystemErrorCategory()));
   }
};
inline static const std::error_category& WinSystemErrorCategory() noexcept {
   return (std::_Immortalize<WinSystemError>());
}

fon9_API ErrC GetSysErrC(DWORD eno) {
   return WinSystemErrorCategory().default_error_condition(static_cast<int>(eno));
}

#else//--------------------------------------------------------------------------//

fon9_API ErrC GetSysErrC(int eno) {
   return std::generic_category().default_error_condition(eno);
}
#endif

} // namespace
