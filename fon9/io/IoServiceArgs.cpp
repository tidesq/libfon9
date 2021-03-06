﻿/// \file fon9/io/IoServiceArgs.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/IoServiceArgs.hpp"
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

ConfigParser::Result IoServiceArgs::OnTagValue(StrView tag, StrView& value) {
   const char* pvalbeg = value.begin();
   if (tag == "ThreadCount") {
      if ((this->ThreadCount_ = StrTo(value, 0u)) <= 0) {
         this->ThreadCount_ = 1;
         value.SetBegin(pvalbeg);
         return ConfigParser::Result::EValueTooSmall;
      }
   }
   else if (tag == "Capacity")
      this->Capacity_ = StrTo(value, 0u);
   else if (tag == "Wait") {
      if ((this->HowWait_ = StrToHowWait(value)) == HowWait::Unknown) {
         this->HowWait_ = HowWait::Block;
         return ConfigParser::Result::EInvalidValue;
      }
   }
   else if (tag == "Cpus") {
      while (!value.empty()) {
         StrView v1 = StrFetchTrim(value, ',');
         if (v1.empty())
            continue;
         const char* pend;
         int n = StrTo(v1, -1, &pend);
         if (n < 0) {
            value.SetBegin(v1.begin());
            return ConfigParser::Result::EInvalidValue;
         }
         if (pend != v1.end()) {
            value.SetBegin(pend);
            return ConfigParser::Result::EInvalidValue;
         }
         this->CpuAffinity_.push_back(static_cast<uint32_t>(n));
      }
   }
   else
      return ConfigParser::Result::EUnknownTag;
   return ConfigParser::Result::Success;
}

//--------------------------------------------------------------------------//

void ServiceThreadArgs::OnThrRunBegin(StrView msgHead) const {
   Result3  cpuAffinityResult{Result3::kNoResult()};
   if (this->CpuAffinity_ >= 0) {
      #if defined(fon9_WINDOWS)
      if (SetThreadAffinityMask(GetCurrentThread(), (static_cast<DWORD_PTR>(1) << this->CpuAffinity_)) == 0) {
         cpuAffinityResult = GetSysErrC();
      #else
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(this->CpuAffinity_, &cpuset);
      if (int iErr = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset)) {
         cpuAffinityResult = GetSysErrC(iErr);
      #endif
      }
      else
         cpuAffinityResult.SetSuccess();
   }
   fon9_LOG_ThrRun(msgHead, ".ThrRun|name=", this->Name_,
                   "|index=", this->ThreadPoolIndex_ + 1,
                   "|Cpu=", this->CpuAffinity_, ':', cpuAffinityResult,
                   "|Wait=", HowWaitToStr(this->HowWait_),
                   "|Capacity=", this->Capacity_);

}

} } // namespaces
