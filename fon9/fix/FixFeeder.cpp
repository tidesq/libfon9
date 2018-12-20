// \file fon9/fix/FixFeeder.cpp
// \author fonwinz@gmail.com
#include "fon9/fix/FixFeeder.hpp"

namespace fon9 { namespace fix {

FixFeeder::~FixFeeder() {
}
FixParser::Result FixFeeder::OnFixMessageError(FixParser::Result res, StrView& fixmsgStream, const char* perr) {
   (void)fixmsgStream; (void)perr;
   return res;
}
FixParser::Result FixFeeder::OnFixStreamReceived(const StrView fixmsgStream) {
   StrView sp{fixmsgStream};
   while(!sp.empty()) {
      const char*       pbeg = sp.begin();
      FixParser::Result res = this->FixParser_.Parse(sp);
      if (res > FixParser::NeedsMore) {
         sp.Reset(pbeg + static_cast<size_t>(res), fixmsgStream.end());
         this->OnFixMessageParsed(StrView{pbeg, sp.begin()});
         this->FixParser_.Clear();
         continue;
      }
      if (res == FixParser::NeedsMore) {
         if (fixmsgStream.begin() != this->RxBuf_.c_str())
            sp.AppendTo(this->RxBuf_);
         else if (size_t esz = static_cast<size_t>(pbeg - fixmsgStream.begin()))
            this->RxBuf_.erase(0, esz);
         return FixParser::NeedsMore;
      }
      const char* perr = sp.begin();
      sp.Reset(pbeg, fixmsgStream.end());
      if ((res = this->OnFixMessageError(res, sp, perr)) <= FixParser::NeedsMore)
         return res;
      this->FixParser_.Clear();
   }
   return FixParser::ParseEnd;
}
FixParser::Result FixFeeder::FeedBuffer(DcQueue& rxbuf) {
   for (;;) {
      auto blk{rxbuf.PeekCurrBlock()};
      if (!blk.second)
         break;
      FixParser::Result res;
      const size_t szUsed = blk.second;
      if (this->RxBuf_.empty()) {
      __PARSE_BLK:
         res = this->OnFixStreamReceived(StrView{reinterpret_cast<const char*>(blk.first), blk.second});
         if (res < FixParser::NeedsMore)
            return res;
      }
      else do {
         size_t expsz = this->FixParser_.GetExpectSize();
         if (expsz <= 0)
            expsz = kFixMinHeaderWidth + kFixTailWidth;
         assert(expsz > this->RxBuf_.size());
         size_t sz = expsz - this->RxBuf_.size();
         if (sz > blk.second)
            sz = blk.second;
         this->RxBuf_.append(reinterpret_cast<const char*>(blk.first), sz);
         if (this->RxBuf_.size() < expsz)
            break;
         assert(this->RxBuf_.size() == expsz);
         res = this->OnFixStreamReceived(&this->RxBuf_);
         if (res < FixParser::NeedsMore)
            return res;
         blk.second -= sz;
         blk.first += sz;
         if (res > FixParser::NeedsMore) {
            this->RxBuf_.clear();
            if (blk.second > 0)
               goto __PARSE_BLK;
         }
      } while (blk.second > 0);
      rxbuf.PopConsumed(szUsed);
   }
   return FixParser::ParseEnd;
}

} } // namespaces
