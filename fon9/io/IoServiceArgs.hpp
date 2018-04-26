/// \file fon9/io/IoServiceArgs.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_IoServiceArgs_hpp__
#define __fon9_io_IoServiceArgs_hpp__
#include "fon9/StrView.hpp"

fon9_BEFORE_INCLUDE_STD;
#include <thread>
#include <vector>
fon9_AFTER_INCLUDE_STD;

namespace fon9 { namespace io {

enum class HowWait {
   Unknown,
   Block,
   Yield,
   Busy,
};
inline bool IsBlockWait(HowWait value) {
   return value <= HowWait::Block;
}
fon9_API HowWait StrToHowWait(StrView value);
fon9_API StrView HowWaitToStr(HowWait value);

/// \ingroup io
/// args: "ThreadCount=n|Wait=Policy|Cpus=List|Capacity=0"
/// Policy: Block(default)
struct fon9_API IoServiceArgs {
   using CpuAffinity = std::vector<uint32_t>;
   CpuAffinity CpuAffinity_;

   uint32_t ThreadCount_{2};
   HowWait  HowWait_{HowWait::Block};

   /// 每個 io service thread 可服務的容量, 例如: MaxConnections.
   /// 0 = 由 io service 自行決定最佳值.
   size_t   Capacity_{0};

   IoServiceArgs() = default;
   IoServiceArgs(StrView args) {
      this->Parse(args);
   }

   int GetCpuAffinity(size_t threadPoolIndex) const {
      if (CpuAffinity_.empty())
         return -1;
      return static_cast<int>(CpuAffinity_[threadPoolIndex % CpuAffinity_.size()]);
   }

   /// 用 tag, value 設定參數.
   /// tag         | value
   /// ------------|------------------------------
   /// ThreadCount | > 0
   /// Capacity    | >= 0
   /// Wait        | "Block" or "Busy" or "Yield"
   /// Cpus        | c0, c1, c2 ... 根據 thread pool index 依序選擇 c0 或 c1 或 c2...
   const char* FromTagValue(StrView tag, StrView value);

   /// 傳回發現錯誤的位置, nullptr 表示解析成功.
   const char* Parse(StrView args);
};

/// \ingroup io
/// 提供給 Io Service thread 使用的啟動參數.
struct fon9_API ServiceThreadArgs {
   std::string Name_;
   size_t      ThreadPoolIndex_;
   int         CpuAffinity_;
   HowWait     HowWait_;
   size_t      Capacity_;

   ServiceThreadArgs() = default;
   ServiceThreadArgs(const IoServiceArgs& ioArgs, const std::string& name, size_t index)
      : Name_{name}
      , ThreadPoolIndex_{index}
      , CpuAffinity_{ioArgs.GetCpuAffinity(index)}
      , HowWait_{ioArgs.HowWait_}
      , Capacity_{ioArgs.Capacity_} {
   }

   /// - 透過 fon9_LOG_ThrRun(msgHead, ".ThrRun|name=", this->Name_...) 記錄 log.
   /// - if (this->CpuAffinity_ >= 0) 設定 cpu affinity.
   void OnThrRunBegin(std::thread& thr, StrView msgHead) const;
};

} } // namespaces
#endif//__fon9_io_IoServiceArgs_hpp__
