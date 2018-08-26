/// \file fon9/seed/SysEnv.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_SysEnv_hpp__
#define __fon9_seed_SysEnv_hpp__
#include "fon9/seed/MaTree.hpp"
#include "fon9/CmdArgs.hpp"

namespace fon9 { namespace seed {

struct SysEnvItem : public NamedSeed {
   fon9_NON_COPY_NON_MOVE(SysEnvItem);
   using base = NamedSeed;
public:
   const std::string Value_;

   SysEnvItem(const CmdArgDef& def, std::string value) : base{def.Name_.ToString()}, Value_{std::move(value)} {
      this->SetTitle(def.Title_.ToString());
      this->SetDescription(def.Description_.ToString());
   }
   SysEnvItem(std::string name, std::string value, std::string title = std::string{}, std::string description = std::string{})
      : base(std::move(name), std::move(title), std::move(description))
      , Value_{std::move(value)} {
   }
};
using SysEnvItemSP = NamedSeedSPT<SysEnvItem>;

class fon9_API SysEnv;
using SysEnvSP = NamedSeedSPT<SysEnv>;
/// \ingroup seed
/// 管理啟動時就已決定, 且執行過程中不會再變動的資料.
class fon9_API SysEnv : public NamedSapling {
   fon9_NON_COPY_NON_MOVE(SysEnv);
   using base = NamedSapling;
   using SysEnvTree = MaTree;
   static LayoutSP MakeDefaultLayout();

public:
   SysEnv(const MaTree* /*maTree*/, std::string name)
      : base(new SysEnvTree{MakeDefaultLayout()}, std::move(name)) {
   }

   #define fon9_kCSTR_SysEnv_DefaultName  "SysEnv"
   /// 在 maTree 上面種一個 SysEnv.
   /// \retval nullptr   seedName已存在.
   /// \retval !=nullptr 種到 maTree 的 SysEnv 物件.
   static SysEnvSP Plant(const MaTreeSP& maTree, std::string seedName = fon9_kCSTR_SysEnv_DefaultName) {
      return maTree->Plant<SysEnv>("SysEnv.Plant", std::move(seedName));
   }

   /// \ref fon9::GetCmdArg()
   /// \retval nullptr  def.Name_ 已存在.
   SysEnvItemSP Add(int argc, char** argv, const CmdArgDef& def);
   SysEnvItemSP Add(SysEnvItemSP item) {
      if (static_cast<SysEnvTree*>(this->Sapling_.get())->Add(item))
         return item;
      return SysEnvItemSP{};
   }

   SysEnvItemSP Get(const StrView& name) const {
      return static_cast<SysEnvTree*>(this->Sapling_.get())->Get<SysEnvItem>(name);
   }

   #define fon9_kCSTR_SysEnvItem_ConfigPath   "ConfigPath"    // config 檔案的預設路徑.
   #define fon9_kCSTR_SysEnvItem_CommandLine  "CommandLine"   // Initialize() 時的參數: argv[0] + " " + argv[1]...
   #define fon9_kCSTR_SysEnvItem_ExecPath     "ExecPath"      // Initialize() 時的路徑.
   #define fon9_kCSTR_SysEnvItem_ProcessId    "ProcessId"
   /// 加入:
   /// - fon9_kCSTR_SysEnvItem_CommandLine: 程式啟動時的參數: argv[0] + " " + argv[1]...
   /// - fon9_kCSTR_SysEnvItem_ExecPath:    getcwd()
   void Initialize(int argc, char** argv);
};

} } // namespaces
#endif//__fon9_seed_SysEnv_hpp__
