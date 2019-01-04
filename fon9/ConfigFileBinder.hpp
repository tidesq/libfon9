// \file fon9/ConfigFileBinder.cpp
// \author fonwinz@gmail.com
#ifndef __fon9_ConfigFileBinder_hpp__
#define __fon9_ConfigFileBinder_hpp__
#include "fon9/StrView.hpp"
#include "fon9/File.hpp"

namespace fon9 {

/// \ingroup Misc
/// - 透過檔案讀/寫設定.
/// - 僅負責讀寫, 不解釋設定內容.
class fon9_API ConfigFileBinder {
   std::string FileName_;
   std::string ConfigStr_;
   TimeStamp   LastModifyTime_;
public:
   bool HasBinding() const {
      return !this->FileName_.empty();
   }
   const std::string& GetFileName() const {
      return this->FileName_;
   }

   /// 開啟設定檔, 載入內容, 若有失敗則傳回錯誤訊息.
   /// - retval.empty() 表示成功, 可透過 GetConfigStr() 取得載入的內容,
   ///   若 isAutoBackup==true(預設為true) 則返回前會先 BackupConfig().
   /// - 若有失敗, 除了傳回錯誤訊息外, 如果 logHeader != nullptr 則還會透過 log 記錄錯誤訊息.
   std::string OpenRead(StrView logErrHeader, std::string cfgfn, bool isAutoBackup = true);

   /// OpenRead() 成功之後, 透過這裡取得檔案(設定)內容.
   /// 僅完整德將檔案內容讀入, 不解釋其內容.
   const std::string& GetConfigStr() const {
      return this->ConfigStr_;
   }
   /// OpenRead() 的檔案時間, 或 Write() 成功的時間.
   TimeStamp GetLastModifyTime() const {
      return this->LastModifyTime_;
   }

   /// 設定有變動時, 將新的設定(cfgstr)寫回設定檔.
   /// - 設定檔為當初透過 OpenRead() 的 cfgfn;
   ///   若 cfgfn.empty() 或沒有呼叫過 OpenRead(); 則只會更新 GetConfigStr();
   /// - 若 cfgstr 與當初讀入的一樣, 則不會有寫入的動作.
   /// - 若有失敗, 除了傳回錯誤訊息外, 如果 logHeader != nullptr 則還會透過 log 記錄錯誤訊息.
   /// - 若 isAutoBackup==true(預設為false), 則在寫入前, 會將舊的設定使用 BackupConfig() 備份.
   std::string WriteConfig(StrView logErrHeader, std::string cfgstr, bool isAutoBackup = false);
};

/// \ingroup Misc
/// 使用檔案最後異動時間當作檔名, 建立備份檔案:
/// - FilePath::GetFullName(cfgFileName): 拆解成 path + name
/// - 備份用的檔名: path + "/cfgbak/" + name + ".yyyymmdd-HHMMSS.uuuuuu" (local time = utctm + GetLocalTimeZoneOffset())
/// - 備份的內容: cfgstr
/// - 若檔案已存在則 **不會** 寫入.
fon9_API void BackupConfig(StrView logErrHeader, StrView cfgFileName, TimeStamp utctm, const std::string& cfgstr);
fon9_API std::string WriteConfig(StrView logErrHeader, const std::string& cfgstr, File& fd);
inline void BackupConfig(StrView logErrHeader, const ConfigFileBinder& cfgb) {
   BackupConfig(logErrHeader, &cfgb.GetFileName(), cfgb.GetLastModifyTime(), cfgb.GetConfigStr());
}

} // namespace
#endif//__fon9_ConfigFileBinder_hpp__
