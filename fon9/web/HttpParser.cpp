// \file fon9/web/HttpParser.cpp
// \author fonwinz@gmail.com
#include "fon9/web/HttpParser.hpp"
#include "fon9/StrTo.hpp"

namespace fon9 { namespace web {

constexpr size_t kMinHeaderSize = sizeof("GET / HTTP/" "\r\n\r\n");
constexpr size_t kMaxHeaderSize = 1024 * 8;

static bool TrimHeadAndAppend(std::string& dst, BufferNode* front) {
   while (front) {
      const char* pend = reinterpret_cast<const char*>(front->GetDataEnd());
      const char* pbeg = StrFindTrimHead(reinterpret_cast<const char*>(front->GetDataBegin()), pend);
      if (pbeg != pend) {
         dst.append(pbeg, pend);
         while ((front = front->GetNext()) != nullptr)
            dst.append(reinterpret_cast<const char*>(front->GetDataBegin()), front->GetDataSize());
         return true;
      }
      front = front->GetNext();
   }
   return false;
}

//--------------------------------------------------------------------------//

void HttpMessage::RemoveFullMessage() {
   const char*  origBegin = this->OrigStr_.c_str();
   StrView tail{origBegin + (this->IsChunked() ? this->ChunkTrailer_.End() : this->Body_.End()),
                origBegin + this->OrigStr_.size()};
   if (StrTrimHead(&tail).empty())
      this->OrigStr_.clear();
   else
      this->OrigStr_.erase(0, static_cast<size_t>(tail.begin() - origBegin));
   this->ClearFields();
}
void HttpMessage::ClearAll() {
   this->OrigStr_.clear();
   this->ClearFields();
}
void HttpMessage::ClearFields() {
   this->StartLine_.Size_ = 0;
   this->Body_.Pos_ = this->Body_.Size_ = 0;
   this->ContentLength_ = 0;
   this->NextChunkSize_ = 0;
   this->CurrChunkFrom_ = 0;
   this->ChunkExt_.clear();
   this->ChunkTrailer_.Pos_ = this->ChunkTrailer_.Size_ = 0;
   this->HeaderFields_.clear();
}

//--------------------------------------------------------------------------//

HttpResult HttpParser::Feed(HttpMessage& msg, BufferList buf) {
   if (msg.IsHeaderReady()) {
      BufferAppendTo(buf, msg.OrigStr_);
      if (msg.IsChunked())
         return HttpParser::ParseChunk(msg);
      return HttpParser::AfterFeedBody(msg);
   }
   if (size_t bfsz = msg.OrigStr_.size()) {
      BufferAppendTo(buf, msg.OrigStr_);
      return HttpParser::AfterFeedHeader(msg, bfsz);
   }
   if (TrimHeadAndAppend(msg.OrigStr_, buf.front()))
      return HttpParser::AfterFeedHeader(msg, 0);
   return HttpResult::Incomplete;
}
HttpResult HttpParser::AfterFeedHeader(HttpMessage& msg, size_t bfsz) {
   if (msg.OrigStr_.size() < kMinHeaderSize) // min http header.
      return HttpResult::Incomplete;
   size_t pHeadEnd = msg.OrigStr_.find("\r\n\r\n", bfsz > 3 ? bfsz - 3 : 0, 4);
   if (pHeadEnd == std::string::npos) {
      if (msg.OrigStr_.size() > kMaxHeaderSize)
         return HttpResult::HeaderTooLarge;
      return HttpResult::Incomplete;
   }
   msg.Body_.Pos_ = pHeadEnd + 4;
   msg.Body_.Size_ = 0;

   const char* const origBegin = msg.OrigStr_.c_str();
   StrView           header{origBegin, pHeadEnd};
   msg.StartLine_.FromStrView(origBegin, StrFetchTrim(header, '\r'));
   using SubStr = HttpMessage::SubStr;
   SubStr sname;
   while (!StrTrim(&header).empty()) {
      StrView  value = StrFetchNoTrim(header, '\r');
      StrView  name = StrFetchNoTrim(value, ':');
      sname.FromStrView(origBegin, name);
      msg.HeaderFields_.kfetch(sname).second.FromStrView(origBegin, StrTrim(&value));
   }
   StrView val = msg.FindHeadField("transfer-encoding");
   while (!val.empty()) {
      StrView v = StrFetchTrim(val, ',');
      if (iequals(v, "chunked"))
         msg.ContentLength_ = kHttpContentLengthChunked;
   }
   if (msg.IsChunked())
      msg.ChunkTrailer_.Pos_ = msg.Body_.Pos_;
   else
      msg.ContentLength_ = StrTo(msg.FindHeadField("content-length"), 0u);
   return HttpParser::AfterFeedBody(msg);
}
HttpResult HttpParser::AfterFeedBody(HttpMessage& msg) {
   if (fon9_UNLIKELY(msg.IsChunked()))
      return HttpParser::ParseChunk(msg);
   if (msg.Body_.Size_ >= msg.ContentLength_)
      return HttpResult::FullMessage;
   return HttpResult::Incomplete;
}
HttpResult HttpParser::ParseChunk(HttpMessage& msg) {
   if (msg.NextChunkSize_ == 0)
      return HttpParser::FetchNextChunkSize(msg);
   return HttpParser::CheckChunkAppend(msg);
}
HttpResult HttpParser::CheckChunkAppend(HttpMessage& msg) {
   assert(msg.NextChunkSize_ > 0);
   auto bodyEnd = msg.Body_.End();
   if (msg.OrigStr_.size() - bodyEnd < msg.NextChunkSize_)
      return HttpResult::Incomplete;
   msg.CurrChunkFrom_ = bodyEnd;
   msg.Body_.Size_ += msg.NextChunkSize_;
   msg.ChunkTrailer_.Pos_ += msg.NextChunkSize_;
   msg.NextChunkSize_ = 0;
   return HttpResult::ChunkAppended;
}
HttpResult HttpParser::FetchNextChunkSize(HttpMessage& msg) {
   assert(msg.NextChunkSize_ == 0);
   if (msg.ChunkTrailer_.Size_ > 0)
      return HttpParser::FetchChunkTrailer(msg);
   const char* const origBegin = msg.OrigStr_.c_str();
   StrView     chunk{origBegin + msg.ChunkTrailer_.Pos_, origBegin + msg.OrigStr_.size()};
   if (StrTrimHead(&chunk).empty()) {
      msg.OrigStr_.erase(msg.ChunkTrailer_.Pos_);
      return HttpResult::Incomplete;
   }
   const char* const pCR = chunk.Find('\r');
   if (pCR == nullptr || pCR == chunk.end() - 1) {
      if (chunk.size() > kHttpMaxChunkLineLength)
         return HttpResult::ChunkSizeLineTooLong;
      msg.ChunkTrailer_.Pos_ = static_cast<size_t>(chunk.begin() - origBegin);
      return HttpResult::Incomplete;
   }
   if (pCR[1] != '\n')
      return HttpResult::BadChunked;
   const char* pHexEnd;
   msg.NextChunkSize_ = HexStrTo(chunk, &pHexEnd);
   if (msg.NextChunkSize_ > kHttpMaxChunkedSize)
      return HttpResult::ChunkSizeTooLarge;
   while (pHexEnd != pCR) {
      if (*pHexEnd == ';') {
         ++pHexEnd;
         break;
      }
      if (!isspace(static_cast<unsigned char>(*pHexEnd)))
         return HttpResult::BadChunked;
      ++pHexEnd;
   }
   msg.ChunkExt_.assign(pHexEnd, pCR);
   chunk.SetBegin(pCR + 2);

   if (msg.NextChunkSize_ > 0) {
      msg.OrigStr_.erase(msg.ChunkTrailer_.Pos_, static_cast<size_t>(chunk.begin() - origBegin - msg.ChunkTrailer_.Pos_));
      return HttpParser::CheckChunkAppend(msg);
   }
   // msg.NextChunkSize_==0: last-chunk.
   msg.CurrChunkFrom_ = msg.Body_.Pos_;
   msg.ChunkTrailer_.Pos_ = static_cast<size_t>(chunk.begin() - origBegin);
   if (chunk.Get1st() == '\r') // 懶惰一下: 不檢查 '\n', 直接排除 trailer '\r'及之後的空白.
      return HttpResult::FullMessage;
   // trailer = *(entity-header CRLF);
   msg.ChunkTrailer_.Size_ = 1;
   return chunk.size() <= 3 ? HttpResult::Incomplete : HttpParser::FetchChunkTrailer(msg);
}
HttpResult HttpParser::FetchChunkTrailer(HttpMessage& msg) {
   assert(msg.NextChunkSize_ == 0 && msg.ChunkTrailer_.Size_ > 0);
   size_t pos = msg.ChunkTrailer_.Size_;
   if (pos == 1 && *(msg.OrigStr_.c_str() + msg.ChunkTrailer_.Pos_) == '\r') {
      msg.ChunkTrailer_.Size_ = 0;
      return HttpResult::FullMessage;
   }
   pos = (pos > 3 ? (msg.ChunkTrailer_.Pos_ + pos - 3) : msg.ChunkTrailer_.Pos_);
   size_t pChunkTailerEnd = msg.OrigStr_.find("\r\n\r\n", pos, 4);
   if (pChunkTailerEnd == std::string::npos) {
      msg.ChunkTrailer_.Size_ = msg.OrigStr_.size() - msg.ChunkTrailer_.Pos_;
      return HttpResult::Incomplete;
   }
   msg.ChunkTrailer_.Size_ = pChunkTailerEnd - msg.ChunkTrailer_.Pos_;
   return HttpResult::FullMessage;
}
HttpResult HttpParser::ContinueEat(HttpMessage& msg) {
   if (!msg.IsHeaderReady())
      return HttpParser::AfterFeedHeader(msg, 0);
   assert(msg.IsChunked());
   return HttpParser::ParseChunk(msg);
}

} } // namespace
