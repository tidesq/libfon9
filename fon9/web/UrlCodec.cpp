// \file fon9/web/UrlCodec.cpp
// \author fonwinz@gmail.com
#include "fon9/web/UrlCodec.hpp"

namespace fon9 { namespace web {

fon9_API void UrlEncodeAppend(std::string& dst, const StrView& src) {
   UrlEncodeAppend<std::string>(dst, src);
}
fon9_API void UrlEncodeAppend(CharVector& dst, const StrView& src) {
   UrlEncodeAppend<CharVector>(dst, src);
}

//--------------------------------------------------------------------------//

fon9_API void UrlDecodeAppend(std::string& dst, const StrView& src) {
   UrlDecodeAppend<std::string>(dst, src);
}
fon9_API void UrlDecodeAppend(CharVector& dst, const StrView& src) {
   UrlDecodeAppend<CharVector>(dst, src);
}

} } // namespace
