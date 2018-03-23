/// \file fon9/FilePath.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_FilePath_hpp__
#define __fon9_FilePath_hpp__
#include "fon9/StrView.hpp"
#include "fon9/Outcome.hpp"
#include <sys/stat.h>
#include <vector>

#ifndef S_ISDIR
#define S_ISDIR(m)   (((m) & _S_IFDIR) == _S_IFDIR)
#endif

namespace fon9 {

/// \ingroup Misc
/// 路徑相關處理.
class fon9_API FilePath {
public:
   #ifdef fon9_WINDOWS
      typedef int  mode_t;
      enum : mode_t {
         DefaultPathAccessMode_ = 0
      };
   #else
      /// 建立路徑時, 預設的路徑存取權限.
      enum : mode_t {
         DefaultPathAccessMode_ = S_IRUSR | S_IWUSR | S_IXUSR
                                | S_IRGRP | S_IXGRP
                                | S_IROTH | S_IXOTH,
         DefaultFileAccessMode_ = S_IRUSR | S_IWUSR
                                | S_IRGRP
                                | S_IROTH,
      };
   #endif

   using Result = Outcome<unsigned>;

   /// 把 '\\' 轉成 '/';
   static std::string NormalizePathName(StrView fname);

   /// 把 '\\' 轉成 '/', 且若 path 尾端沒有 '/' 則加上;
   static std::string AppendPathTail(StrView path);

   /// 移除 path 尾端的 '/'; 不會先做 NormalizePathName()!
   static StrView RemovePathTail(StrView path);

   /// 從檔名 fname 之中, 抽取出路徑名稱.
   /// - 不含尾端的分隔號.
   /// - 傳回的名稱, 分隔號為 '/'
   static std::string ExtractPathName(StrView fname);

   /// 從檔名 fname 之中, 抽取出檔案名稱的部分(移除路徑).
   static std::string ExtractFileName(StrView fname);
   static void SplitPathFileName(StrView fname, std::string& path, std::string& name);

   /// 建立完整路徑(包含次路徑).
   /// 若路徑已存在則會傳回成功.
   static Result MakePathTree(StrView pathName, mode_t pathAccessMode = DefaultPathAccessMode_);

   /// 取得完整路徑(含檔案)名稱.
   /// 在 Linux 上: fname 必須存在才能正確取得, 否則直接傳回 fname.
   static std::string GetFullName(StrView fname);

   /// 合併 path.
   /// - newPath.Get1st()=='/' 則不考慮 curPath, 直接返回 newPath.
   /// - 如果要移除合併後路徑內無效的 "./" or "../" 則應使用 NormalizeFileName();
   /// - 返回前, 移除尾端的 '/'
   static std::string MergePath(StrView curPath, StrView newPath);

   /// 合併 path, 不考慮 appPath 開頭是否有 "/", "..".
   /// - 返回前, 移除尾端的 '/'
   static std::string AppendPath(StrView curPath, StrView appPath);

   /// 檢查 fname 是否有前置路徑: 開頭是否為 "./" or "../" or "/".
   /// \retval true  fname = "./filename" or "../filename" or "/filename"
   /// \retval false fname = "filename" or "path/filename"
   static bool HasPrefixPath(StrView fname);

   using StrList = std::vector<StrView>;
   /// 把 fname 拆解成路徑陣列, 用 '/' 分隔.
   /// - 移除多餘的 "./" or "/"
   /// - 遇到 "../" 則移除前一個路徑, 如果沒有前一個路徑, 則會返回 empty() 且 fname.begin() == "../fileanme"
   /// - 返回時如果 !fname.IsNullOrEmpty() 則表示有錯, fname.begin() 為錯誤發生的位置.
   /// - 返回值不包含 '/', 但如果有用括號、引號包含的字串, 則不會進行拆解, 例: '123/456', "123/456", {123/456}
   static StrList SplitPathList(StrView& fname);

   /// 把 plist 的內容合併.
   /// 返回值 **頭尾不包含額外的'/'**
   static std::string MergePathList(const StrList& plist);

   /// 把 fname 正規化.
   /// - 移除多餘的 "./" or "/"
   /// - 若有包含 "../" 則移到前一個路徑, 如果沒有前一個路徑, 則會返回 empty() 且 fname.begin() == "../fileanme"
   /// - 返回時如果 !fname.IsNullOrEmpty() 則表示有錯, fname.begin() 為錯誤發生的位置.
   /// - 返回值 **頭尾不包含'/'**
   static std::string NormalizeFileName(StrView& fname) {
      return MergePathList(SplitPathList(fname));
   }
};

} // namespaces
#endif//__fon9_FilePath_hpp__
