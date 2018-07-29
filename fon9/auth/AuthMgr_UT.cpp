// \file fon9/AuthMgr_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/TestTools.hpp"
#include "fon9/framework/Framework.hpp"
#include "fon9/seed/SeedSearcher.hpp"
#include "fon9/auth/RoleMgr.hpp"
#include "fon9/InnSyncerFile.hpp"
#include "fon9/Timer.hpp"
#include "fon9/DefaultThreadPool.hpp"

//--------------------------------------------------------------------------//

static const char                kInnSyncInFileName[] = "AuthSynIn.f9syn";
static const char                kInnSyncOutFileName[] = "AuthSynOut.f9syn";
static const char                kDbfFileName[] = "AuthDbf.f9dbf";
static const fon9::TimeInterval  kSyncInterval{fon9::TimeInterval_Millisecond(10)};

void RemoveTestFiles() {
   remove(kInnSyncInFileName);
   remove(kInnSyncOutFileName);
   remove(kDbfFileName);
}

void TestInitStart(fon9::Framework& fon9sys, std::string innSyncOutFileName, std::string innSyncInFileName) {
   // fon9sys.Initialize(argc, argv);
   fon9sys.Root_.reset(new fon9::seed::MaTree{"Service"});
   fon9sys.Syncer_.reset(new fon9::InnSyncerFile(fon9::InnSyncerFile::CreateArgs(
      std::move(innSyncOutFileName), std::move(innSyncInFileName), kSyncInterval
   )));
   fon9::StrView  maAuthDbfName{"MaAuth"};
   fon9::InnDbfSP maAuthStorage{new fon9::InnDbf(maAuthDbfName, fon9sys.Syncer_)};
   maAuthStorage->Open(kDbfFileName);
   fon9sys.MaAuth_ = fon9::auth::AuthMgr::Plant(fon9sys.Root_, maAuthStorage, maAuthDbfName.ToString());
   fon9sys.Start();
}

//--------------------------------------------------------------------------//

struct RoleConfig {
   const char* PolicyName_;
   const char* PolicyId_;
};
using RoleConfigs = std::vector<RoleConfig>;
struct RoleItem {
   const char* RoleId_;
   const char* Description_;
   RoleConfigs Configs_;
};
// 使用 seed 機制找到 /MaAuth/RoleMgr.
// /MaAuth/RoleMgr
//          +- admin
//          |  +- PoAcl=full
//          |  \- PoOrder=view
//          +- trader
//          +- trader01
//          |  \- PoOrders=trader
//          \- dealer01
//             \- PoOrder=dealer
static const RoleItem RoleItems[] = {
   {"admin",    "Administrator", {{"PoAcl", "full"},{"PoOrder", "view"}}},
   {"trader",   nullptr,         {}},
   {"trader01", "TraderRoom01",  {{"PoOrder", "trader"}}},
   {"dealer01", "DealerRoom01",  {{"PoOrder", "dealer"}}},
};
#define kSPL    "\t"
#define kROWSPL "\n"
static const char kGridViewRoleMgr[] =
"admin"    kSPL "Administrator" kROWSPL
"dealer01" kSPL "DealerRoom01"  kROWSPL
"trader"   kSPL                 kROWSPL
"trader01" kSPL "TraderRoom01";

static const char kGridViewRoleMgr_admin[] =
"PoAcl"   kSPL "full" kROWSPL
"PoOrder" kSPL "view";

static const char kGridViewRoleMgr_trader[] = "";
static const char kGridViewRoleMgr_trader01[] = "PoOrder" kSPL "trader";
static const char kGridViewRoleMgr_dealer01[] = "PoOrder" kSPL "dealer";

