/// \file fon9/seed/Plugins.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_Plugins_hpp__
#define __fon9_seed_Plugins_hpp__
#include "fon9/seed/MaTree.hpp"
#include "fon9/DllHandle.hpp"
#include "fon9/Log.hpp"
#include "fon9/SimpleFactory.hpp"

namespace fon9 { namespace seed {

struct PluginsDesc;

fon9_WARN_DISABLE_PADDING;
/// \ingroup seed
/// 用來維護一個 Plugin 啟動後的運行、管理互動、結束.
class fon9_API PluginsHolder : public intrusive_ref_counter<PluginsHolder>, private DllHandleSym {
   fon9_NON_COPYABLE(PluginsHolder);
   uintmax_t   PluginsBookmark_{0};
   bool        IsRunning_{false};
   CharVector  CurFileName_;
   CharVector  CurEntryName_;

   void LogPluginsSt(LogLevel level, RevBufferList&& rbuf);
   /// To PluginsMgr thread and set state message.
   virtual void SetPluginStImpl(std::string stmsg) = 0;
protected:
   /// retval.empty() 載入成功, else 失敗訊息.
   std::string LoadPlugins(StrView fileName, StrView entryName);
   /// \retval true 啟動成功.
   /// \retval false 啟動失敗 or 重複啟動.
   bool StartPlugins(StrView args);
   void StopPlugins();
public:
   /// Plugins 啟動時, 可透過 Root_ 尋找、建立必要的物件(e.g. "FactoryPark_Device");
   const MaTreeSP   Root_;
   
   PluginsHolder(MaTreeSP root) : Root_{std::move(root)} {
   }
   virtual ~PluginsHolder();

   uintmax_t GetPluginsBookmark() const {
      return this->PluginsBookmark_;
   }
   void SetPluginsBookmark(uintmax_t value) {
      this->PluginsBookmark_ = value;
   }

   /// 記錄 log, 並設定 plugins 狀態.
   template <class... ArgsT>
   void SetPluginsSt(LogLevel level, ArgsT&&... args) {
      RevBufferList rbuf{kLogBlockNodeSize};
      RevPutChar(rbuf, '\n');
      RevPrint(rbuf, std::forward<ArgsT>(args)...);
      this->LogPluginsSt(level, std::move(rbuf));
   }

   const PluginsDesc* GetPluginsDesc() const {
      return reinterpret_cast<const PluginsDesc*>(this->Sym_);
   }
};
using PluginsHolderSP = intrusive_ptr<PluginsHolder>;

/// \ingroup seed
/// 擴充插件描述.
/// 描述一個「動態載入插件的進入點」包含: 啟動、結束、管理互動.
/// - 同一個插件可能會被啟用多次: 呼叫多次 FnStart_()
/// - 在 dll(so) 裡面的定義方式, 在適當的 cpp 裡面加上:
///   `extern "C" fon9_EXPORT const fon9::seed::PluginsDesc myPlugins{...};`
struct PluginsDesc {
   /// 此插件的文字說明, 可能包含: 版本、建構日期... 之類的訊息.
   const char* Description_;

   /// 返回 true 則表示插件啟動成功, 在插件卸載時會呼叫 FnStop_();
   /// 返回 false 則表示插件啟動失敗, 之後不會呼叫 FnStop_();
   bool (*FnStart_)(PluginsHolder& holder, StrView args);

   /// 當插件要卸載時呼叫.
   /// - 若您在 FnStart_() 有保留 PluginsHolder, 則在此處返回後, 就不可再使用了!
   /// - FnStop_ 可以是 nullptr, 表示 FnStart_() 成功後不用釋放.
   /// - FnStop_() 返回 false: 不可釋放 dll, 通常是因為還有物件在運行
   /// - FnStop_() 返回 true: 表示可以釋放 dll
   bool (*FnStop_)(PluginsHolder& holder);

   /// 當插件啟動之後, 可透過指令與已啟動的 plugins 互動.
   void (*FnCommand_)(PluginsHolder& holder, StrView cmdln, FnCommandResultHandler resHandler);
};

/// \ingroup seed
/// 如果您的系統不想用 dll(so), 可以採用預先註冊的方式使用 plugins.
/// 必須在系統啟用前先註冊, 或在 static lib 的 .cpp 裡面自主註冊:
/// \code
///   static fon9::seed::PluginsPark   ps{"name1", &myPlugins1,
///                                       "name2", &myPlugins2};
/// \endcode
class fon9_API PluginsPark : public SimpleFactoryPark<PluginsPark> {
   using base = SimpleFactoryPark<PluginsPark>;
public:
   using base::base;
   PluginsPark() = delete;

   /// - 若 plugins == nullptr 則用 name 查找先前註冊的 plugins.
   /// - 若 plugins != nullptr 且 name 重複, 則註冊失敗, 傳回先前的 plugins,
   ///   同時使用 fprintf(stderr, ...); 輸出錯誤訊息.
   /// - 若 plugins != nullptr 且 name 沒重複, 則註冊成功, 傳回 plugins.
   /// - plugins 必須在註冊後依然保持不變. 註冊表僅保留 plugins 的指標, 沒有複製其內容.
   static const PluginsDesc* Register(StrView name, const PluginsDesc* plugins);
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_seed_Plugins_hpp__
