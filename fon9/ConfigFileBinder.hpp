// \file fon9/ConfigFileBinder.cpp
// \author fonwinz@gmail.com
#ifndef __fon9_ConfigFileBinder_hpp__
#define __fon9_ConfigFileBinder_hpp__
#include "fon9/StrView.hpp"

namespace fon9 {

/// \ingroup Misc
/// - 透過檔案讀/寫設定.
/// - 僅負責讀寫, 不解釋設定內容.
class fon9_API ConfigFileBinder {
   std::string FileName_;
   std::string ConfigStr_;
public:
   bool HasBinding() const {
      return !this->FileName_.empty();
   }

   /// 開啟設定檔, 載入內容, 若有失敗則傳回錯誤訊息.
   /// - retval.empty() 表示成功, 可透過 GetConfigStr() 取得載入的內容.
   /// - 若有失敗, 除了傳回錯誤訊息外, 如果 logHeader != nullptr 則還會透過 log 記錄錯誤訊息.
   std::string OpenRead(StrView logHeader, std::string cfgfn);

   /// OpenRead() 成功之後, 透過這裡取得檔案(設定)內容.
   /// 僅完整德將檔案內容讀入, 不解釋其內容.
   const std::string& GetConfigStr() const {
      return this->ConfigStr_;
   }

   /// 寫設定有變動時, 將新的設定寫回設定檔.
   /// - 設定檔為當初透過 OpenRead() 的 cfgfn;
   ///   若 cfgfn.empty() 或沒有呼叫過 OpenRead(); 則只會更新 GetConfigStr();
   /// - 若 cfgstr 與當初讀入的一樣, 則不會有寫入的動作.
   /// - 若有失敗, 除了傳回錯誤訊息外, 如果 logHeader != nullptr 則還會透過 log 記錄錯誤訊息.
   std::string Write(StrView logHeader, std::string cfgstr);
};

} // namespace

#endif//__fon9_ConfigFileBinder_hpp__
