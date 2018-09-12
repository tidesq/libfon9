// \file fon9/framework/Framework.cpp
// \author fonwinz@gmail.com
#include "fon9/framework/Framework.hpp"
#include "fon9/seed/SysEnv.hpp"
#include "fon9/ConfigLoader.hpp"
#include "fon9/InnSyncerFile.hpp"
#include "fon9/FilePath.hpp"
#include "fon9/LogFile.hpp"
#include "fon9/Log.hpp"
#include "fon9/HostId.hpp"
#include "fon9/DefaultThreadPool.hpp"
#include "fon9/CountDownLatch.hpp"

#if !defined(fon9_WINDOWS)
#include <sys/mman.h> // mlockall()
bool SetCurrentDirectory(const char* path) {
   return chdir(path) == 0;
}
#endif

namespace fon9 {

Framework::~Framework() {
   this->Dispose();
}

static void RaiseInitializeError(std::string err) {
   fon9_LOG_FATAL(err);
   Raise<std::runtime_error>(err);
}
static void LogSysEnv(seed::SysEnvItemSP item) {
   if (item)
      fon9_LOG_IMP("SysEnv|", item->Name_, "=", item->Value_);
}

void Framework::Initialize(int argc, char** argv) {
   auto workDir = GetCmdArg(argc, argv, CmdArgDef{
      StrView{"WorkDir"}, //Name
      StrView{},          //DefaultValue
      StrView{"w"},       //ArgShort_
      StrView{"workdir"}, //ArgLong_
      nullptr,            //EnvName_
      StrView{},          //Title_
      StrView{}});        //Description_
   if (!workDir.empty()) {
      if (!SetCurrentDirectory(workDir.ToString().c_str()))
         // 如果路徑設定失敗, 表示現在的執行環境不正確: 不可執行程式.
         RaiseInitializeError(RevPrintTo<std::string>("SetCurrentDirectory(\"", workDir, "\")|err=", GetSysErrC()));
   }

   this->Root_.reset(new seed::MaTree{"Services"});

   auto sysEnv = seed::SysEnv::Plant(this->Root_);
   auto cfgPath = sysEnv->Add(argc, argv, CmdArgDef{
      StrView{fon9_kCSTR_SysEnvItem_ConfigPath}, //Name
      StrView{"fon9cfg"}, //DefaultValue
      StrView{"c"},       //ArgShort_
      StrView{"cfg"},     //ArgLong_
      "fon9cfg",          //EnvName_
      StrView{},          //Title_
      StrView{}});        //Description_
   this->ConfigPath_ = FilePath::AppendPathTail(&cfgPath->Value_);
   sysEnv->Initialize(argc, argv);

   #define fon9_kCSTR_SyncerPath    "SyncerPath"
   #define fon9_kCSTR_MaAuthName    "MaAuthName"
   ConfigLoader cfgld{this->ConfigPath_};
   cfgld.IncludeConfig("$" fon9_kCSTR_SyncerPath "=fon9syn\n"
                       "$" fon9_kCSTR_MaAuthName "=MaAuth\n"
                       "$include:fon9local.cfg");
   cfgld.IncludeConfig("$include:fon9common.cfg");

   StrView        cfgstr;
   RevBufferList  rbuf{128};

   // 如果沒設定, log 就輸出在 console.
   if (auto logFileFmt = cfgld.GetVariable("LogFileFmt")) {
      cfgstr = &logFileFmt->Value_.Str_;
      if (!StrTrim(&cfgstr).empty()) {
         File::SizeType maxFileSizeMB = 0;
         if (auto logFileSize = cfgld.GetVariable("LogFileSizeMB"))
            maxFileSizeMB = StrTo(&logFileSize->Value_.Str_, 0u);
         std::string fname = StrView_ToNormalizeStr(cfgstr);
         auto        res = InitLogWriteToFile(fname, FileRotate::TimeScale::Day, maxFileSizeMB * 1024 * 1024, 0);
         if (res.IsError())
            RevPrint(rbuf, "err=", res.GetError());
         else if (maxFileSizeMB > 0)
            RevPrint(rbuf, "MaxFileSizeMB=", maxFileSizeMB);
         sysEnv->Add(new seed::SysEnvItem("LogFileFmt", std::move(fname), std::string{}, BufferTo<std::string>(rbuf.MoveOut())));
      }
   }

   #define fon9_kCSTR_HostId   "HostId"
   if (auto hostId = cfgld.GetVariable(fon9_kCSTR_HostId)) {
      LocalHostId_ = StrTo(&hostId->Value_.Str_, 0u);
      if (LocalHostId_ == 0)
         RaiseInitializeError(RevPrintTo<std::string>("$" fon9_kCSTR_HostId "=", hostId->Value_.Str_,
                                                      "|err=Local $HostId cannot be zero"
                                                      "|from=", hostId->From_));
      sysEnv->Add(new seed::SysEnvItem(fon9_kCSTR_HostId, RevPrintTo<std::string>(LocalHostId_), "Local HostId"));
   }

   // Syncer=InnSyncerFile: syncOutFileName, syncInFileName, 100ms
   // TODO: 設定 Syncer 用哪種機制及參數. (不一定要使用 InnSyncerFile)
   cfgstr = &cfgld.GetVariable(fon9_kCSTR_SyncerPath)->Value_.Str_;
   this->SyncerPath_ = FilePath::AppendPathTail(StrTrim(&cfgstr));
   sysEnv->Add(new seed::SysEnvItem{fon9_kCSTR_SyncerPath, this->SyncerPath_});
   this->Syncer_.reset(new InnSyncerFile(InnSyncerFile::CreateArgs(
      this->SyncerPath_ + "SyncOut.f9syn",
      this->SyncerPath_ + "SyncIn.f9syn",
      TimeInterval_Second(1)
   )));

   // MaAuthName 用在:
   // (1) Syncer 時尋找 Handler 時使用(一個 Syncer 可以有多個 Handler).
   // (2) 從 Root 尋找使用: "/MaAuthName"
   // (3) ConfigPath_/MaAuthName.f9dbf
   cfgstr = &cfgld.GetVariable(fon9_kCSTR_MaAuthName)->Value_.Str_;
   InnDbfSP maAuthStorage{new InnDbf(StrTrim(&cfgstr), this->Syncer_)};
   maAuthStorage->Open(this->ConfigPath_ + cfgstr.ToString() + ".f9dbf");
   this->MaAuth_ = auth::AuthMgr::Plant(this->Root_, maAuthStorage, cfgstr.ToString());

   #define fon9_kCSTR_MemLock       "MemLock"
   if (auto varMemLock = cfgld.GetVariable(fon9_kCSTR_MemLock)) {
      cfgstr = &varMemLock->Value_.Str_;
      if (toupper(static_cast<unsigned char>(StrTrimHead(&cfgstr).Get1st())) == 'Y') {
         // MemLock 開啟 mlockall(MCL_CURRENT|MCL_FUTURE), 不支援 Windows.
         #define MLOCK()       mlockall(MCL_CURRENT|MCL_FUTURE)
         #define MLOCK_cstr   "mlockall(MCL_CURRENT|MCL_FUTURE)"
         #define MLOCK_log    "$" fon9_kCSTR_MemLock ":" MLOCK_cstr "|"
         std::string desc;
         #if !defined(fon9_WINDOWS)
            if (MLOCK() == 0) {
               desc = "OK";
               fon9_LOG_INFO(MLOCK_log "info=OK!");
            }
            else {
               desc = RevPrintTo<std::string>("err=", GetSysErrC());
               fon9_LOG_ERROR(MLOCK_log, desc);
            }
         #else
            desc = "err=Not support on Windows";
            fon9_LOG_WARN(MLOCK_log, desc);
         #endif
         sysEnv->Add(new seed::SysEnvItem(fon9_kCSTR_MemLock, "Y", MLOCK_cstr, std::move(desc)));
      }
   }

   // 透過 log 紀錄基本的執行環境.
   LogSysEnv(sysEnv->Get(fon9_kCSTR_SysEnvItem_ProcessId));
   LogSysEnv(sysEnv->Get(fon9_kCSTR_SysEnvItem_CommandLine));
   LogSysEnv(sysEnv->Get(fon9_kCSTR_SysEnvItem_ExecPath));
   LogSysEnv(sysEnv->Get(fon9_kCSTR_SysEnvItem_ConfigPath));
   LogSysEnv(sysEnv->Get(fon9_kCSTR_HostId));
}

void Framework::Start() {
   GetDefaultThreadPool();
   GetDefaultTimerThread();
   this->MaAuth_->Storage_->LoadAll();
   this->Syncer_->StartSync();
}

void Framework::Dispose() {
   this->Syncer_->StopSync();
   this->MaAuth_->Storage_->Close();
   this->Root_->OnParentSeedClear();
}

void Framework::DisposeForAppQuit() {
   this->Dispose();

   // 讓等候中的工作有機會全部做完.
   // 等候每個 DefaultThreadPool 的每個 thread 執行 n 次,
   for (unsigned count = 3; count > 0; --count) {
      unsigned       L = static_cast<unsigned>(GetDefaultThreadPool().GetThreadCount());
      CountDownLatch waiter{L};
      for (; L > 0; --L) {
         GetDefaultThreadPool().EmplaceMessage([&waiter]() {
            // TODO: 如何確定每次執行都是在不同的 thread?
            waiter.CountDown();
         });
      }
      waiter.Wait();
      // 有些 thread 會花比較久的時間結束(e.g. IoService),
      // 要如何等候這些 thread 呢? 用 global CountDownLatch 嗎?
      std::this_thread::sleep_for(std::chrono::milliseconds{50});
   }

   // 結束 DefaultThreadPool, 不會再處理後續加入的工作!!
   GetDefaultThreadPool().WaitForEndAfterWorkDone();
}

} // namespaces
