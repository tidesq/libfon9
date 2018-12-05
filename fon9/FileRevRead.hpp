/// \file fon9/FileRevRead.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_FileRevRead_hpp__
#define __fon9_FileRevRead_hpp__
#include "fon9/File.hpp"

namespace fon9 {

enum class LoopControl {
   Continue,
   Break,
};

/// \ingroup Misc
/// 依序從後往前讀取檔案區塊.
/// - 第一次讀取: 尾端不足 blockSize 的部分.
/// - 之後才繼續往檔頭方向讀取 blockSize.
/// - blockSize   建議為 4K 的整數倍.
class fon9_API FileRevRead {
protected:
   virtual ~FileRevRead();

public:
   /// 開始從尾端往檔頭方向讀取:
   /// - 如果檔案大小不是 blockSize 整數倍:
   ///   - 第一次讀取檔尾不足 blockSize 的部分.
   /// - 透過 this->OnFileBlock(rdsz) 通知.
   /// - 終止讀取條件:
   ///   - this->OnFileBlock(rdsz) 返回 LoopControl::Break;
   ///   - 已讀完檔案.
   ///   - 讀檔發生錯誤.
   ///
   /// \retval success 最後一次讀取區塊的檔案位置.
   File::Result Start(File& fd, void* blockBuffer, size_t blockSize);

   File::PosType GetBlockPos() const {
      return this->BlockPos_;
   }

private:
   File::PosType BlockPos_;
   virtual LoopControl OnFileBlock(size_t rdsz) = 0;
};

/// \ingroup Misc
/// 包含緩衝區的 FileRevRead.
/// 為了降低分配記憶體的負擔, 此物件內包含一個固定大小(kBlockSize + maxMessageBufferSize)的緩衝區.
template <size_t blockSize = 1024 * 4, size_t maxMessageBufferSize = blockSize>
class FileRevReadBuffer : protected FileRevRead {
   using base = FileRevRead;
protected:
   char  BlockBuffer_[blockSize + maxMessageBufferSize];
public:
   enum : size_t {
      kBlockSize = blockSize,
      kMaxMessageBufferSize = maxMessageBufferSize,
   };
   File::Result Start(File& fd) {
      return base::Start(fd, this->BlockBuffer_, kBlockSize);
   }
   char* GetBlockBuffer() {
      return this->BlockBuffer_;
   }
   using base::GetBlockPos;
};

/// \ingroup Misc
/// 透過 FileRevRead 機制, 從檔案尾端往前尋找特定字元.
class fon9_API FileRevSearch {
   fon9_NON_COPY_NON_MOVE(FileRevSearch);
protected:
   /// 檔尾 or 上次找到的指定字元位置.
   char* DataEnd_{nullptr};

   /// 找到指定的字元時的通知.
   /// - *pbeg == 要求尋找的的字元.
   /// - pend = 檔尾 or 上次找到的指定字元位置 or (pbeg + kMaxMessageBufferSize).
   virtual LoopControl OnFoundChar(char* pbeg, char* pend) = 0;

   virtual ~FileRevSearch();

public:
   const size_t   BlockSize_;
   const size_t   MaxMessageBufferSize_;
   char* const    BlockBufferPtr_;

   FileRevSearch(size_t blockSize, size_t maxMessageBufferSize, void* blockBuffer)
      : BlockSize_{blockSize}
      , MaxMessageBufferSize_{maxMessageBufferSize}
      , BlockBufferPtr_{reinterpret_cast<char*>(blockBuffer)} {
   }
   
   template <size_t blockSize, size_t maxMessageBufferSize>
   FileRevSearch(FileRevReadBuffer<blockSize, maxMessageBufferSize>& reader) 
      : FileRevSearch{reader.kBlockSize, reader.kMaxMessageBuferSize, reader.GetBlockBuffer()} {
   }

   /// - 若有找到 ch, 則會透過 this->OnFoundChar() 通知.
   /// - 若尋找完畢, 還有剩餘未處理的資料,
   ///   則會搬移到 BlockBuffer_ + BlockSize_, 等候下一個 block 載入後使用.
   ///   \code
   ///      ch = '^'
   ///      block 1: abcdefgh^........^.........
   ///      block 2: 1234567890^.............^..abcdefgh
   ///               |__ 此次 block 的內容______/|______/
   ///                                          |
   ///                                          +--上次沒用到的資料
   ///   \endcode
   /// - 若已到檔頭(fpos==0), 則此時 [BlockBuffer_ .. DataEnd_) = 最後未處理的資料.
   ///
   /// \retval LoopControl::Break   OnFoundChar() 傳回 LoopControl::Break, 中斷搜尋.
   LoopControl RevSearchBlock(File::PosType fpos, char ch, size_t rdsz);
};

} // namespace
#endif//__fon9_FileRevRead_hpp__
