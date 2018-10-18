// \file fon9/DllHandle.cpp
// \author fonwinz@gmail.com
#include "fon9/DllHandle.hpp"
#include "fon9/FilePath.hpp"
#include "fon9/ErrC.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 {

#ifdef fon9_POSIX
fon9_API HINSTANCE LoadLibrary(const char* path) {
   if (HINSTANCE h = dlopen(path, RTLD_NOW))
      return h;
   std::string pso{path};
   if (pso.size() > 3 && memcmp(&*pso.end() - 3, ".so", 3) == 0)
      return nullptr;
   pso.append(".so");
   return dlopen(pso.c_str(), RTLD_NOW);
}
#endif

std::string DllHandle::Open(const char* path) {
   this->Close();
   if ((this->Instance_ = LoadLibrary(path)) != nullptr)
      return std::string{};
#ifdef fon9_WINDOWS
   return RevPrintTo<std::string>(GetSysErrC());
#else
   std::string err = dlerror();
   if (!FilePath::HasPrefixPath(StrView_cstr(path))) {
      // Linux 不會自動搜尋現在路徑 (在Windows則會).
      // 從現在路徑動態載入 so, 還蠻常用到的, 所以這裡協助處理此事.
      std::string cpath("./");
      cpath.append(path);
      if ((this->Instance_ = LoadLibrary(cpath.c_str())) != nullptr)
         return std::string{};
      err.push_back('\n');
      err += dlerror();
   }
   return err;
#endif
}
std::string DllHandleSym::Open(const char* path, const char* symName) {
   this->Sym_ = nullptr;
   std::string res = this->Dll_.Open(path);
   if (!res.empty())
      return res;
   if ((this->Sym_ = this->Dll_.GetSym(symName)) != nullptr)
      return std::string{};

#ifdef fon9_WINDOWS
   res = RevPrintTo<std::string>(GetSysErrC());
#else
   res = dlerror();
#endif
   this->Dll_.Close();
   return res;
}

} // namespaces