struct SetRole : public fon9::seed::SeedSearcher {
   fon9_NON_COPY_NON_MOVE(SetRole);
   using base = fon9::seed::SeedSearcher;
   SetRole() : base{"/MaAuth/RoleMgr"} {
   }
   void OnError(fon9::seed::OpResult res) override {
      std::cout
         << fon9::RevPrintTo<std::string>("SetRole.OnError|path=", this->OrigPath_, "|remain=", this->RemainPath_,
                                          "|res=", res, ':', fon9::seed::GetOpResultMessage(res))
         << "\r[ERROR]" << std::endl;
      abort();
   }
   void OnFoundTree(fon9::seed::TreeOp& opTree) override {
      for (auto& r : RoleItems) {
         this->CurRoleItem_ = &r;
         opTree.Add(fon9::ToStrView(r.RoleId_), std::bind(&SetRole::SetRoleItem, this, std::placeholders::_1, std::placeholders::_2));
      }
   }
   const RoleItem* CurRoleItem_;
   void SetRoleItem(const fon9::seed::PodOpResult& res, fon9::seed::PodOp* opPod) {
      if (opPod == nullptr) {
         std::cout
            << fon9::RevPrintTo<std::string>("SetRole.Add|roleId=", this->CurRoleItem_->RoleId_,
                                             "|res=", res.OpResult_, ':', fon9::seed::GetOpResultMessage(res.OpResult_))
            << "\r[ERROR]" << std::endl;
         abort();
      }
      fon9::seed::Tab* tab = res.Sender_->LayoutSP_->GetTab(0);
      opPod->BeginWrite(*tab, [this](const fon9::seed::SeedOpResult& resWr, const fon9::seed::RawWr* wr) {
         resWr.Tab_->Fields_.Get("Description")->StrToCell(*wr, fon9::ToStrView(this->CurRoleItem_->Description_));
      });
      fon9::seed::TreeSP cfgs = opPod->MakeSapling(*tab);
      cfgs->OnTreeOp(std::bind(&SetRole::SetRoleConfigs, this, std::placeholders::_1, std::placeholders::_2));
   }
   void SetRoleConfigs(const fon9::seed::TreeOpResult& res, fon9::seed::TreeOp* opTree) {
      if (opTree == nullptr) {
         std::cout
            << fon9::RevPrintTo<std::string>("SetRole.MakeSapling|roleId=", this->CurRoleItem_->RoleId_,
                                             "|res=", res.OpResult_, ':', fon9::seed::GetOpResultMessage(res.OpResult_))
            << "\r[ERROR]" << std::endl;
         abort();
      }
      for (auto& item : this->CurRoleItem_->Configs_) {
         opTree->Add(fon9::ToStrView(item.PolicyName_),
                     [&item](const fon9::seed::PodOpResult& resPod, fon9::seed::PodOp* opPod) {
            fon9::seed::Tab* tab = resPod.Sender_->LayoutSP_->GetTab(0);
            opPod->BeginWrite(*tab, [&item](const fon9::seed::SeedOpResult& resWr, const fon9::seed::RawWr* wr) {
               resWr.Tab_->Fields_.Get("PolicyId")->StrToCell(*wr, fon9::ToStrView(item.PolicyId_));
            });
         });
      }
   }
};

//--------------------------------------------------------------------------//

void CheckGridView(fon9::seed::Tree& root, const char* path, const char* gvExpect) {
   std::cout << "[TEST ] GridView|path=" << path;
   fon9::seed::GetGridView(root, fon9::ToStrView(path),
                           fon9::seed::GridViewRequest{fon9::seed::TreeOp::TextBegin()}, 0,
                           [gvExpect](fon9::seed::GridViewResult& res) {
      if (res.GridView_ != gvExpect) {
         std::cout << "|gv=" << "\r[ERROR]\n" << res.GridView_ << std::endl;
         abort();
      }
   });
   std::cout << "\r[OK   ]" << std::endl;
}
void CheckInitGridViewAll(fon9::seed::Tree& root) {
   CheckGridView(root, "/MaAuth/RoleMgr",          kGridViewRoleMgr);
   CheckGridView(root, "/MaAuth/RoleMgr/admin",    kGridViewRoleMgr_admin);
   CheckGridView(root, "/MaAuth/RoleMgr/trader",   kGridViewRoleMgr_trader);
   CheckGridView(root, "/MaAuth/RoleMgr/trader01", kGridViewRoleMgr_trader01);
   CheckGridView(root, "/MaAuth/RoleMgr/dealer01", kGridViewRoleMgr_dealer01);
}

