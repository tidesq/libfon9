/// \file fon9/io/IoServiceArgs.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/IoServiceArgs.hpp"
#include "fon9/Utility.hpp"
#include "fon9/StrTo.hpp"
#include "fon9/Log.hpp"
#include "fon9/Outcome.hpp"

namespace fon9 { namespace io {

static const StrView howWaitStrMap[]{
   fon9_MAKE_ENUM_CLASS_StrView_NoSeq(1, HowWait, Block),
   fon9_MAKE_ENUM_CLASS_StrView_NoSeq(2, HowWait, Yield),
   fon9_MAKE_ENUM_CLASS_StrView_NoSeq(3, HowWait, Busy),
};

fon9_API HowWait StrToHowWait(StrView value) {
   int idx = 0;
   for (const StrView& v : howWaitStrMap) {
      ++idx;
      if (v == value)
         return static_cast<HowWait>(idx);
   }
   return HowWait::Unknown;
}

fon9_API StrView HowWaitToStr(HowWait value) {
   size_t idx = static_cast<size_t>(value) - 1;
   if (idx >= numofele(howWaitStrMap))
      return StrView("Unknown");
   return howWaitStrMap[idx];
}

//--------------------------------------------------------------------------//

const char* IoServiceArgs::FromTagValue(StrView tag, StrView value) {
   if (tag == "ThreadCount") {
      if ((this->ThreadCount_ = StrTo(value, 0u)) <= 0) {
         this->ThreadCount_ = 1;
         return value.begin();
      }
   }
   else if (tag.Compare("Capacity") == 0)
      this->Capacity_ = StrTo(value, 0u);
   else if (tag.Compare("Wait") == 0) {
      if ((this->HowWait_ = StrToHowWait(value)) == HowWait::Unknown) {
         this->HowWait_ = HowWait::Block;
         return value.begin();
      }
   }
   else if (tag.Compare("Cpus") == 0) {
      for (;;) {
         StrTrimHead(value);
         if (value.empty())
            break;
         do {
            int n = StrTo(value, -1);
            if (n < 0)
               return value.begin();
            this->CpuAffinity_.push_back(static_cast<uint32_t>(n));
            StrTrimHead(value);
            if (value.empty())
               return nullptr;
         } while (isdigit(static_cast<unsigned char>(value.Get1st())));
         value.SetBegin(value.begin() + 1);
      }
   }
   else
      return tag.begin();
   return nullptr;
}

const char* IoServiceArgs::Parse(StrView values) {
   StrView tag, value;
   while (FetchTagValue(values, tag, value)) {
      if (const char* perr = this->FromTagValue(tag, value))
         return perr;
   }
   return nullptr;
}

//--------------------------------------------------------------------------//

void ServiceThreadArgs::OnThrRunBegin(std::thread& thr, StrView msgHead) const {
   Result3<ErrC>   cpuAffinityResult;
   if (this->CpuAffinity_ >= 0) {
      #if defined(fon9_WINDOWS)
      if (SetThreadAffinityMask(thr.native_handle(), (static_cast<DWORD_PTR>(1) << this->CpuAffinity_)) == 0) {
         cpuAffinityResult = GetSysErrC();
      #else
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(this->CpuAffinity_, &cpuset);
      if (int iErr = pthread_setaffinity_np(thr.native_handle(), sizeof(cpu_set_t), &cpuset)) {
         cpuAffinityResult = GetSysErrC(iErr);
      #endif
      }
      else
         cpuAffinityResult.SetResultOK();
   }
   fon9_LOG_ThrRun(msgHead, ".ThrRun|name=", this->Name_,
                    "|index=", this->ThreadPoolIndex_ + 1,
                    "|Cpu=", this->CpuAffinity_, cpuAffinityResult,
                    "|Wait=", HowWaitToStr(this->HowWait_),
                    "|Capacity=", this->Capacity_);

}

} } // namespaces
