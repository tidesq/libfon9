// \file fon9/web/HttpParser.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_web_HttpParser_hpp__
#define __fon9_web_HttpParser_hpp__
#include "fon9/web/HttpMessage.hpp"
#include "fon9/buffer/BufferList.hpp"

namespace fon9 { namespace web {

enum : size_t {
   /// strlen("chunk-size[chunk-ext]\r\n");
   kHttpMaxChunkLineLength = 64,
   kHttpMaxChunkedSize = 1024 * 1024 * 8,
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
