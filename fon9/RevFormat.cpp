/// \file fon9/RevFormat.cpp
/// \author fonwinz@gmail.com
#include "fon9/RevFormat.hpp"
#include "fon9/StrTo.hpp"

namespace fon9 {

static const char* ParseFmtArg(const FmtContext& ctx, size_t argid, const char* pfmtcur) {
   const FmtArgContextBase* argctx = ctx.ArgContexts_[argid];
   switch (*pfmtcur++) {
   case ':':
      if (const char* pfmtend = static_cast<const char*>(memchr(pfmtcur, '}', static_cast<size_t>(ctx.FmtEnd_ - pfmtcur)))) {
         ParseFmt(ctx, pfmtend + 1);
         // 即使 pfmtcur == pfmtend 也要使用 [格式化輸出],
         // 因為 ToStrRev(pout, value); ToStrRev(pout, value, fmt{""}); 可能會有不同的輸出!
         argctx->RevPut(*ctx.Buffer_, StrView{pfmtcur, pfmtend});
         return nullptr;
      }
      return ctx.FmtEnd_;
   case '}':
      ParseFmt(ctx, pfmtcur);
      argctx->RevPut(*ctx.Buffer_);
      return nullptr;
   default:
      return pfmtcur;
   }
}

fon9_API void ParseFmt(const FmtContext& ctx, const char* pfmtcur) {
   const char* const pfmtbeg = pfmtcur;
   while (pfmtcur != ctx.FmtEnd_) {
      char chfmt = *pfmtcur++;
      if (chfmt != '{' && chfmt != '}')
         continue;
      if (pfmtcur != ctx.FmtEnd_&& *pfmtcur == chfmt) {
         ParseFmt(ctx, pfmtcur + 1);
         break;
      }
      if (chfmt == '}')
         continue;
      const char* pend;
      size_t      argid = NaiveStrToUInt(pfmtcur, ctx.FmtEnd_, &pend);
      if (argid >= ctx.ArgCount_)
         continue;
      if (pend == ctx.FmtEnd_)
         break;
      if ((pend = ParseFmtArg(ctx, argid, pend)) == nullptr) {
         --pfmtcur; // 排除 '{' 不輸出.
         break;
      }
      pfmtcur = pend;
   }
   RevPutMem(*ctx.Buffer_, pfmtbeg, pfmtcur);
}

} // namespace
