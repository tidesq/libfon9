/// \file fon9/InnFile.cpp
/// \author fonwinz@gmail.com
#include "fon9/InnFile.hpp"
#include "fon9/Endian.hpp"
#include "fon9/buffer/FwdBufferList.hpp"

namespace fon9 {

static const char kInnHeaderStr[20] = "fon9.inn.00001\n";
enum : InnFile::SizeT {
   kBlockSizeAlign = 16,

   kOffset_BlockCount = 0,
   kOffset_RoomType = kOffset_BlockCount + sizeof(InnFile::SizeT),
   kOffset_DataSize = kOffset_RoomType + sizeof(InnRoomType),
   kOffset_DataBegin = kOffset_DataSize + sizeof(InnFile::SizeT),
};
static_assert(kOffset_DataBegin == InnFile::kRoomHeaderSize, "RoomHeaderSize or kOffset_DataBegin ERROR!");

//--------------------------------------------------------------------------//

InnFile::InnFile() {
}
InnFile::~InnFile() {
}

InnFile::OpenResult InnFile::Open(OpenArgs& args) {
   if (this->Storage_.IsOpened())
      return InnFile::OpenResult{std::errc::already_connected};
   File        fd;
   OpenResult  res = fd.Open(args.FileName_, args.OpenMode_);
   if (!res)
      return res;
   byte  header[kInnHeaderSize];
   res = fd.Read(0, header, sizeof(header));
   if (!res)
      return res;
   if (fon9_UNLIKELY(res.GetResult() == 0)) {
      // new file, write header.
      memcpy(header, kInnHeaderStr, sizeof(kInnHeaderStr));
      byte* pdat = header + sizeof(kInnHeaderStr);
      PutBigEndian(pdat, this->HeaderSize_ = kInnHeaderSize);
      pdat += sizeof(SizeT);

      PutBigEndian(pdat, this->BlockSize_ = args.BlockSize_);
      pdat += sizeof(SizeT);

      memset(pdat, 0, static_cast<size_t>(header + sizeof(header) - pdat));

      res = fd.Write(0, header, this->HeaderSize_);
      if (!res) {
         fd.SetFileSize(0);
         return res;
      }
      this->Storage_ = std::move(fd);
      this->FileSize_ = this->HeaderSize_;
      return OpenResult{0};
   }
   if (res.GetResult() != sizeof(header))
      return InnFile::OpenResult{std::errc::bad_message};
   if (memcmp(header, kInnHeaderStr, sizeof(kInnHeaderStr)) != 0)
      return InnFile::OpenResult{std::errc::bad_message};
   const byte* pdat = header + sizeof(kInnHeaderStr);
   this->HeaderSize_ = GetBigEndian<SizeT>(pdat);
   pdat += sizeof(SizeT);
   this->BlockSize_ = GetBigEndian<SizeT>(pdat);

   static_assert(kInnHeaderSize % kBlockSizeAlign == 0, "Bad 'kInnHeaderSize'");
   if (this->BlockSize_ < kBlockSizeAlign
       || (this->BlockSize_ % kBlockSizeAlign != 0)
       || (this->HeaderSize_ < kInnHeaderSize)
       || (this->HeaderSize_ % kBlockSizeAlign != 0))
      return InnFile::OpenResult{std::errc::bad_message};
   res = fd.GetFileSize();
   if (!res)
      return res;
   if ((res.GetResult() - this->HeaderSize_) % this->BlockSize_ != 0)
      return InnFile::OpenResult{std::errc::bad_message};
   args.BlockSize_ = this->BlockSize_;
   this->Storage_ = std::move(fd);
   this->FileSize_ = res.GetResult();
   return OpenResult{(res.GetResult() - this->HeaderSize_) / this->BlockSize_};
}

//--------------------------------------------------------------------------//

static inline void CheckIoSize(const File::Result& res, File::SizeType sz, const char* exResError, const char* exSizeError) {
   if (fon9_UNLIKELY(!res))
      Raise<InnFileError>(res.GetError(), exResError);
   if (fon9_UNLIKELY(res.GetResult() != sz))
      Raise<InnFileError>(std::errc::bad_message, exSizeError);
}

void InnFile::CheckRoomPos(RoomPosT pos, const char* exNotOpen, const char* exBadPos) const {
   if (fon9_UNLIKELY(!this->Storage_.IsOpened()))
      Raise<InnFileError>(std::errc::bad_file_descriptor, exNotOpen);
   if (fon9_UNLIKELY(!this->IsGoodRoomPos(pos)))
      Raise<InnRoomPosError>(exBadPos);
}

//--------------------------------------------------------------------------//

void InnFile::RoomHeaderToRoomKeyInfo(const byte* roomHeader, RoomKey::Info& roomKeyInfo) {
   roomKeyInfo.RoomSize_ = this->CalcRoomSize(GetBigEndian<SizeT>(roomHeader + kOffset_BlockCount));
   roomKeyInfo.CurrentRoomType_ = roomKeyInfo.PendingRoomType_ = GetBigEndian<InnRoomType>(roomHeader + kOffset_RoomType);
   roomKeyInfo.DataSize_ = GetBigEndian<SizeT>(roomHeader + kOffset_DataSize);
}

InnFile::RoomKey InnFile::MakeRoomKey(RoomPosT roomPos, void* exRoomHeader, SizeT exRoomHeaderSize) {
   if (roomPos == 0)
      roomPos = this->HeaderSize_;
   this->CheckRoomPos(roomPos, "InnFile.MakeRoomKey: not opened.", "InnFile.MakeRoomKey: bad roomPos.");
   if (roomPos >= this->FileSize_) { // 已到尾端(EOF).
      RoomKey::Info info;
      ZeroStruct(info);
      return RoomKey{info};
   }
   byte   roomHeader[kRoomHeaderSize + 128];
   SizeT  sz = kRoomHeaderSize + exRoomHeaderSize;
   if (sz > sizeof(roomHeader))
      sz = kRoomHeaderSize;
   auto res = this->Storage_.Read(roomPos, roomHeader, sz);
   CheckIoSize(res, sz,
               "InnFile.MakeRoomKey: read RoomHeader error.",
               "InnFile.MakeRoomKey: read RoomHeader error size.");

   RoomKey::Info info;
   info.RoomPos_ = roomPos;
   this->RoomHeaderToRoomKeyInfo(roomHeader, info);
   //因為在 MakeRoomKey() 時, 還不確定 exRoomHeader 是否為期望的(可能需要判斷 RoomType).
   //所以此處不判斷 DataSize_ 是否正確, 交給返回後的使用者自行處理.
   //if (info.DataSize_ < exRoomHeaderSize)
   //   Raise<InnFileError>(std::errc::bad_message, "InnFile.MakeRoomKey: DataSize < exRoomHeaderSize.");
   if (info.DataSize_ > info.RoomSize_ || info.RoomSize_ <= 0
       || info.RoomPos_ + info.RoomSize_ + kRoomHeaderSize > this->FileSize_)
      Raise<InnFileError>(std::errc::bad_message, "InnFile.MakeRoomKey: bad RoomHeader.");

   if (sz == kRoomHeaderSize) {
      if (exRoomHeaderSize) {
         res = this->Storage_.Read(roomPos + sz, exRoomHeader, exRoomHeaderSize);
         CheckIoSize(res, exRoomHeaderSize,
                     "InnFile.MakeRoomKey: read room ExHeader error.",
                     "InnFile.MakeRoomKey: read room ExHeader error size.");
      }
   }
   else if (exRoomHeaderSize)
      memcpy(exRoomHeader, roomHeader + kRoomHeaderSize, exRoomHeaderSize);
   return RoomKey{info};
}

InnFile::RoomKey InnFile::MakeNextRoomKey(const RoomKey& roomKey, void* exRoomHeader, SizeT exRoomHeaderSize) {
   if (!roomKey)
      Raise<InnRoomPosError>("InnFile.MakeNextRoomKey: invalid current RoomKey.");
   return MakeRoomKey(roomKey.GetNextRoomPos(), exRoomHeader, exRoomHeaderSize);
}

InnFile::RoomKey InnFile::MakeNewRoom(InnRoomType roomType, SizeT size) {
   if (fon9_UNLIKELY(!this->Storage_.IsOpened()))
      Raise<InnFileError>(std::errc::bad_file_descriptor, "InnFile.MakeNewRoom: not opened.");

   RoomKey::Info info;
   info.RoomPos_ = this->FileSize_;
   SizeT blockCount = static_cast<SizeT>(static_cast<size_t>(size) + kRoomHeaderSize + this->BlockSize_ - 1) / this->BlockSize_;
   info.CurrentRoomType_ = info.PendingRoomType_ = roomType;
   info.DataSize_ = 0;
   info.RoomSize_ = this->CalcRoomSize(blockCount);

   byte   roomHeader[kRoomHeaderSize];
   PutBigEndian(roomHeader + kOffset_BlockCount, blockCount);
   PutBigEndian(roomHeader + kOffset_RoomType, info.CurrentRoomType_);
   PutBigEndian(roomHeader + kOffset_DataSize, info.DataSize_);

   auto res = this->Storage_.SetFileSize(this->FileSize_ = info.RoomPos_ + blockCount * this->BlockSize_);
   const char* exWhat;
   if (!res) {
      exWhat = "InnFile.MakeNewRoom: build room error.";
__RETURN_RESTORE_FILE:
      this->Storage_.SetFileSize(this->FileSize_ = info.RoomPos_);
      Raise<InnFileError>(res.GetError(), exWhat);
   }

   res = this->Storage_.Write(info.RoomPos_, roomHeader, kRoomHeaderSize);
   if (!res) {
      exWhat = "InnFile.MakeNewRoom: write RoomHeader error.";
      goto __RETURN_RESTORE_FILE;
   }
   if (res.GetResult() != kRoomHeaderSize) {
      res = std::errc::bad_message;
      exWhat = "InnFile.MakeNewRoom: write RoomHeader error size.";
      goto __RETURN_RESTORE_FILE;
   }
   return RoomKey{info};
}

void InnFile::UpdateRoomHeader(RoomKey::Info& info, SizeT newsz, const char* exResError, const char* exSizeError) {
   if (info.CurrentRoomType_ == info.PendingRoomType_) {
      if (info.DataSize_ != newsz) {
         PutBigEndian(&newsz, info.DataSize_ = newsz);
         CheckIoSize(this->Storage_.Write(info.RoomPos_ + kOffset_DataSize, &newsz, sizeof(newsz)),
                     sizeof(newsz), exResError, exSizeError);
      }
   }
   else {
      static_assert(kOffset_DataSize == kOffset_RoomType + 1, "RoomHeader format error.");
      byte roomHeader[sizeof(InnRoomType) + sizeof(SizeT)];
      PutBigEndian(roomHeader, info.CurrentRoomType_ = info.PendingRoomType_);
      size_t wrsz;
      if (info.DataSize_ == newsz)
         wrsz = 1;
      else {
         wrsz = sizeof(roomHeader);
         PutBigEndian(roomHeader + sizeof(info.CurrentRoomType_), info.DataSize_ = newsz);
      }
      CheckIoSize(this->Storage_.Write(info.RoomPos_ + kOffset_RoomType, roomHeader, wrsz),
                  wrsz, exResError, exSizeError);
   }
}

void InnFile::ClearRoom(RoomKey& roomKey, SizeT requiredSize) {
   this->CheckRoomPos(roomKey.GetRoomPos(), "InnFile.ClearRoom: not opened.", "InnFile.ClearRoom: bad roomPos.");
   if (roomKey.GetRoomSize() < requiredSize)
      Raise<InnRoomSizeError>("InnFile.ClearRoom: requiredSize too big.");
   this->UpdateRoomHeader(roomKey.Info_, 0,
                          "InnFile.ClearRoom: update RoomHeader error.",
                          "InnFile.ClearRoom: update RoomHeader error size.");
}

void InnFile::Reduce(RoomKey& roomKey, SizeT newDataSize, const void* exRoomHeader, SizeT exRoomHeaderSize) {
   this->CheckRoomPos(roomKey.Info_.RoomPos_, "InnFile.Reduce: not opened.", "InnFile.Reduce: bad roomPos.");
   if (exRoomHeaderSize > newDataSize)
      Raise<InnFileError>(std::errc::invalid_argument, "InnFile.Reduce: exRoomHeaderSize > newDataSize");
   if (roomKey.GetDataSize() < newDataSize) // Reduce() 資料量可以縮減(或不變), 不能增加.
      Raise<InnFileError>(std::errc::invalid_argument, "InnFile.Reduce: oldDataSize < newDataSize");

   if (exRoomHeaderSize <= 0) {
      this->UpdateRoomHeader(roomKey.Info_, newDataSize,
                             "InnFile.Reduce: update RoomHeader error.",
                             "InnFile.Reduce: update RoomHeader error size.");
      return;
   }
   if (roomKey.GetCurrentRoomType() != roomKey.GetPendingRoomType() || roomKey.GetDataSize() != newDataSize) {
      char roomHeader[sizeof(InnRoomType) + sizeof(newDataSize) + 128];
      PutBigEndian(roomHeader, roomKey.Info_.CurrentRoomType_ = roomKey.Info_.PendingRoomType_);
      PutBigEndian(roomHeader + sizeof(InnRoomType), roomKey.Info_.DataSize_ = newDataSize);
      static_assert(kOffset_DataSize + sizeof(newDataSize) == kOffset_DataBegin, "RoomHeader format error.");
      SizeT wrsz = sizeof(InnRoomType) + sizeof(newDataSize);
      if (0 < exRoomHeaderSize && exRoomHeaderSize <= sizeof(roomHeader) - wrsz) {
         memcpy(roomHeader + wrsz, exRoomHeader, exRoomHeaderSize);
         wrsz += exRoomHeaderSize;
         exRoomHeaderSize = 0;
      }
      CheckIoSize(this->Storage_.Write(roomKey.Info_.RoomPos_ + kOffset_RoomType, roomHeader, wrsz), wrsz,
                  "InnFile.Reduce: update RoomHeader2 error.",
                  "InnFile.Reduce: update RoomHeader2 error size.");
   }
   if (exRoomHeaderSize > 0) {
      CheckIoSize(this->Storage_.Write(roomKey.Info_.RoomPos_ + kRoomHeaderSize, exRoomHeader, exRoomHeaderSize),
                  exRoomHeaderSize,
                  "InnFile.Reduce: update room ExHeader error.",
                  "InnFile.Reduce: update room ExHeader error size.");
   }
}

//--------------------------------------------------------------------------//

InnFile::RoomPosT InnFile::CheckReadArgs(const RoomKey& roomKey, SizeT offset, SizeT& size) {
   this->CheckRoomPos(roomKey.Info_.RoomPos_, "InnFile.Read: not opened.", "InnFile.Read: bad roomPos.");
   if (offset >= roomKey.GetDataSize() || size <= 0)
      return 0;
   if (offset + size > roomKey.GetDataSize())
      size = roomKey.GetDataSize() - offset;
   return roomKey.Info_.RoomPos_ + kRoomHeaderSize + offset;
}

static InnFile::SizeT ReadToNode(File& fd, FwdBufferNode* back, InnFile::RoomPosT pos, InnFile::SizeT size) {
   CheckIoSize(fd.Read(pos, back->GetDataEnd(), size), size,
               "InnFile.Read: read error.",
               "InnFile.Read: read error size.");
   back->SetDataEnd(back->GetDataEnd() + size);
   return size;
}

InnFile::SizeT InnFile::Read(const RoomKey& roomKey, SizeT offset, SizeT size, BufferList& buf) {
   RoomPosT pos = this->CheckReadArgs(roomKey, offset, size);
   if (pos == 0)
      return 0;
   File::Result res;
   if (FwdBufferNode* back = FwdBufferNode::CastFrom(buf.back())) {
      if (back->GetRemainSize() >= size)
         return ReadToNode(this->Storage_, back, pos, size);
   }
   FwdBufferNode* back = FwdBufferNode::Alloc(size);
   BufferList     tempbuf; // for auto free back.
   tempbuf.push_back(back);
   ReadToNode(this->Storage_, back, pos, size);
   buf.push_back(tempbuf.ReleaseList());
   return size;
}
InnFile::SizeT InnFile::Read(const RoomKey& roomKey, SizeT offset, SizeT size, void* buf) {
   if (RoomPosT pos = this->CheckReadArgs(roomKey, offset, size)) {
      CheckIoSize(this->Storage_.Read(pos, buf, size), size,
                  "InnFile.Read: read error.",
                  "InnFile.Read: read error size.");
      return size;
   }
   return 0;
}
InnFile::SizeT InnFile::ReadAll(const RoomKey& roomKey, void* buf, SizeT bufsz) {
   if (bufsz >= roomKey.GetDataSize())
      return this->Read(roomKey, 0, roomKey.GetDataSize(), buf);
   Raise<InnFileError>(std::errc::invalid_argument, "InnFile.ReadAll: buffer too small.");
}

//--------------------------------------------------------------------------//

void WriteRoom(File& fd, File::PosType pos, size_t wrsz, DcQueue& buf, const char* exResError, const char* exSizeError) {
   for (;;) {
      auto blk = buf.PeekCurrBlock();
      if (blk.second > wrsz)
         blk.second = wrsz;
      CheckIoSize(fd.Write(pos, blk.first, blk.second), blk.second, exResError, exSizeError);
      buf.PopConsumed(blk.second);
      if ((wrsz -= blk.second) <= 0)
         break;
      pos += blk.second;
   }
}

InnFile::SizeT InnFile::Rewrite(RoomKey& roomKey, DcQueue& buf) {
   RoomPosT pos = roomKey.Info_.RoomPos_;
   this->CheckRoomPos(pos, "InnFile.Rewrite: not opened.", "InnFile.Rewrite: bad roomPos.");
   size_t bufsz = buf.CalcSize();
   if (bufsz > roomKey.GetRoomSize())
      Raise<InnRoomSizeError>("InnFile.Rewrite: request size > RoomSize.");
   this->UpdateRoomHeader(roomKey.Info_, static_cast<SizeT>(bufsz),
                          "InnFile.Rewrite: update RoomHeader error.",
                          "InnFile.Rewrite: update RoomHeader error size.");
   WriteRoom(this->Storage_, pos + kRoomHeaderSize, bufsz, buf,
             "InnFile.Rewrite: write error.",
             "InnFile.Rewrite: write error size.");
   return roomKey.Info_.DataSize_;
}

InnFile::SizeT InnFile::Write(RoomKey& roomKey, const SizeT offset, const SizeT size, DcQueue& buf) {
   RoomPosT pos = roomKey.Info_.RoomPos_;
   this->CheckRoomPos(pos, "InnFile.Write: not opened.", "InnFile.Write: bad roomPos.");
   if (offset > roomKey.GetDataSize())
      Raise<InnRoomSizeError>("InnFile.Write: bad offset.");
   if (static_cast<size_t>(offset) + size > roomKey.GetRoomSize())
      Raise<InnRoomSizeError>("InnFile.Write: bad request size.");

   if (size <= 0)
      return 0;
   if (size > buf.CalcSize())
      Raise<InnFileError>(std::errc::invalid_argument, "InnFile.Write: request size > buffer size.");
   WriteRoom(this->Storage_, pos + kRoomHeaderSize + offset, size, buf,
             "InnFile.Write: write error.",
             "InnFile.Write: write error size.");
   SizeT newsz = offset + size;
   if (newsz < roomKey.Info_.DataSize_)
      newsz = roomKey.Info_.DataSize_;
   this->UpdateRoomHeader(roomKey.Info_, newsz,
                          "InnFile.Write: update RoomHeader error.",
                          "InnFile.Write: update RoomHeader error size.");
   return size;
}

} // namespaces
