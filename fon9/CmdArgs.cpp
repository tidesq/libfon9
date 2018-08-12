/// \file fon9/CmdArgs.cpp
/// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS  // Windows: getenv()
#include "fon9/CmdArgs.hpp"

namespace fon9 {

StrView GetCmdArg(int argc, char** argv, const StrView& argShort, const StrView& argLong) {
   if (argc > 1 && argv != nullptr && (!argShort.empty() || !argLong.empty())) {
      for (int L = argc; L > 0;) {
         const char*    vstr = argv[--L];
         const StrView* argName;
         switch (vstr[0]) {
         default:
            continue;
         case '-':
            if (vstr[1] == '-') { // --argLong
               argName = &argLong;
               vstr += 2;
               break;
            }
            // 不用 break; 使用 -argShort
         case '/':
            argName = &argShort;
            ++vstr;
            break;
         }
         if (argName->empty())
            continue;
         StrView     opt = StrView_cstr(vstr);
         const char* pvalue = opt.Find('=');
         vstr = opt.end();
         if (pvalue)
            opt = StrView{opt.begin(), pvalue};
         if (opt == *argName)
            return pvalue ? StrView{pvalue + 1, vstr}
                          : (L + 1 >= argc ? StrView{""} : StrView_cstr(argv[L + 1]));
      }
   }
   return StrView{nullptr};
}
StrView GetCmdArg(int argc, char** argv, const CmdArgDef& def) {
   StrView retval = GetCmdArg(argc, argv, def.ArgShort_, def.ArgLong_);
   if (!retval.empty())
      return retval;
   if (def.EnvName_ && *def.EnvName_)
      if (const char* envstr = getenv(def.EnvName_))
         return StrView_cstr(envstr);
   return def.DefaultValue_;
}

} // namespaces
