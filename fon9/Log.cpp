// \file fon9/Log.cpp
// \author fonwinz@gmail.com
#include "fon9/Log.hpp"
#include "fon9/ThreadId.hpp"

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
   fwrite(buf.GetCurrent(), 1, buf.GetUsedSize(), stdout);
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