void TestInit() {
   RemoveTestFiles();
   fon9::Framework fon9sys;
   TestInitStart(fon9sys, kInnSyncOutFileName, kInnSyncInFileName);

   std::cout << "[TEST ] SetRole";
   StartSeedSearch(*fon9sys.Root_, new SetRole);
   std::cout << "\r[OK   ]" << std::endl;

   CheckInitGridViewAll(*fon9sys.Root_);
   fon9sys.Dispose();
}
void TestReload(void(*fnCheckGridView)(fon9::seed::Tree& root)) {
   std::cout << "After reload:\n";
   fon9::Framework fon9sys;
   TestInitStart(fon9sys, kInnSyncOutFileName, kInnSyncInFileName);
   fnCheckGridView(*fon9sys.Root_);
}
void TestSync(void(*fnCheckGridView)(fon9::seed::Tree& root)) {
   remove(kDbfFileName);
   std::cout << "After sync:\n";
   fon9::Framework fon9sys;
   TestInitStart(fon9sys, kInnSyncInFileName, kInnSyncOutFileName); // syncIn, syncOut 反向.
   std::this_thread::sleep_for(fon9::TimeInterval(kSyncInterval * 10).ToDuration());
   fnCheckGridView(*fon9sys.Root_);
}

//--------------------------------------------------------------------------//

static const char kRemovedGridViewRoleMgr[] =
"admin"    kSPL "Administrator" kROWSPL
"trader01" kSPL "TraderRoom01";
static const char kRemovedGridViewRoleMgr_admin[]    = "PoAcl"   kSPL "full";
static const char kRemovedGridViewRoleMgr_trader[]   = "";
static const char kRemovedGridViewRoleMgr_trader01[] = "PoOrder" kSPL "trader";
static const char kRemovedGridViewRoleMgr_dealer01[] = "";

void CheckRemovedGridViewAll(fon9::seed::Tree& root) {
   CheckGridView(root, "/MaAuth/RoleMgr",          kRemovedGridViewRoleMgr);
   CheckGridView(root, "/MaAuth/RoleMgr/admin",    kRemovedGridViewRoleMgr_admin);
   CheckGridView(root, "/MaAuth/RoleMgr/trader",   kRemovedGridViewRoleMgr_trader);
   CheckGridView(root, "/MaAuth/RoleMgr/trader01", kRemovedGridViewRoleMgr_trader01);
   CheckGridView(root, "/MaAuth/RoleMgr/dealer01", kRemovedGridViewRoleMgr_dealer01);
}

void TestRemove() {
   std::cout << "Removed:\n";
   fon9::Framework fon9sys;
   TestInitStart(fon9sys, kInnSyncOutFileName, kInnSyncInFileName);
   auto nullRemoveHandler = [](const fon9::seed::PodRemoveResult& res) {
      if (res.OpResult_ < fon9::seed::OpResult::no_error) {
         std::cout
            << fon9::RevPrintTo<std::string>(
                        "TestRemove|pos=", res.KeyText_.ToString(),
                        "|res=", res.OpResult_, ':', fon9::seed::GetOpResultMessage(res.OpResult_))
            << std::endl;
      }
   };
   fon9::seed::RemoveSeed(*fon9sys.Root_, "/MaAuth/RoleMgr/admin/PoOrder", nullRemoveHandler);
   fon9::seed::RemoveSeed(*fon9sys.Root_, "/MaAuth/RoleMgr/trader",        nullRemoveHandler);
   fon9::seed::RemoveSeed(*fon9sys.Root_, "/MaAuth/RoleMgr/dealer01",      nullRemoveHandler);
   CheckRemovedGridViewAll(*fon9sys.Root_);
}

//--------------------------------------------------------------------------//

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   //_CrtSetBreakAlloc(176);
#endif

   fon9::AutoPrintTestInfo utinfo{"AuthMgr"};
   fon9::GetDefaultTimerThread();
   fon9::GetDefaultThreadPool();
   std::this_thread::sleep_for(std::chrono::milliseconds{10});
   //--------------------------------------------------------------------------//

   TestInit();
   TestReload(&CheckInitGridViewAll);
   TestSync(&CheckInitGridViewAll);

   TestRemove();
   TestReload(&CheckRemovedGridViewAll);
   TestSync(&CheckRemovedGridViewAll);

   //--------------------------------------------------------------------------//
   bool isKeepTestFiles = false;
   for (int L = 1; L < argc; ++L) {
      if (strcmp(argv[L], "--keep") == 0)
         isKeepTestFiles = true;
   }
   if (!isKeepTestFiles)
      RemoveTestFiles();
}
