/// \file fon9/ConsoleIO.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ConsoleIO_hpp__
#define __fon9_ConsoleIO_hpp__
#include "fon9/sys/Config.hpp"

#ifdef fon9_WINDOWS
#include <conio.h> // getch()
#else
#include <sys/ioctl.h>
#endif

namespace fon9 {

#ifdef fon9_WINDOWS
inline int getch() {
   return ::_getch();
}
#else
extern fon9_API int getch();
#endif

/// \ingroup Misc
/// 要求使用者從 console 輸入密碼.
/// buflen 包含 EOS'\0', 一般用法:
/// \code
///   char pwstr[32]; // 最長 31 字元.
///   getpass("Password: ", pwstr, sizeof(pwstr));
/// \endcode
/// \return 不含EOS的長度.
fon9_API size_t getpass(const char prompt[], char* pwbuf, size_t buflen);

#ifdef fon9_WINDOWS
struct winsize { int ws_row, ws_col; };
inline void GetConsoleSize(struct winsize& winsz) {
   CONSOLE_SCREEN_BUFFER_INFO csbi;
   GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
   winsz.ws_row = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
   winsz.ws_col = csbi.srWindow.Right - csbi.srWindow.Left + 1;
}
#else
using winsize = ::winsize;
inline void GetConsoleSize(winsize& winsz) {
   ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsz);
}
#endif

} // namespaces
#endif//__fon9_ConsoleIO_hpp__
