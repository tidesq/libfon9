// \file fon9/web/HttpMessage.cpp
// \author fonwinz@gmail.com
#include "fon9/web/HttpMessage.hpp"

namespace fon9 { namespace web {

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
   this->ExHeaderValues_.clear();
}
HttpMessage::HeaderFieldValues HttpMessage::FindHeadFieldList(StrView name) const {
   HeaderFieldValues res;
   auto ifind = this->HeaderFields_.find(name);
   if (ifind != this->HeaderFields_.end()) {
      const FieldValue* fld = &ifind->second;
      for (;;) {
         res.emplace_back(fld->Value_.ToStrView(this->OrigStr_.c_str()));
         if (fld->Next_ <= 0)
            break;
         fld = &this->ExHeaderValues_[fld->Next_ - 1];
      }
   }
   return res;
}

} } // namespace
