/// \file fon9/seed/SysEnv.cpp
/// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS  // Windows: getenv()
#include "fon9/seed/SysEnv.hpp"
#include "fon9/seed/FieldMaker.hpp"

#ifdef fon9_WINDOWS
#include <direct.h>  // _getcwd();
#else
#include <unistd.h>
#define _getcwd   getcwd
static inline pid_t GetCurrentProcessId() {
   return getpid();
}
#endif

namespace fon9 { namespace seed {

LayoutSP SysEnv::MakeDefaultLayout() {
   Fields fields = NamedSeed::MakeDefaultFields();
   fields.Add(fon9_MakeField(Named{"Value"}, SysEnvItem, Value_));
   return new Layout1(fon9_MakeField(Named{"Name"}, SysEnvItem, Name_),
                      new Tab{Named{"SysEnv"}, std::move(fields)});
}
SysEnvItemSP SysEnv::Add(int argc, char** argv, const CmdArgDef& def) {
   return this->Add(new SysEnvItem(def, GetCmdArg(argc, argv, def).ToString()));
}
void SysEnv::Initialize(int argc, char** argv) {
   std::string cmdstr{argv[0]};
   for (int L = 1; L < argc; ++L) {
      cmdstr.push_back(' ');
      StrView v{StrView_cstr(argv[L])};
      char    chQuot = (v.Find(' ') || v.Find('\t')) ? '`' : '\0';
      if (chQuot)
         cmdstr.push_back(chQuot);
      v.AppendTo(cmdstr);
      if (chQuot)
         cmdstr.push_back(chQuot);
   }
   fon9_GCC_WARN_DISABLE("-Wmissing-field-initializers");
   this->Add(0, nullptr, CmdArgDef{StrView{fon9_kCSTR_SysEnvItem_CommandLine}, &cmdstr});

   char        msgbuf[1024 * 64];
   CmdArgDef   cwd{StrView{fon9_kCSTR_SysEnvItem_ExecPath}};
   if (_getcwd(msgbuf, sizeof(msgbuf)))
      cwd.DefaultValue_ = StrView_cstr(msgbuf);
   else { // getcwd()失敗, 應使用 "./"; 然後在 Description 設定錯誤訊息.
      cmdstr = std::string{"getcwd():"} + strerror(errno);
      cwd.Description_ = &cmdstr;
      cwd.DefaultValue_ = StrView{"./"};
   }
   this->Add(argc, argv, cwd);
   fon9_GCC_WARN_POP;

   RevBufferFixedSize<128> rbuf;
   RevPrint(rbuf, GetCurrentProcessId());
   this->Add(new SysEnvItem{fon9_kCSTR_SysEnvItem_ProcessId, rbuf.ToStrT<std::string>()});
}

} } // namespaces
