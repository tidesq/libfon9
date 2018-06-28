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
   roomKeyInfo.RoomType_ = GetBigEndian<InnRoomType>(roomHeader + kOffset_RoomType);
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
               "InnFile.MakeRoomKey: read room header error.",
               "InnFile.MakeRoomKey: read room header error size.");

   RoomKey::Info info;
   info.RoomPos_ = roomPos;
   this->RoomHeaderToRoomKeyInfo(roomHeader, info);
   if (info.DataSize_ < exRoomHeaderSize)
      Raise<InnFileError>(std::errc::bad_message, "InnFile.MakeRoomKey: DataSize < exRoomHeaderSize.");
   if (info.DataSize_ > info.RoomSize_ || info.RoomSize_ <= 0
       || info.RoomPos_ + info.RoomSize_ + kRoomHeaderSize > this->FileSize_)
      Raise<InnFileError>(std::errc::bad_message, "InnFile.MakeRoomKey: bad room header.");

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
   return MakeRoomKey(roomKey.Info_.RoomPos_ + roomKey.Info_.RoomSize_ + kRoomHeaderSize,
                      exRoomHeader, exRoomHeaderSize);
}

InnFile::RoomKey InnFile::MakeNewRoom(InnRoomType roomType, SizeT size) {
   if (fon9_UNLIKELY(!this->Storage_.IsOpened()))
      Raise<InnFileError>(std::errc::bad_file_descriptor, "InnFile.MakeNewRoom: not opened.");

   RoomKey::Info info;
   info.RoomPos_ = this->FileSize_;
   SizeT blockCount = static_cast<SizeT>(static_cast<size_t>(size) + kRoomHeaderSize + this->BlockSize_ - 1) / this->BlockSize_;
   info.RoomType_ = roomType;
   info.DataSize_ = 0;
   info.RoomSize_ = this->CalcRoomSize(blockCount);

   byte   roomHeader[kRoomHeaderSize];
   PutBigEndian(roomHeader + kOffset_BlockCount, blockCount);
   PutBigEndian(roomHeader + kOffset_RoomType, info.RoomType_);
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
      exWhat = "InnFile.MakeNewRoom: write room header error.";
      goto __RETURN_RESTORE_FILE;
   }
   if (res.GetResult() != kRoomHeaderSize) {
      res = std::errc::bad_message;
      exWhat = "InnFile.MakeNewRoom: write room header error size.";
      goto __RETURN_RESTORE_FILE;
   }
   return RoomKey{info};
}

InnFile::RoomKey InnFile::ClearRoom(RoomPosT roomPos, InnRoomType roomType, SizeT size) {
   this->CheckRoomPos(roomPos, "InnFile.ClearRoom: not opened.", "InnFile.ClearRoom: bad roomPos.");
   SizeT blockCount;
   auto  res = this->Storage_.Read(roomPos, &blockCount, sizeof(blockCount));
   blockCount = GetBigEndian<SizeT>(&blockCount);
   CheckIoSize(res, sizeof(blockCount),
               "InnFile.ClearRoom: read room header error.",
               "InnFile.ClearRoom: read room header error size.");

   RoomKey::Info info;
   info.RoomSize_ = this->CalcRoomSize(blockCount);
   if (info.RoomSize_ < size)
      Raise<InnRoomSizeError>("InnFile.ClearRoom: room size error.");

   static_assert(kOffset_RoomType == sizeof(blockCount) && kOffset_DataSize == kOffset_RoomType + 1,
                 "room header format error.");
   byte clearRoomHeader[kRoomHeaderSize - sizeof(blockCount)];
   clearRoomHeader[0] = info.RoomType_ = roomType;
   PutBigEndian(clearRoomHeader + sizeof(roomType), info.DataSize_ = 0);
   res = this->Storage_.Write((info.RoomPos_ = roomPos) + kOffset_RoomType, clearRoomHeader, sizeof(clearRoomHeader));
   CheckIoSize(res, sizeof(clearRoomHeader),
               "InnFile.ClearRoom: update room header error.",
               "InnFile.ClearRoom: update room header error size.");

   return RoomKey{info};
}

