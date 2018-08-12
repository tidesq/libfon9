/// \file fon9/ConsoleIO.cpp
/// \author fonwinz@gmail.com
#include "fon9/ConsoleIO.hpp"
#include <stdio.h>
#include <ctype.h>

#ifndef fon9_WINDOWS
#include <unistd.h>
#include <termios.h>
#include <string.h>
#endif

namespace fon9 {

#ifndef fon9_WINDOWS
fon9_API int getch() {
   struct termios cur;
   memset(&cur, 0, sizeof(cur));
   if (tcgetattr(STDIN_FILENO, &cur) < 0) {
      // perror("tcsetattr()"); // 不是 tty, 也許用 "< inputfile" 或 pipeline | 方式執行.
   }
   const struct termios old = cur;
   cfmakeraw(&cur);
   if (tcsetattr(STDIN_FILENO, TCSANOW, &cur) < 0) {
      // perror("tcsetattr TCSANOW");
   }
   int ch = getchar();
   if (tcsetattr(STDIN_FILENO, TCSANOW, &old) < 0) {
      // perror ("tcsetattr ~TCSANOW");
   }
   return ch;
}
#endif

fon9_API size_t getpass(const char prompt[], char* pwbuf, const size_t buflen) {
   FILE* fprompt = stdout;
   fprintf(fprompt, "%s", prompt);
   size_t pos = 0;
   pwbuf[0] = 0;
   for (;;) {
      fflush(fprompt);
      int ch = getch();
      switch (ch) {
      case EOF:
      case '\n':
      case '\r':
         return pos;
      case '\b':
         if (pos > 0) {
            fprintf(fprompt, "\b \b");
            pwbuf[--pos] = 0;
         }
         continue;
      default:
         if (0 < ch && isprint(static_cast<unsigned char>(ch))) {
            if (pos < buflen - 1) {
               pwbuf[pos++] = static_cast<char>(ch);
               pwbuf[pos] = 0;
               fputc('*', fprompt);
               continue;
            } // else = over buflen.
         } // else = control code.
         fputc('\a', fprompt);//beep.
         break;
      }
   }
}

} // namespaces
