/// \file fon9/ConfigLoader.cpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ConfigLoader_hpp__
#define __fon9_ConfigLoader_hpp__
#include "fon9/buffer/RevBuffer.hpp"
#include "fon9/CharVector.hpp"
#include "fon9/File.hpp"
#include "fon9/SortedVector.hpp"
#include "fon9/intrusive_ref_counter.hpp"
#include "fon9/Exception.hpp"

namespace fon9 {

/// \ingroup Misc
/// 載入設定內容.
/// - 移除註解及尾端空白, 保留換行字元.
/// - 逐行展開:
///   - 處理 `$include:cfgFileName` 引入額外設定檔
///      - cfgFileName 支援單行變數展開
///      - 路徑選擇順序:
///         1. 優先使用設定檔相同路徑.
///         2. DefaultConfigPath_
///         3. 現在路徑
///   - 處理變數設定:
///      - $Variable = single line
///        - 單行訊息會移除前後空白.
///        - 如果確定要有空白, 可使用: $Variable = {  single line  }
///      - $Variable = {single/multi lines}
///        - 此時 Variable 的值為大括號內部的資料, 不含大括號.
///        - 處理到右方大括號之後, 會接續處理, 例如, 底下的訊息可以在同一行:
///          - $TxLang=${TxLang:-zh} $include:forms/ApiAll_$TxLang.cfg
///      - 變數重複設定: 後蓋前
///   - 處理變數展開, 底下假設 $var = {OLD}
///      - $var 尾端必須有非命名字元, 例如: `$var.cfg` 會展開成 `OLD.cfg`
///      - ${var} 這種用法可以連結字串: ${var}Tail 避免 $varTail 是另一個變數的問題.
///         - ${var}Tail 會得到 `OLDTail`
///      - ${var-value}  當 $var 沒定義, 則傳回 value; 此處因 $var = {OLD}, 所以傳回 `OLD`
///      - ${var:-value} 當 $var 沒定義or為空白, 則傳回 value; 此處因 $var = {OLD}, 所以傳回 `OLD`
///      - 上面的 `${`, `-`, `:-` 必須連在一起, 不可分開.
///      - 用 ${var} 取得變數值, 如果變數不存在則: `${var}` 會被移除.
///      - 用 $var 取得變數值, 如果變數不存在則將 `$var` 留在原地.
/// - GetCfgStr() 取得最後展開後的結果, 若發現有誤, 可透過 GetLineFrom() 取得錯誤位置的資料來源(fileName:Ln#).
/// - GetVarMap() 取得全部的變數列表, 除了使用 GetCfgStr() 解析展開後的內容, 也可透過 GetVarMap() 取得變數列表來進行設定.
/// - 錯誤處理: 拋出 ConfigLoader::Err 例外.
class fon9_API ConfigLoader {
   fon9_NON_COPY_NON_MOVE(ConfigLoader);
public:
   using VarName = CharVector;
   using LineCount = uint32_t;
   using ColCount = uint32_t;
   struct LineFrom;
   using LineFromSP = intrusive_ptr<const LineFrom>;
   using Pos = std::string::size_type;

   /// 這裡的 vector<> 不儲存逐行的資訊, 僅在 from 有變動時, 記錄一筆.
   /// 若有需要知道某位置的來源, 可透過 GetLineFrom() 取得.
   struct fon9_API LineInfos : std::vector<LineFromSP> {
      using base = std::vector<LineFromSP>;
      std::string Str_;
      void push_back(LineFromSP copyFrom, LineCount ln, ColCount col) {
         this->emplace_back(LineFromSP{new LineFrom(this->Str_.size(), copyFrom, ln, col)});
      }
      void clear() {
         base::clear();
         this->Str_.clear();
      }
      /// pos 必須是在 this->Str_ 之內.
      /// \retval nullptr  pos位置無效.
      /// \retval !nullptr pos所在的 LineNo_, ColNo_ 等相關資訊.
      LineFromSP GetLineFrom(const char* pos) const;
   };

   fon9_WARN_DISABLE_PADDING;
   struct Variable : public intrusive_ref_counter<Variable, thread_unsafe_counter> {
      fon9_NON_COPY_NON_MOVE(Variable);
      Variable(const StrView& name) : Name_{name} {
      }
      /// 變數名稱.
      const VarName  Name_;
      /// Value_.Str_ = 變數的內容, 可能有多行.
      LineInfos      Value_;
      /// 此變數來自何方?
      /// 可透過 ConfigLoader::ToInfoMessage(); 取得「來自何方」的文字訊息.
      LineFromSP     From_;
   };
   using VariableSP = intrusive_ptr<Variable>;

   struct fon9_API LineFrom : public intrusive_ref_counter<LineFrom, thread_unsafe_counter> {
      /// 此行所在位置: LineInfos_.Str_.c_str() + Pos_;
      Pos         Pos_;
      std::string FileName_;
      LineCount   FileLn_{0};
      ColCount    ColNo_{0};
      uint32_t    IncludeDeep_{0};
      LineFromSP  IncludeFrom_;
      VariableSP  VariableFrom_;

      LineFrom(Pos pos, LineFromSP includeFrom, const File& fd)
         : Pos_{pos}
         , FileName_{fd.GetOpenName()}
         , FileLn_{1}
         , IncludeDeep_{includeFrom ? includeFrom->IncludeDeep_ + 1 : 0}
         , IncludeFrom_{std::move(includeFrom)} {
      }
      LineFrom(Pos pos, LineFromSP copyFrom, LineCount newLn, ColCount colNo) {
         if (copyFrom)
            *this = *copyFrom;
         this->Pos_ = pos;
         this->FileLn_ = newLn;
         this->ColNo_ = colNo;
      }
   };
   fon9_WARN_POP;