void InnFile::Reduce(RoomKey& roomKey, SizeT newDataSize, const void* exRoomHeader, SizeT exRoomHeaderSize) {
   this->CheckRoomPos(roomKey.Info_.RoomPos_, "InnFile.Reduce: not opened.", "InnFile.Reduce: bad roomPos.");
   if (exRoomHeaderSize > newDataSize)
      Raise<InnFileError>(std::errc::invalid_argument, "InnFile.Reduce: exRoomHeaderSize > newDataSize");
   if (roomKey.GetDataSize() < newDataSize) // Reduce() 資料量可以縮減(或不變), 不能增加.
      Raise<InnFileError>(std::errc::invalid_argument, "InnFile.Reduce: oldDataSize < newDataSize");

   if (roomKey.GetDataSize() != newDataSize) {
      char roomHeader[sizeof(newDataSize) + 128];
      PutBigEndian(roomHeader, roomKey.Info_.DataSize_ = newDataSize);
      SizeT wrsz = sizeof(newDataSize);
      if (0 < exRoomHeaderSize && exRoomHeaderSize <= sizeof(roomHeader) - sizeof(newDataSize)) {
         memcpy(roomHeader + sizeof(newDataSize), exRoomHeader, exRoomHeaderSize);
         wrsz += exRoomHeaderSize;
         exRoomHeaderSize = 0;
      }
      CheckIoSize(this->Storage_.Write(roomKey.Info_.RoomPos_ + kOffset_DataSize, roomHeader, wrsz), wrsz,
                  "InnFile.Reduce: update room DataSize error.",
                  "InnFile.Reduce: update room DataSize error size.");
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

void InnFile::UpdateDataSize(RoomKey& roomKey, InnFile::SizeT newsz, const char* exResError, const char* exSizeError) {
   PutBigEndian(&newsz, roomKey.Info_.DataSize_ = newsz);
   CheckIoSize(this->Storage_.Write(roomKey.Info_.RoomPos_ + kOffset_DataSize, &newsz, sizeof(newsz)),
               sizeof(newsz), exResError, exSizeError);
}

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
   WriteRoom(this->Storage_, pos + kRoomHeaderSize, bufsz, buf,
             "InnFile.Rewrite: write error.",
             "InnFile.Rewrite: write error size.");
   if (roomKey.Info_.DataSize_ != static_cast<SizeT>(bufsz))
      this->UpdateDataSize(roomKey, static_cast<SizeT>(bufsz),
                           "InnFile.Rewrite: update DataSize error.",
                           "InnFile.Rewrite: update DataSize error size.");
   return roomKey.Info_.DataSize_;
}

InnFile::SizeT InnFile::Write(RoomKey& roomKey, const SizeT offset, const SizeT size, DcQueue& buf) {
   RoomPosT pos = roomKey.Info_.RoomPos_;
   this->CheckRoomPos(pos, "InnFile.Write: not opened.", "InnFile.Write: bad roomPos.");
   if (offset > roomKey.GetDataSize())
      Raise<InnRoomSizeError>("InnFile.Write: bad offset.");
   if (offset + size > roomKey.GetRoomSize())
      Raise<InnRoomSizeError>("InnFile.Write: bad request size.");

   if (size <= 0)
      return 0;
   if (size > buf.CalcSize())
      Raise<InnFileError>(std::errc::invalid_argument, "InnFile.Write: request size > buffer size.");
   WriteRoom(this->Storage_, pos + kRoomHeaderSize + offset, size, buf,
             "InnFile.Write: write error.",
             "InnFile.Write: write error size.");
   SizeT newsz = offset + size;
   if (roomKey.Info_.DataSize_ < newsz)
      this->UpdateDataSize(roomKey, newsz,
                           "InnFile.Write: update DataSize error.",
                           "InnFile.Write: update DataSize error size.");
   return size;
}

} // namespaces
