/// \file fon9/Log.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_Log_hpp__
#define __fon9_Log_hpp__
#include "fon9/RevFormat.hpp"
#include "fon9/TimeStamp.hpp"

namespace fon9 {

// TODO: 在 BufferList 尚未完成之前, 暫時使用固定大小的緩衝區.
struct RevBufferList : public  RevBufferFixedSize<1024> {
   fon9_NON_COPY_NON_MOVE(RevBufferList);
   RevBufferList(size_t) {}
   RevBufferList&& MoveOut() { return std::move(*this); }
};
using BufferList = RevBufferList;

/// \ingroup Misc
/// Log訊息的等級, 數字越小越不重要.
enum class LogLevel : uint8_t {
   /// 追蹤程式運行用的訊息.
   Trace,
   /// 抓蟲用的訊息.
   Debug,
   /// 一般資訊.
   Info,
   /// 重要訊息, 例如: thread start.
   Important,
   /// 警告訊息.
   Warn,
   /// 錯誤訊息.
   Error,
   /// 嚴重錯誤.
   Fatal,
   /// LogLevel 的數量.
   Count,
};
/// \ingroup Misc
/// 執行階段的Log等級設定.
extern fon9_API LogLevel LogLevel_;

using LogLevelText = const char[8];

/// \ingroup Misc
/// 用 LogLevel 為索引, 取得 LogLevel 的 C style 字串.
/// 前面加一個'[', 後面加個']', 後面的']' 可以協助解析log: 找到每行第一個']',之後表示log實際內容.
/// - 例: LogLevel::Trace = "[TRACE]";
/// - 例: LogLevel::Info  = "[INFO ]"
/// - 例: LogLevel::Count = "[?????]";
extern fon9_API const LogLevelText CstrLevelMap[static_cast<int>(LogLevel::Count) + 1];

/// \ingroup Misc
/// 若 level >= LogLevel::Count 則傳回 CstrLevelMap[LogLevel::Count];
inline LogLevelText& GetLevelStr(LogLevel level) {
   return CstrLevelMap[static_cast<unsigned>(level) < static_cast<unsigned>(LogLevel::Count)
      ? static_cast<unsigned>(level)
      : static_cast<unsigned>(LogLevel::Count)];
}

fon9_WARN_DISABLE_PADDING;
/// \ingroup Misc
/// 傳遞給 Log Writer 用的參數.
struct LogArgs {
   TimeStamp   Time_;
   LogLevel    Level_;
   LogArgs(LogLevel lv, TimeStamp tm = UtcNow()) : Time_{tm}, Level_{lv} {
   }
};
fon9_WARN_POP;

/// \ingroup Misc
/// Log 訊息的最後寫入函式型別.
/// args.Time_ = 此筆記錄的時間, 可協助判斷是否需要開啟新檔案.
typedef void (*FnLogWriter) (const LogArgs& logArgs, BufferList&& buf);

/// \ingroup Misc
/// 設定 Log 訊息的最後寫入函式, 預設: 寫到 stdout(預設值不是 thread safe: 可能會 interlace)
/// 如果 fnLogWriter = nullptr 則還原為預設 stdout 輸出.
/// NOT thread safe!
fon9_API void SetLogWriter(FnLogWriter fnLogWriter);

/// \ingroup Misc
/// 如果現在的 LogWriter == fnLogWriter, 則還原為預設 stdout 輸出.
/// NOT thread safe!
fon9_API void ResetLogWriter(FnLogWriter fnLogWriter);

/// \ingroup Misc
/// 把 buf 寫入 log: 透過 SetLogWriter() 設定的 log 寫入函式.
fon9_API void LogWrite(const LogArgs& logArgs, BufferList&& buf);

/// \ingroup Misc
/// 在 rbuf 之前加上 `YYYYMMDD-HHMMSS.uuuuuu thrid[LEVEL]` 之後, 寫入 log:
/// `FnLogWriter(tm, rbuf.MoveOut());` 所以返回前 rbuf 會被清空.
fon9_API void LogWrite(LogLevel level, RevBufferList&& rbuf);

/// \ingroup Misc
/// 根據Log等級(level), 寫入 Log.
/// 記錄的格式: `YYYYMMDD-HHMMSS.uuuuuu thrid[LEVEL]...\n`
/// 雖然這裡(...)沒有強制格式, 但是建議格式為:
///   - `function|tag1=value1|tag2=value2|err=error message`
///   - function 若為 class method, 則使用「.」分隔, 例: "TimedFile.OpenNewFile"
///   - 範例:
///      - fon9_LOG_ERROR("TimedFile.OpenNewFile|FileName=", newFile.GetOpenName(), "|OpenMode=", newFile.GetOpenMode(), "|err=", res);
///      - fon9_LOG_INFO("DllMgr.LoadConfig|seedName=", this->Name_, "|cfgFileName=", cfgFileName);
#define fon9_LOG(level, ...) do {                              \
   if (fon9_UNLIKELY(level >= fon9::LogLevel_)) {              \
      fon9::RevBufferList rbuf_{256+sizeof(fon9::NumOutBuf)};  \
      fon9::RevPrint(rbuf_, __VA_ARGS__, '\n');                \
      fon9::LogWrite(level, std::move(rbuf_));                 \
   }                                                           \
} while(0)

#ifdef fon9_NOLOG_TRACE
#define fon9_LOG_TRACE(...)  do{}while(0)
#else
#define fon9_LOG_TRACE(...)  fon9_LOG(fon9::LogLevel::Trace, __VA_ARGS__)
#endif

#ifdef fon9_NOLOG_DEBUG
#define fon9_LOG_DEBUG(...)  do{}while(0)
#else
#define fon9_LOG_DEBUG(...)  fon9_LOG(fon9::LogLevel::Debug, __VA_ARGS__)
#endif

#ifdef fon9_NOLOG_INFO
#define fon9_LOG_INFO(...)   do{}while(0)
#else
#define fon9_LOG_INFO(...)   fon9_LOG(fon9::LogLevel::Info, __VA_ARGS__)
#endif

#define fon9_LOG_IMP(...)    fon9_LOG(fon9::LogLevel::Important, __VA_ARGS__)
#define fon9_LOG_WARN(...)   fon9_LOG(fon9::LogLevel::Warn, __VA_ARGS__)
#define fon9_LOG_ERROR(...)  fon9_LOG(fon9::LogLevel::Error, __VA_ARGS__)
#define fon9_LOG_FATAL(...)  fon9_LOG(fon9::LogLevel::Fatal, __VA_ARGS__)

#define fon9_LOG_ThrRun(...) fon9_LOG_IMP(__VA_ARGS__)

}// namespace
#endif//__fon9_Log_hpp__
