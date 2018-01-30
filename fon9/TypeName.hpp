/// \file fon9/TypeName.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_TypeName_hpp__
#define __fon9_TypeName_hpp__
#include "fon9/sys/Config.hpp"

#ifndef fon9_WINDOWS
#include <cxxabi.h>
#endif

namespace fon9 {

#ifdef fon9_WINDOWS
/// 排除 name 前方的 "class " or "struct ";
inline static const char* TypeNameDemangle(const char* name) {
   size_t namelen = strlen(name);
   #define fon9_TNAME_SKIP_HEAD(head) \
      if (namelen > (sizeof(head) - 1) && memcmp(name, head, (sizeof(head) - 1)) == 0) \
         return name + (sizeof(head) - 1);
   //---------------------------------
   fon9_TNAME_SKIP_HEAD("class ");
   fon9_TNAME_SKIP_HEAD("struct ");
   #undef fon9_STR_SKIP_HEAD
   return name;
}
class TypeName {
   const char* Name_;
   TypeName(const char* name) : Name_{name} {
   }
public:
   template <class T>
   TypeName(const T& t) : Name_{TypeNameDemangle(typeid(t).name())} {
      (void)t;
   }
   template <typename T>
   static TypeName Make() {
      return TypeName{TypeNameDemangle(typeid(T).name())};
   }
   const char* get() const {
      return this->Name_;
   }
};
#else
inline static const char* TypeNameDemangle(const char* name, char* buf, size_t bufsz) {
   int   status = 0;
   if (char* res = abi::__cxa_demangle(name, buf, &bufsz, &status))
      return res;
   return name;
}
class TypeName {
   char        NameBuffer_[1024];
   const char* Name_;
public:
   template <class T>
   TypeName(const T& t) : Name_{TypeNameDemangle(typeid(t).name(), NameBuffer_, sizeof(NameBuffer_))} {
      (void)t;
   }
   template <typename T>
   static TypeName Make() {
      return TypeName{*static_cast<T*>(nullptr)};
   }
   const char* get() const {
      return this->Name_;
   }
};
#endif

} // namespace fon9
#endif//__fon9_TypeName_hpp__
