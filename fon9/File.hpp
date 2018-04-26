/// \file fon9/File.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_File_hpp__
#define __fon9_File_hpp__
#include "fon9/Fdr.hpp"
#include "fon9/Outcome.hpp"
#include "fon9/TimeStamp.hpp"

namespace fon9 {

class fon9_API DcQueueList;

/// \ingroup Misc
/// File 的開啟模式
enum class FileMode {
   /// 開檔後允許讀.
   Read = 0x0001,
   /// 開檔後允許寫.
   Write = 0x0002,
   /// 開啟後支援由 OS 提供的 Append 寫入方式.
   Append = 0x0004,
   /// - Windows: FILE_FLAG_WRITE_THROUGH
   /// - Linux: F_DSYNC or F_SYNC
   WriteThrough = 0x0008,

   /// 若檔案不存在, 則建立新檔案.
   OpenAlways = 0x0010,
   /// 若路徑不存在, 則建立路徑後開啟檔案 (可以不用再加 OpenAlways).
   CreatePath = 0x0020,
   /// 必須是此次開檔時建立的新檔, 若開檔前檔案已存在則會失敗: file_exists
   MustNew = 0x0040,
   /// 開檔後把檔案大小清為0.
   Trunc = 0x0080,

   /// 開檔後禁止其他人用讀取模式開檔, 直到檔案關閉為止.
   /// 若已有其他人用讀取模式開檔, 則此次開檔會失敗.
   DenyRead = 0x0100,
   /// 開檔後禁止其他人用寫入模式開檔, 直到檔案關閉為止.
   /// 若已有其他人用寫入模式開檔, 則此次開檔會失敗.
   DenyWrite = 0x0200,
};
fon9_ENABLE_ENUM_BITWISE_OP(FileMode);

/// \ingroup Misc
/// value = "Read + Write + ..." 中間使用 '+' 連結多個 FileMode.
/// - '+' 前後可以有空白, 名稱與 FileMode 相同, 區分大小寫.
/// - 也可用縮寫設定(例: "rc" = "Read + CreatePath"):
///   - r = Read
///   - w = Write
///   - a = Append
///   - o = OpenAlways
///   - c = CreatePath
///   - n = MustNew
fon9_API FileMode StrToFileMode(StrView modes);

/// \return 字串輸出一律使用長名稱, 選項之間使用 '+' 連接.
fon9_API void FileModeToStrAppend(FileMode fm, std::string& out);

inline std::string FileModeToStr(FileMode fm) {
   std::string out;
   FileModeToStrAppend(fm, out);
   return out;
}

fon9_WARN_DISABLE_PADDING;
/// \ingroup Misc
/// 一般檔案處理.
/// - 不支援 Seek, 一律在讀寫時指定位置, 或使用 Append 模式開檔、寫檔.
/// - 不額外提供 txt 讀寫函式, 如果有需要, fopen()、fgets() 可能會是更好的選擇.
class fon9_API File {
   fon9_NON_COPYABLE(File);

   /// 呼叫 Open() 時的 fname.
   std::string OpenName_;

protected:
   FdrAuto     Fdr_;
   FileMode    OpenMode_{};

public:
   using PosType = uint64_t;
   using SizeType = uint64_t;

   /// 結果型別: 讀寫結果、取得or設定檔案大小結果.
   using Result = Outcome<PosType>;

   File() = default;

   /// 建構時僅設定 OpenMode_, 在稍後開檔時可以使用: `fd.Open(fname, fd.GetOpenMode());`.
   /// 已預先知道開檔模式, 但尚未到達開檔時機, 可先將 openMode 保留起來,
   /// 等開檔時機到時就可以直接拿來用了!
   File(FileMode openMode) : OpenMode_{openMode} {
   }

   ~File() = default;

   File(File&&) = default;
   File& operator=(File&&) = default;

   /// 禁止複製, 使用 File::Duplicate() const; 更能表達實際含意.
   File Duplicate() const;

   File(std::string fname, FileMode fmode) {
      this->Open(std::move(fname), fmode);
   }
   bool IsOpened() const {
      return this->Fdr_.IsReadyFD();
   }

   /// 一律先關檔, 再處理開檔作業, NOT thread-safe.
   /// 不論成功或失敗, OpenName、OpenMode 都會更新.
   /// \param fname  檔案路徑及檔名.
   /// \param fmode  開檔模式.
   /// \retval Result{0}    成功.
   /// \retval Result{ErrC} 失敗.
   Result Open(std::string fname, FileMode fmode);

   void Close() {
      this->Fdr_.Close();
   }

   /// 取得呼叫 Open() 時指定的 fname.
   /// **不提供** 取得完整檔名的功能, 若有需要:
   /// - 可在 Open() 時, 使用 FilePath::GetFullName() 取得完整檔名,
   /// - 為何要在 Open() 之前取得完整檔名? => 避免在程式運行過程改變了現在路徑.
   const std::string& GetOpenName() const {
      return this->OpenName_;
   }

   FileMode GetOpenMode() const {
      return this->OpenMode_;
   };

   /// 寫入一個區塊.
   /// - 只要不在其他 thread 執行 Open(), Close(), 則可確保正確寫入.
   ///
   /// \retval 成功 實際寫入的bytes數量.
   /// \retval 其他系統錯誤
   Result Write(PosType offset, const void* buf, size_t count);

   /// 寫入一個字串.
   /// \copydetails Write()
   Result Write(PosType offset, StrView buf) {
      return this->Write(offset, buf.begin(), buf.size());
   }
   Result Write(PosType offset, DcQueueList& outbuf);

   /// 將資料寫到檔尾.
   /// - 如果使用 FileMode::Append 開啟, 則一定要用此函式寫檔.
   /// - 如果開檔沒使用 FileMode::Append, 若有其他thread同時Append(), 則不保證正確寫入.
   ///
   /// \retval 成功 實際寫入的bytes數量.
   Result Append(const void* buf, size_t count);

   /// 將字串寫到檔尾.
   /// \copydetails Append()
   Result Append(StrView buf) {
      return this->Append(buf.begin(), buf.size());
   }
   /// 如果 outbuf 包含多個 block, 並且可能在多個 threads 同時呼叫: 應考慮使用 FileAppender.
   Result Append(DcQueueList& outbuf);

   /// 強迫寫入儲存媒體.
   /// 可參閱:
   /// - Linux: fdatasync
   /// - Windows: FlushFileBuffers
   void Sync();

   /// 讀取一個區塊.
   /// 只要不在其他 thread 執行 Open(), Close(), 則可確保正確讀取.
   /// \retval 成功  實際讀取的bytes數量.
   Result Read(PosType offset, void* buf, SizeType count);

   /// 取得檔案大小.
   /// \retval 成功 檔案大小 bytes.
   /// \retval 失敗 尚未開檔, 或無法取得檔案大小.
   Result GetFileSize() const;

   /// 設定檔案大小.
   /// \retval 成功 新的檔案大小 bytes.
   /// \retval 失敗 尚未開檔, 或無法設定檔案大小.
   Result SetFileSize(PosType newFileSize);

   /// 取得檔案最後異動時間.
   TimeStamp GetLastModifyTime() const;

   Fdr::fdr_t ReleaseFD() {
      return this->Fdr_.ReleaseFD();
   }
};
fon9_WARN_POP;

} // namespaces
#endif//__fon9_File_hpp__