   struct CompareVariableSP {
      bool operator()(const VariableSP& lhs, const VariableSP& rhs) const {
         return lhs->Name_ < rhs->Name_;
      }
      bool operator()(const VariableSP& v, const StrView& key) const {
         return ToStrView(v->Name_) < key;
      }
      bool operator()(const StrView& key, const VariableSP& v) const {
         return key < ToStrView(v->Name_);
      }
   };
   using VarMap = SortedVectorSet<VariableSP, CompareVariableSP>;

   //--------------------------------------------------------------------------//

   class Err : public std::runtime_error {
      using base = std::runtime_error;
   public:
      LineFromSP  From_;
      ErrC        ErrC_;
      Err() = delete;
      Err(const std::string& what, LineFromSP from, ErrC err)
         : base{what}
         , From_{std::move(from)}
         , ErrC_{std::move(err)} {
      }
   };

   //--------------------------------------------------------------------------//

   const std::string DefaultConfigPath_;

   ConfigLoader(std::string defaultConfigPath) : DefaultConfigPath_{std::move(defaultConfigPath)} {
   }
   virtual ~ConfigLoader();

   void Clear();

   /// 讀入設定檔, 並呼叫 Append() 展開內容.
   /// 檔案格式支援:
   /// - UTF-8, 可以有 BOM(ef bb bf).
   /// - ASCII
   /// - 使用 '\n' 換行
   /// - 傳回 line count, 若有失敗則拋出 Err 異常.
   LineCount LoadFile(const StrView& cfgfn) {
      return this->IncludeFile(nullptr, cfgfn);
   }

   /// 展開 cfgstr 的內容.
   /// - 傳回 line count, 若有失敗則拋出 Err 異常.
   LineCount Append(const StrView& cfgstr) {
      return this->Append(nullptr, cfgstr);
   }

   /// cfgstr = "default configs\n" "$include:file"; 當 file 不存在時, 不拋出異常.
   /// - 若 cfgstr 有多個 "$include:" 時, 則在第一個檔案找不到時就會中斷處理, cfgstr 後續的設定就不會處理了.
   /// - 仍會拋出其他解析異常.
   void IncludeConfig(const StrView& cfgstr);

   /// - StrTrimHead(&cfg);
   /// - if (cfgs.Get1st() == '$') 使用 Append(cfgs); 載入設定.
   ///   - e.g. `$TxLang={zh} $include:forms/ApiAll.cfg`
   /// - if (cfgs.Get1st() != '$') 使用 LoadFile(cfgs); 載入設定.
   ///   - e.g. `forms/ApiAll.cfg`
   /// - 傳回 line count, 若有失敗則拋出 Err 異常.
   LineCount CheckLoad(StrView cfgs) {
      if (StrTrimHead(&cfgs).Get1st() == '$')
         return this->Append(cfgs);
      else
         return this->LoadFile(cfgs);
   }

   /// 取得 [移除/展開] 變數($var)之後 的內容.
   /// 一般而言在 LoadFile(), Append() 做完之後,
   /// 透過這裡解析所需要的設定.
   const std::string& GetCfgStr() const {
      return this->LineInfos_.Str_;
   }

   /// pos 必須是在 GetCfgStr() 的位置.
   LineFromSP GetLineFrom(const char* pos) const {
      return this->LineInfos_.GetLineFrom(pos);
   }

   /// 取得變數列表, 變數名稱不包含開頭的 '$'
   const VarMap& GetVarMap() const {
      return this->VarMap_;
   }

   /// varName 不含 '$'
   /// 取出的值, **包含** 前後空白.
   VariableSP GetVariable(const StrView& varName) const {
      VarMap::const_iterator ifind = this->VarMap_.find(varName);
      return ifind == this->VarMap_.end() ? VariableSP{} : *ifind;
   }

   /// - 一般而言是在內部解析時使用.
   /// - StrTrimHead(&pr);
   /// - 從 pr 取出 name (不含 '$').
   /// - 用 name 取得 var (可能為 nullptr: 找不到).
   /// - 設定 pr.SetBegin(name.end());
   VariableSP FindVariable(StrView& pr) const;

protected:
   /// 預設呼叫 this->IncludeFile();
   /// - 傳回 line count, 若有失敗則拋出 Err 異常.
   virtual LineCount OnConfigInclude(LineFromSP includeFrom, const StrView& cfgfn);

private:
   LineInfos   LineInfos_;
   VarMap      VarMap_;

   File::Result OpenFile(const LineFrom* includeFrom, File& fd, StrView cfgfn);
   // 傳回讀入的 bytes 數量, 若有錯誤, 則拋出 Err 異常.
   File::SizeType OpenRead(LineFromSP& includeFrom, const StrView& cfgfn, CharVector& cfgout);
   LineCount IncludeFile(LineFromSP includeFrom, const StrView& cfgfn);

   struct Appender;
   LineCount Append(LineFromSP from, const StrView& cfgstr);
};

/// \ingroup Misc
/// 輸出 from 的文字資訊, 追查某來源.
/// 格式: FileName:LnNo:ColNo->InNestFileName:LnNo:ColNo...
fon9_API void RevPrint(RevBuffer& rbuf, const ConfigLoader::LineFrom* from);
inline void RevPrint(RevBuffer& rbuf, const ConfigLoader::LineFromSP& from) {
   RevPrint(rbuf, from.get());
}

/// \ingroup Misc
/// 輸出 e 的文字資訊, 追查某來源.
/// 格式: RevPrint(rbuf, e.what(), "|errc=", e.ErrC_, "|from=", e.From_.get());
fon9_API void RevPrint(RevBuffer& rbuf, const ConfigLoader::Err& e);

} // namespace
#endif//__fon9_ConfigLoader_hpp__
