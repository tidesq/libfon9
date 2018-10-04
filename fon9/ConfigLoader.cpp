// \file fon9/ConfigLoader.cpp
// \author fonwinz@gmail.com
#include "fon9/ConfigLoader.hpp"
#include "fon9/FilePath.hpp"
#include "fon9/RevPrint.hpp"
#include "fon9/Named.hpp"

namespace fon9 {

static constexpr StrBrPair CfgBrPair[]{
   {'\'', '\'', false},
   {'"', '"', false},
   {'`', '`', false},
   {'#', '\n', false},
};
static const StrBrArg CfgBrArg{CfgBrPair};

inline static ConfigLoader::LineCount CountLnCol(const char* pbeg, const char* pend, ConfigLoader::ColCount& col) {
   ConfigLoader::LineCount ln = 0;
   while (pbeg < pend) {
      if (*pbeg++ != '\n')
         ++col;
      else {
         ++ln;
         col = 1;
      }
   }
   return ln;
}

//--------------------------------------------------------------------------//

ConfigLoader::LineFromSP ConfigLoader::LineInfos::GetLineFrom(const char* pos) const {
   Pos pn = static_cast<Pos>(pos - this->Str_.c_str());
   if (pn >= this->Str_.size() || this->empty())
      return nullptr;
   const_iterator ifind = std::lower_bound(this->begin(), this->end(), pn,
                                           [](const LineFromSP& i, Pos k) { return i->Pos_ < k; });
   LineFromSP from = (ifind == this->end() ? this->back()
                      : ((*ifind)->Pos_ == pn || ifind == this->begin()) ? *ifind
                      : *(ifind - 1));
   LineCount ln = from->FileLn_;
   ColCount  col = from->ColNo_;
   if (from->VariableFrom_ && pn >= from->Pos_)
      pn -= from->Pos_;
   else
      ln += CountLnCol(this->Str_.c_str() + from->Pos_, pos, col);
   return LineFromSP{new LineFrom(pn, from, ln, col)};
}

fon9_API void RevPrint(RevBuffer& rbuf, const ConfigLoader::LineFrom* from) {
   if (from == nullptr) {
      RevPutChar(rbuf, ':');
      return;
   }
   for (;;) {
      if (ConfigLoader::Variable* var = from->VariableFrom_.get()) {
         RevPutChar(rbuf, '}');
         if(ConfigLoader::LineFromSP invar = var->Value_.GetLineFrom(var->Value_.Str_.c_str() + from->Pos_))
            RevPrint(rbuf, invar.get());
         if (const ConfigLoader::LineFrom* varFrom = var->From_.get())
            RevPrint(rbuf, '@', varFrom);
         RevPrint(rbuf, "${", var->Name_);
      }
      RevPrint(rbuf, from->FileName_, ':', from->FileLn_, ':', from->ColNo_);
      if ((from = from->IncludeFrom_.get()) == nullptr)
         break;
      RevPrint(rbuf, "->");
   }
}

fon9_API void RevPrint(RevBuffer& rbuf, const ConfigLoader::Err& e) {
   RevPrint(rbuf, e.what(), "|errc=", e.ErrC_, "|from=", e.From_.get());
}

//--------------------------------------------------------------------------//

ConfigLoader::~ConfigLoader() {
}
void ConfigLoader::Clear() {
   this->LineInfos_.clear();
   this->VarMap_.clear();
}

void ConfigLoader::IncludeConfig(const StrView& cfgstr) {
   try {
      this->Append(cfgstr);
   }
   catch (Err& e) {
      if (e.ErrC_ != std::errc::no_such_file_or_directory || !e.From_ || !e.From_->FileName_.empty())
         throw;
   }
}
File::Result ConfigLoader::OpenFile(const LineFrom* includeFrom, File& fd, StrView cfgfn) {
   if (FilePath::HasPrefixPath(cfgfn))
      return fd.Open(cfgfn.ToString(), FileMode::Read);
   if (includeFrom == nullptr) {
      for (const LineFromSP& li : this->LineInfos_)
         if (!li->FileName_.empty()) {
            includeFrom = li.get();
            break;
         }
   }
   File::Result res;
   std::string  fn1;
   if (includeFrom) {
      fn1 = FilePath::ExtractPathName(ToStrView(includeFrom->FileName_));
      fn1 = FilePath::MergePath(ToStrView(fn1), cfgfn);
      res = fd.Open(fn1, FileMode::Read);
      if (res.HasResult() || res.GetError() != std::errc::no_such_file_or_directory)
         return res;
   }
   std::string fn2;
   if (!this->DefaultConfigPath_.empty()) {
      fn2 = FilePath::MergePath(&this->DefaultConfigPath_, cfgfn);
      if (fn2 == fn1)
         fn2.clear();
      else {
         res = fd.Open(fn2, FileMode::Read);
         if (res.HasResult() || res.GetError() != std::errc::no_such_file_or_directory)
            return res;
      }
   }
   if (cfgfn == &fn1 || cfgfn == &fn2)
      return File::Result{std::errc::no_such_file_or_directory};
   return fd.Open(cfgfn.ToString(), FileMode::Read);
}
File::SizeType ConfigLoader::OpenRead(LineFromSP& includeFrom, const StrView& cfgfn, CharVector& cfgout) {
   File           fd;
   auto           res = this->OpenFile(includeFrom.get(), fd, cfgfn);
   StrView        errfn; // err function name.
   RevBufferList  errbuf{128};
   if (!res) {
      errfn = "Open";
   __RAISE_ERROR:
      RevPrint(errbuf, "ConfigLoader.OpenRead.", errfn, "|fileName=", fd.GetOpenName());
      Raise<Err>(BufferTo<std::string>(errbuf.MoveOut()), includeFrom, res.GetError());
   }
   res = fd.GetFileSize();
   if (!res) {
      errfn = "GetFileSize";
      goto __RAISE_ERROR;
   }
   File::SizeType fsz = res.GetResult();
   void* buf = cfgout.alloc(fsz);
   if (buf == nullptr) {
      errfn = "Alloc";
   __RAISE_ERROR_FILE_SIZE:
      RevPrint(errbuf, "|fsz=", fsz);
      goto __RAISE_ERROR;
   }
   res = fd.Read(0, buf, fsz);
   if (!res) {
      errfn = "Read";
      goto __RAISE_ERROR_FILE_SIZE;
   }
   if (res.GetResult() != fsz) {
      res = std::errc::io_error;
      RevPrint(errbuf, "|rdsz=", res.GetResult());
      errfn = "Less";
      goto __RAISE_ERROR_FILE_SIZE;
   }
   includeFrom.reset(new LineFrom(this->LineInfos_.Str_.size(), includeFrom, fd));
   return res.GetResult();
}
ConfigLoader::LineCount ConfigLoader::IncludeFile(LineFromSP includeFrom, const StrView& cfgfn) {
   CharVector cfgout;
   if (this->OpenRead(includeFrom, cfgfn, cfgout) <= 0)
      return 0;
   // 移除 UTF-8-BOM: ef bb bf
   StrView cfgstr{ToStrView(cfgout)};
   if (cfgstr.size() >= 3) {
      const char* pbeg = cfgstr.begin();
      if (pbeg[0] == '\xef' && pbeg[1] == '\xbb' && pbeg[2] == '\xbf')
         cfgstr.SetBegin(pbeg + 3);
   }
   return this->Append(includeFrom, cfgstr);
}
ConfigLoader::LineCount ConfigLoader::OnConfigInclude(LineFromSP includeFrom, const StrView& cfgfn) {
   if (includeFrom && includeFrom->IncludeFrom_ && includeFrom->IncludeDeep_ > 10)
      Raise<Err>("ConfigLoader::OnConfigInclude|err=include nested too deeply.",
                 includeFrom, std::errc::too_many_files_open);
   return this->IncludeFile(includeFrom, cfgfn);
}

ConfigLoader::VariableSP ConfigLoader::FindVariable(StrView& pr) const {
   VariableSP var;
   if (const char* pnEnd = FindInvalidNameChar(StrTrimHead(&pr))) {
      var = this->GetVariable(StrView{pr.begin(), pnEnd});
      pr.SetBegin(pnEnd);
   }
   else {
      var = this->GetVariable(pr);
      pr.SetBegin(pr.end());
   }
   return var;
}

//--------------------------------------------------------------------------//

struct ConfigLoader::Appender {
   fon9_NON_COPY_NON_MOVE(Appender);
   ConfigLoader&  Loader_;
   LineFromSP     From_;
   StrView        CfgRemain_;
   LineInfos&     LineInfos_;
   LineCount      LineNo_{1};
   ColCount       ColNo_{1};
   Appender(ConfigLoader& loader, LineFromSP from, const StrView& cfgstr, LineInfos& linfos)
      : Loader_(loader)
      , From_{std::move(from)}
      , CfgRemain_{cfgstr}
      , LineInfos_(linfos) {
   }
   virtual ~Appender() {
   }
   enum BeforeAppendResult {
      BeforeAppend_NormalLine,
      BeforeAppend_ContinueSameLine,
      BeforeAppend_ToNextLineNormal,
      BeforeAppend_ToNextLineInfoRequired,
   };
   virtual BeforeAppendResult BeforeAppendLine(StrView& lnpr) {
      (void)lnpr;
      return BeforeAppend_NormalLine;
   }
   LineCount Run() {
      for (;;) {
         const char*  plnEnd = this->CfgRemain_.Find('\n');
         StrView      lnpr{plnEnd ? StrView{this->CfgRemain_.begin(), plnEnd} : this->CfgRemain_};
         lnpr = SbrFetchNoTrim(StrTrimHead(&lnpr), '#', StrBrArg::Quotation_); // remove remark.
         // lnpr = 一行: 移除開頭空白, 移除尾端註解(僅考慮「引號」, 不考慮括號, 因為括號可能跨行, 引號不會跨行)
         // 此時 this->CfgRemain_.begin() 指向此行的開始位置, 暫時不能動它, 因為它是計算 ColNo 的基礎.
         BeforeAppendResult appRes;
         switch (appRes = this->BeforeAppendLine(lnpr)) {
         case BeforeAppend_ContinueSameLine:
            continue;
         case BeforeAppend_NormalLine:
            this->ExpandLine(lnpr);
            // 不用 break, 檢查是否需要 push_back('\n');
         case BeforeAppend_ToNextLineNormal:
            if (plnEnd)
               this->LineInfos_.Str_.push_back('\n');
            break;
         case BeforeAppend_ToNextLineInfoRequired:
            break;
         }
         if (plnEnd == nullptr)
            break;
         this->CfgRemain_.SetBegin(plnEnd + 1);
         this->ColNo_ = 1;
         ++this->LineNo_;
         if (appRes == BeforeAppend_ToNextLineInfoRequired)
            this->LineInfos_.push_back(this->From_, this->LineNo_, this->ColNo_);
      }
      return this->LineNo_;
   }
   void ExpandLine(StrView lnpr) {
      StrView     inVar;
      const char* prvBeg;
      // lnpr 還原到尚未 StrTrimHead() 之前的位置 = 此行原本開始位置, 因為: 展開時,保留原本空白.
      lnpr.SetBegin(prvBeg = this->CfgRemain_.begin());
      while (const char* pnBeg = lnpr.Find('$')) {
         lnpr.SetBegin(pnBeg + 1);
         VariableSP  var;
         if (lnpr.Get1st() != '{') {
            var = this->Loader_.FindVariable(lnpr);
            if (!var) // $var 變數必須存在, 如果不存在, 則將 `$var` 留在原地.
               continue;
         }
         else { // ${var} 這種用法可以連結字串: ${var}Tail 避免 $varTail 是另一個變數的問題.
            lnpr.SetBegin(pnBeg + 2);
            inVar = SbrFetchNoTrim(lnpr, '}');
            if (inVar.end() == lnpr.begin()) {
               this->ColNo_ += static_cast<ColCount>(pnBeg + 1 - prvBeg); // 指向 '{' 的位置.
               Raise<Err>("ConfigLoader.ExpandLine|err=Bad variable expand, no corresponding closing '}' found.",
                          new LineFrom(this->LineInfos_.Str_.size(), this->From_, this->LineNo_, this->ColNo_),
                          std::errc::bad_message);
            }
            var = this->Loader_.FindVariable(inVar);
            switch(StrTrimHead(&inVar).Get1st()) {
            case '-': // ${var-value} 當 $var 沒定義, 則傳回 value; 否則傳回 $var
               inVar.SetBegin(inVar.begin() + 1);
               // 如果 var存在: 使用 var內容 取代 ${var-value}.
               // 如果不存在, 使用 value(剩餘的inVar) 取代 ${var-value}.
               break;
            case ':': // ${var:-value} 當 $var 沒定義or為空白, 則傳回 value; 否則傳回 $var
               inVar.SetBegin(inVar.begin() + 1);
               if (inVar.Get1st() != '-') // 不認識的格式, 要視為錯誤嗎? 目前為: 保留原樣.
                  continue;
               inVar.SetBegin(inVar.begin() + 1);
               if (var && var->Value_.Str_.empty())
                  var.reset();
               break;
            default: // ${var ...} 其中的「...」要視為錯誤嗎? 目前為: 不認識的格式, 保留原樣.
               if (!inVar.empty())
                  continue;
               break;
            }
         }
         if (prvBeg != pnBeg) {
            this->LineInfos_.push_back(this->From_, this->LineNo_, this->ColNo_);
            this->LineInfos_.Str_.append(prvBeg, pnBeg);
            this->ColNo_ += static_cast<ColCount>(pnBeg - prvBeg);
         }
         this->LineInfos_.push_back(this->From_, this->LineNo_, this->ColNo_);
         if (var) {
            this->LineInfos_.Str_.append(var->Value_.Str_);
            const_cast<LineFrom*>(this->LineInfos_.back().get())->VariableFrom_ = var;
         }
         else {
            inVar.AppendTo(this->LineInfos_.Str_);
         }
         this->ColNo_ += static_cast<ColCount>(lnpr.begin() - pnBeg);
         // 變數後面剩下空白, 則移除.
         if (StrFindTrimHead(lnpr.begin(), lnpr.end()) == lnpr.end()) {
            prvBeg = lnpr.end();
            break;
         }
         prvBeg = lnpr.begin();
      }
      if (prvBeg != this->CfgRemain_.begin())
         this->LineInfos_.push_back(this->From_, this->LineNo_, this->ColNo_);
      if (prvBeg != lnpr.end())
         this->LineInfos_.Str_.append(prvBeg, lnpr.end());
   }
};

ConfigLoader::LineCount ConfigLoader::Append(LineFromSP from, const StrView& cfgstr) {
   struct MainAppender : Appender {
      fon9_NON_COPY_NON_MOVE(MainAppender);
      using Appender::Appender;
      LineFromSP GetCurrentFrom() {
         return LineFromSP{new LineFrom(this->LineInfos_.Str_.size(), this->From_, this->LineNo_, this->ColNo_)};
      }
      virtual BeforeAppendResult BeforeAppendLine(StrView& lnpr) override {
         if (lnpr.Get1st() != '$')
            return BeforeAppend_NormalLine;
         const char* plnbeg = lnpr.begin();
         static const char cstrInclude[] = "include:";
         if (lnpr.size() > sizeof(cstrInclude) && memcmp(plnbeg + 1, cstrInclude, sizeof(cstrInclude) - 1) == 0) {
            // $include:cfgFileName
            //this->ColNo_ += static_cast<ColCount>(plnbeg - this->CfgRemain_.begin());// ColNo = '$' 的位置.
            lnpr.SetBegin(plnbeg + sizeof(cstrInclude)); // cstrInclude 的 EOS, 與 *plnbeg 的 '$' 抵銷, 所以不用 -1
            StrTrim(&lnpr);
            this->ColNo_ += static_cast<ColCount>(lnpr.begin() - this->CfgRemain_.begin());// ColNo = '$include:' 之後的位置.
            // 變數展開: cfgFileName, 例如:  $include: Api_${TxLang}.cfg
            LineFromSP incFrom = this->GetCurrentFrom();
            LineInfos  incln;
            Appender   app(this->Loader_, incFrom, lnpr, incln);
            app.LineNo_ = this->LineNo_;
            app.ColNo_ = this->ColNo_;
            app.Run();
            StrView    includeFileName{&incln.Str_};
            LineFromSP incFrom2 = incln.GetLineFrom(StrTrim(&includeFileName).begin());
            this->Loader_.OnConfigInclude(incFrom2 ? incFrom2 : incFrom, includeFileName);
            return BeforeAppend_ToNextLineInfoRequired;
         }
         lnpr.SetBegin(plnbeg + 1);
         StrView varName = StrFetchNoTrim(lnpr, '=');
         if (varName.end() == lnpr.begin()) // 沒有找到 '='.
            return BeforeAppend_NormalLine;
         StrTrim(&varName);
         if (FindInvalidNameChar(varName) != nullptr) // 有無效的字元.
            return BeforeAppend_NormalLine;
         // 處理變數設定: $Var = single line 或 $Var = {single/multi lines}
         plnbeg = this->CfgRemain_.begin();
         bool      isSingleLine = (StrTrimHead(&lnpr).Get1st() != '{');
         LineCount lcount;
         ColCount  coln = this->ColNo_;
         if (isSingleLine) { // 變數設定被拿走之後, 該行變成空行, 所以填入 '\n'.
            lcount = 0;
            StrTrimTail(&lnpr);
         }
         else {
            const char* pBrOpen = lnpr.begin();
            this->CfgRemain_.SetBegin(pBrOpen + 1);
            lnpr = SbrFetchNoTrim(this->CfgRemain_, '}', CfgBrPair); // CfgBrPair = 考慮行尾的 # 不對稱右括號 } 註解.
            // lnpr=大括號內的文字, this->CfgRemain_.begi()=右大括號+1.
            if (lnpr.end() == this->CfgRemain_.begin()) {
               this->ColNo_ += static_cast<ColCount>(pBrOpen - plnbeg); // 指向 '{' 的位置.
               Raise<Err>(RevPrintTo<std::string>("ConfigLoader.Append|var=", varName, "|err=Bad variable defined, no corresponding closing '}' found."),
                          this->GetCurrentFrom(), std::errc::bad_message);
            }
            if ((lcount = CountLnCol(plnbeg, CfgRemain_.begin(), coln)) > 0)
               this->LineInfos_.Str_.append(lcount, '\n');
         }
         // 處理變數內容的展開, 例如: $TxLang=${TxLang:-zh}, 此時 ln = "${TxLang:-zh}";
         // 要將 ln 展開成 "zh" 或 $TxLang 的內容.
         VariableSP var{new Variable{varName}};
         var->From_ = this->GetCurrentFrom();
         Appender   app(this->Loader_, var->From_, lnpr, var->Value_);
         app.LineNo_ = this->LineNo_;
         app.ColNo_ = this->ColNo_ + static_cast<ColCount>(lnpr.begin() - plnbeg);
         app.Run();
         *this->Loader_.VarMap_.insert(var).first = var; // 此 varName 如果有重複設定, 則使用此處新的 var (後蓋前).
         this->LineNo_ += lcount;
         this->ColNo_ = coln;
         return(isSingleLine ? BeforeAppend_ToNextLineNormal : BeforeAppend_ContinueSameLine);
      }
   };
   MainAppender app(*this, from, cfgstr, this->LineInfos_);
   return app.Run();
}

} // namespace
