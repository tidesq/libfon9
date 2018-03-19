// \file fon9/Log.cpp
// \author fonwinz@gmail.com
#include "fon9/Log.hpp"
#include "fon9/ThreadId.hpp"
#include "fon9/buffer/DcQueueList.hpp"

namespace fon9 {

fon9_API LogLevel            LogLevel_;
fon9_API const LogLevelText  CstrLevelMap[static_cast<int>(LogLevel::Count) + 1] = {
   "[TRACE]",
   "[DEBUG]",
   "[INFO ]",
   "[IMP  ]",
   "[WARN ]",
   "[ERROR]",
   "[FATAL]",
   "[?????]",
};

static void LogWriteToStdout(const LogArgs& /*logArgs*/, BufferList&& buf) {
   if (buf.empty())
      return;
   DcQueueList dcQueue{std::move(buf)};
   for (;;) {
      const auto  mem{dcQueue.PeekCurrBlock()};
      if (mem.first == nullptr)
         break;
      size_t wrsz = fwrite(mem.first, 1, mem.second, stdout);
      int    eno = (wrsz < mem.second ? errno : 0);
      dcQueue.PopConsumed(wrsz);
      if (eno) {
         std::error_condition econd = std::generic_category().default_error_condition(eno);
         dcQueue.ConsumeErr(econd);
         break;
      }
   }
   assert(dcQueue.empty());
}
static FnLogWriter   FnLogWriter_ = &LogWriteToStdout;

fon9_API void ResetLogWriter(FnLogWriter fnLogWriter) {
   if (FnLogWriter_ == fnLogWriter)
      FnLogWriter_ = &LogWriteToStdout;
}

fon9_API void SetLogWriter(FnLogWriter fnLogWriter) {
   FnLogWriter_ = fnLogWriter ? fnLogWriter : &LogWriteToStdout;
}

fon9_API void LogWrite(const LogArgs& logArgs, BufferList&& buf) {
   FnLogWriter_(logArgs, std::move(buf));
}

fon9_API void LogWrite(LogLevel level, RevBufferList&& rbuf) {
   LogArgs logArgs{level};
   RevPrint(rbuf, ThisThread_.GetThreadIdStr(), GetLevelStr(level));
   RevPut_Date_Time_us(rbuf, logArgs.Time_);
   FnLogWriter_(logArgs, rbuf.MoveOut());
}

}// namespace
