// \file fon9/web/HttpParser.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_web_HttpParser_hpp__
#define __fon9_web_HttpParser_hpp__
#include "fon9/StrTools.hpp"
#include "fon9/SortedVector.hpp"
#include "fon9/buffer/BufferList.hpp"

namespace fon9 { namespace web {

enum : size_t {
   kHttpContentLengthChunked = static_cast<size_t>(-1),
   kHttpMaxChunkedSize = 1024 * 1024 * 8,
   /// strlen("chunk-size[chunk-ext]\r\n");
   kHttpMaxChunkLineLength = 64,
};

enum class HttpResult {
   HeaderTooLarge = -1,
   BadChunked = -2,
   /// chunked size 超過 kHttpMaxChunkedSize
   ChunkSizeTooLarge = -3,
   ChunkSizeLineTooLong = -4,
   Incomplete = 0,
   FullMessage = 1,
   ChunkAppended = 2,
};

struct fon9_API HttpParser;
struct fon9_API HttpMessage {
   HttpMessage() : HeaderFields_{Compare{&OrigStr_}} {
   }

   /// 一筆 FullMessage 處理完畢後的清理.
   void RemoveFullMessage();

   /// 通常用於 TcpClient 斷線後的清理工作.
   void ClearAll();

   /// - 移除前後空白之後的 start-line.
   /// - e.g. "GET /index.html HTTP/1.1"
   /// - e.g. "HTTP/1.1 200 OK"
   /// - 後續可用 StrFetchNoTrim(startline, ' '); 取得各個元素的內容, e.g.:
   ///   \code
   ///   StrView method = StrFetchNoTrim(startline, ' '); 
   ///   StrView target = StrFetchNoTrim(startline, ' '); 
   ///   StrView version = startline;
   ///   \endcode
   StrView StartLine() const {
      return this->StartLine_.ToStrView(this->OrigStr_.c_str());
   }
   /// 傳回值==nullptr, 則表示沒有設該欄位.
   StrView FindHeadField(StrView name) {
      auto ifind = this->HeaderFields_.find(name);
      return ifind == this->HeaderFields_.end() ? StrView{nullptr}
         : ifind->second.ToStrView(this->OrigStr_.c_str());
   }
   StrView Body() const {
      return this->Body_.ToStrView(this->OrigStr_.c_str());
   }
   /// 當解析結果為 HttpResult::ChunkAppended, 則可取得此次新增的部分.
   StrView ChunkData() const {
      const char* origBegin = this->OrigStr_.c_str();
      return StrView{origBegin + this->CurrChunkFrom_, origBegin + this->Body_.End()};
   }
   /// 尚未解析的 chunk-ext.
   StrView ChunkExt() const {
      return &this->ChunkExt_;
   }
   /// 尚未解析的 chunk-trailer.
   StrView ChunkTrailer() const {
      return this->ChunkTrailer_.ToStrView(this->OrigStr_.c_str());
   }

   bool IsHeaderReady() const {
      return this->StartLine_.Size_ != 0;
   }
   bool IsChunked() const {
      return (this->ContentLength_ == kHttpContentLengthChunked);
   }

private:
   void ClearFields();

   friend struct fon9_API HttpParser;
   struct SubStr {
      size_t   Pos_{0};
      size_t   Size_{0};
      StrView ToStrView(const char* from) const {
         return StrView{from + this->Pos_, this->Size_};
      }
      void FromStrView(const char* from, StrView s) {
         this->Pos_ = static_cast<size_t>(s.begin() - from);
         this->Size_ = s.size();
      }
      size_t End() const {
         return this->Pos_ + this->Size_;
      }
   };

   /// 完整的訊息內容.
   std::string OrigStr_;
   SubStr      StartLine_;
   SubStr      Body_;

   /// 優先順序如下:
   /// Transfer-Encoding: chunked 則此值為 kHttpContentLengthChunked.
   /// Content-Length: 所設定的長度.
   size_t      ContentLength_{0};

   size_t      NextChunkSize_{0};
   size_t      CurrChunkFrom_{0};

   /// 尚未解析的 chunk-ext.
   /// 因為 chunk-size[;chunk-ext] 在收到 chunk-data 時,
   /// 會從 body 移除, 讓 body 成為沒有 chunk-size 的「乾淨」訊息,
   /// 所以需要用額外的空間儲存 ChunkExt_;
   std::string ChunkExt_;
   /// 尚未解析的 chunk-trailer.
   SubStr      ChunkTrailer_;

   struct Compare {
      std::string* OrigStr_;
      bool operator()(const SubStr& a, const SubStr& b) const {
         return icompare(a.ToStrView(this->OrigStr_->c_str()), b.ToStrView(this->OrigStr_->c_str())) < 0;
      }
      bool operator()(const StrView& a, const SubStr& b) const {
         return icompare(a, b.ToStrView(this->OrigStr_->c_str())) < 0;
      }
      bool operator()(const SubStr& a, const StrView& b) const {
         return icompare(a.ToStrView(this->OrigStr_->c_str()), b) < 0;
      }
   };
   using Fields = SortedVector<SubStr, SubStr, Compare>;
   Fields      HeaderFields_;
};

struct fon9_API HttpParser {
   static HttpResult Feed(HttpMessage& msg, BufferList buf);
   static HttpResult ContinueEat(HttpMessage& msg);

private:
   static HttpResult AfterFeedBody(HttpMessage& msg);
   static HttpResult AfterFeedHeader(HttpMessage& msg, size_t bfsz);
   static HttpResult ParseChunk(HttpMessage& msg);
   static HttpResult CheckChunkAppend(HttpMessage& msg);
   static HttpResult FetchNextChunkSize(HttpMessage& msg);
   static HttpResult FetchChunkTrailer(HttpMessage& msg);
};

} } // namespaces
#endif//__fon9_web_HttpParser_hpp__
