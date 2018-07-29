// \file fon9/framework/Framework.cpp
// \author fonwinz@gmail.com
#include "fon9/framework/Framework.hpp"
#include "fon9/InnSyncerFile.hpp"
#include "fon9/FilePath.hpp"

namespace fon9 {

Framework::~Framework() {
   this->Dispose();
}

void Framework::Initialize(int argc, char** argv) {
   (void)argc; (void)argv;
   // TODO: 從 env, args... 取出 ConfigPath_;
   // - 從 ConfigPath/fon9common.ini 取出:
   //   - memlock=Y/N
   //   - LogFile=./logs/{0:f+TW}/fon9sys-{1:04}.log  # 超過 {0:f+8}=YYYYMMDD(台灣時間), {1:04}=檔案序號.
   //   - LogFileSizeMB=N                             # 超過 N MB 就換檔.
   //   - Syncer=InnSyncerFile: syncOutFileName, syncInFileName, 100ms
   //   - MaAuthDbfName=MaAuth                        # 用在 Syncer 時尋找 Handler 時使用, 一個 Syncer 可以有多個 Handler.
   // - 從 ConfigPath/fon9local.ini 取出(在 fon9common.ini 使用 $include fon9local.ini 即可):
   //   - HostId=
   this->ConfigPath_ = FilePath::AppendPathTail(&this->ConfigPath_);
   this->SyncerPath_ = FilePath::AppendPathTail(&this->SyncerPath_);
   this->Root_.reset(new seed::MaTree{"Service"});
   this->Syncer_.reset(new InnSyncerFile(InnSyncerFile::CreateArgs(
      this->ConfigPath_ + "SyncOut.f9syn",
      this->ConfigPath_ + "SyncIn.f9syn",
      TimeInterval_Second(1)
   )));

   StrView  maAuthDbfName{"MaAuth"};
   InnDbfSP maAuthStorage{new InnDbf(maAuthDbfName, this->Syncer_)};
   maAuthStorage->Open(this->ConfigPath_ + maAuthDbfName.ToString() + ".f9dbf");
   this->MaAuth_ = auth::AuthMgr::Plant(this->Root_, maAuthStorage, maAuthDbfName.ToString());
}

void Framework::Start() {
   this->MaAuth_->Storage_->LoadAll();
   this->Syncer_->StartSync();
}

void Framework::Dispose() {
   this->Syncer_->StopSync();
   this->MaAuth_->Storage_->Close();
   this->Root_->OnParentSeedClear();
}

} // namespaces
