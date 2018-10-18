/// \file fon9/DllHandle.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_DllHandle_hpp__
#define __fon9_DllHandle_hpp__
#include "fon9/sys/Config.hpp"
#include <string>
#include <vector>

#ifdef fon9_POSIX
#include <dlfcn.h>
#endif

namespace fon9 {

#ifdef fon9_POSIX
using HINSTANCE = void*;
fon9_API HINSTANCE LoadLibrary(const char* path);
inline void FreeLibrary(HINSTANCE handle) {
   dlclose(handle);
}
inline void* GetProcAddress(HINSTANCE h, const char* sym) {
   return dlsym(h, sym);
}
#endif

/// \ingroup Misc
/// DLL or so 的載入 & 解構時自動卸載.
class fon9_API DllHandle {
   fon9_NON_COPYABLE(DllHandle);
   HINSTANCE   Instance_{};
public:
   ~DllHandle() {
      this->Close();
   }
   DllHandle() = default;
   DllHandle(DllHandle&& rhs) : Instance_(rhs.Instance_) {
      rhs.Instance_ = HINSTANCE{};
   }
   DllHandle& operator=(DllHandle&& rhs) {
      this->Close();
      this->Instance_ = rhs.Instance_;
      rhs.Instance_ = HINSTANCE{};
      return *this;
   }

   /// 如果發生錯誤, 則傳回錯誤字串.
   /// retval.empty() == true 表示成功.
   std::string Open(const char* path);

   void Close() {
      if (this->Instance_) {
         FreeLibrary(this->Instance_);
         this->Instance_ = HINSTANCE{};
      }
   }
   HINSTANCE Release() {
      HINSTANCE h = this->Instance_;
      this->Instance_ = HINSTANCE{};
      return h;
   }
   void* GetSym(const char* name) const {
      return this->Instance_ ? GetProcAddress(this->Instance_, name) : nullptr;
   }
};

class fon9_API DllHandleSym {
   fon9_NON_COPYABLE(DllHandleSym);
protected:
   DllHandle   Dll_;
   void*       Sym_{nullptr};

public:
   DllHandleSym() = default;

   void* GetSym() const {
      return this->Sym_;
   }

   /// 如果發生錯誤, 則傳回錯誤字串.
   /// retval.empty() == true 表示成功.
   std::string Open(const char* path, const char* symName);

   void Close() {
      this->Sym_ = nullptr;
      this->Dll_.Close();
   }

   HINSTANCE Release() {
      this->Sym_ = nullptr;
      return this->Dll_.Release();
   }
   DllHandle DllMoveOut() {
      this->Sym_ = nullptr;
      return std::move(this->Dll_);
   }
};

/// \ingroup Misc
/// 檢查 fnName 的 prototype 是否為 fnPrototype.
/// 必須由 fnName 的開發者自主檢查.
#define fon9_ASSERT_PROTOTYPE(fnPrototype, fnName) \
static_assert(std::is_same<fnPrototype, decltype(&fnName)>::value, "The prototype is not match.")

/// \ingroup Misc
/// 在動態載入 dll 之後, 如果 dll 有建立物件, 則必須等該物件死亡後才能卸載 dll.
/// 如果在一個集合物件中, 透過多個 dll 建立物件, 則 DllHolder 可協助保留 dll.
/// \code
///   class MyMgr : private DllHolder, public ... {
///      // dll 建立的物件放在此處, 當 MyMgr 解構時, 這些物件死光了, 才會透過 DllHolder 釋放 dll.
///      ObjVector   ObjVector_;
///
///      void CreateObj() {
///         DllHandleSym         dll;
///         DllHandleSym::Result res = dll.Open(dllEntryName);
///         if (res) {
///            MyCreaterFn fn = reinterpret_cast<MyCreaterFn>(dll.GetSym());
///            if (MyObjSP obj = fn()) {
///               this->ObjVector_.emplace_back(obj);
///               this->DllHandles_.emplace_back(dll.DllMoveOut());
///            }
///         }
///      }
///   };
/// \endcode
struct DllHolder {
   using DllHandles = std::vector<DllHandle>;
   DllHandles  DllHandles_;
};

} // namespaces
#endif//__fon9_DllHandle_hpp__
