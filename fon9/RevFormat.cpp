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
      if (pfmtcur != ctx.FmtEnd_ && *pfmtcur == chfmt) {
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

//--------------------------------------------------------------------------//

void FmtPre::DoFormat(const FmtContext& ctx) const {
   const char* const pbeg = this->FmtStr_.c_str();
   for (const FmtPreSlice& slice : this->Slices_) {
      if (slice.ArgId_ < 0)
         RevPutMem(*ctx.Buffer_, pbeg + slice.Begin_, pbeg + slice.End_);
      else if(slice.ArgId_ >= static_cast<int>(ctx.ArgCount_))
         RevPutMem(*ctx.Buffer_, pbeg + slice.BrBegin_, pbeg + slice.End_ + 1);
      else {
         const FmtArgContextBase* argctx = ctx.ArgContexts_[slice.ArgId_];
         if (slice.Begin_)
            argctx->RevPut(*ctx.Buffer_, StrView{pbeg + slice.Begin_, pbeg + slice.End_});
         else
            argctx->RevPut(*ctx.Buffer_);
      }
   }
}

void FmtPre::AddSlice(int argid, const char* pbeg, const char* pend) {
   if (argid >= 0 || pbeg != pend) {
      const char* strbeg = this->FmtStr_.c_str();
      this->Slices_.push_back(FmtPreSlice{argid,
                              static_cast<unsigned>(pbeg - strbeg),
                              static_cast<unsigned>(pend - strbeg),
                              0});
   }
}

const char* FmtPre::ParseFmtArg(int argid, const char* pfmtcur, const char* const pfmtend) {
   switch (*pfmtcur++) {
   case ':':
      if (const char* pend = static_cast<const char*>(memchr(pfmtcur, '}', static_cast<size_t>(pfmtend - pfmtcur)))) {
         this->ParseFmt(pend + 1, pfmtend);
         // 即使 pfmtcur == pend 也要使用 [格式化輸出],
         // 因為 ToStrRev(pout, value); ToStrRev(pout, value, fmt{""}); 可能會有不同的輸出!
         this->AddSlice(argid, pfmtcur, pend);
         return nullptr;
      }
      return pfmtend;
   case '}':
      this->ParseFmt(pfmtcur, pfmtend);
      this->Slices_.push_back(FmtPreSlice{argid,0,0,0});
      return nullptr;
   default:
      return pfmtcur;
   }
}

void FmtPre::ParseFmt(const char* pfmtcur, const char* const pfmtend) {
   const char* const pfmtbeg = pfmtcur;
   while (pfmtcur != pfmtend) {
      char chfmt = *pfmtcur++;
      if (chfmt != '{' && chfmt != '}')
         continue;
      if (pfmtcur != pfmtend && *pfmtcur == chfmt) {
         this->ParseFmt(pfmtcur + 1, pfmtend);
         break;
      }
      if (chfmt == '}')
         continue;
      const char* pend;
      size_t      argid = NaiveStrToUInt(pfmtcur, pfmtend, &pend);
      if (pend == pfmtend)
         break;
      if (this->MaxArgId_ < static_cast<int>(argid))
         this->MaxArgId_ = static_cast<int>(argid);
      if ((pend = this->ParseFmtArg(static_cast<int>(argid), pend, pfmtend)) == nullptr) {
         --pfmtcur; // 排除 '{' 不輸出.
         this->Slices_.back().BrBegin_ = static_cast<unsigned>(pfmtcur - pfmtbeg);
         break;
      }
      pfmtcur = pend;
   }
   this->AddSlice(-1, pfmtbeg, pfmtcur);
}

void FmtPre::Parse(std::string fmt) {
   this->FmtStr_ = std::move(fmt);
   this->Slices_.clear();
   this->MaxArgId_ = -1;
   const char* pfmtcur = this->FmtStr_.c_str();
   this->ParseFmt(pfmtcur, pfmtcur + this->FmtStr_.size());
}

bool FmtPre::GetArgIdFormat(unsigned argid, StrView& fmt) const {
   size_t L = this->Slices_.size();
   while (L > 0) {
      const FmtPreSlice& slice = this->Slices_[--L];
      if (static_cast<unsigned>(slice.ArgId_) == argid) {
         if (slice.Begin_ == 0)
            fmt.Reset(nullptr, nullptr);
         else {
            const char* pbeg = this->FmtStr_.c_str();
            fmt.Reset(pbeg + slice.Begin_, pbeg + slice.End_);
         }
         return true;
      }
   }
   return false;
}

} // namespace
