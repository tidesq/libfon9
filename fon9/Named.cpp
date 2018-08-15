/// \file fon9/Named.cpp
/// \author fonwinz@gmail.com
#include "fon9/Named.hpp"
#include "fon9/StrTools.hpp"

namespace fon9 {

fon9_API const char* FindInvalidNameChar(StrView str) {
   const char* pend = str.end();
   const char* pbeg = str.begin();
   if (pbeg == pend || !IsValidName1stChar(*pbeg))
      return pbeg;
   while (++pbeg != pend) {
      if (!IsValidNameChar(*pbeg))
         return pbeg;
   }
   return nullptr;
}

fon9_API Named DeserializeNamed(StrView& cfg, char chSpl, int chTail) {
   StrView  descr = chTail == -1 ? cfg : SbrFetchField(cfg, static_cast<char>(chTail));
   StrView  name  = SbrFetchField(descr, chSpl);
   StrView  title = SbrFetchField(descr, chSpl);
   
   StrTrim(&name);
   if (const char* pInvalid = FindInvalidNameChar(name)) {
      cfg.SetBegin(pInvalid);
      return Named{};
   }
   cfg.SetBegin(descr.end() + (chTail != -1));
   return Named(name.ToString(),
      StrView_ToNormalizeStr(StrTrimRemoveQuotes(title)),
      StrView_ToNormalizeStr(StrTrimRemoveQuotes(descr)));
}

fon9_API void SerializeNamed(std::string& dst, const Named& named, char chSpl, int chTail) {
   dst.append(named.Name_);
   if (!named.GetTitle().empty() || !named.GetDescription().empty()) {
      dst.push_back(chSpl);
      if (!named.GetTitle().empty()) {
         dst.push_back('\'');
         StrView_ToEscapeStr(dst, &named.GetTitle());
         dst.push_back('\'');
      }
      if (!named.GetDescription().empty()) {
         dst.push_back(chSpl);
         dst.push_back('\'');
         StrView_ToEscapeStr(dst, &named.GetDescription());
         dst.push_back('\'');
      }
   }
   if (chTail != -1)
      dst.push_back(static_cast<char>(chTail));
}

} // namespaces
