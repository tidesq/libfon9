/// \file fon9/BitvArchive.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_BitvArchive_hpp__
#define __fon9_BitvArchive_hpp__
#include "fon9/Archive.hpp"
#include "fon9/BitvEncode.hpp"
#include "fon9/BitvDecode.hpp"

namespace fon9 {

fon9_WARN_DISABLE_PADDING;
/// \ingroup AlNum
/// BitvOutArchive, BitvInArchive 的基底,
/// 可用來判斷 std::is_base_of<BitvArchive, A>::value 是否為 BitvArchive.
struct BitvArchive {};

struct BitvOutArchive : public OutArchive, public BitvArchive {
   fon9_NON_COPY_NON_MOVE(BitvOutArchive);
   BitvOutArchive() = delete;

   RevBuffer& Buffer_;
   BitvOutArchive(RevBuffer& rbuf) : Buffer_(rbuf) {
   }

   template <class... ArgsT>
   void operator()(ArgsT&&... args) const {
      SerializeOut(*this, std::forward<ArgsT>(args)...);
   }

   template <class T>
   void Save(T&& v) const {
      ToBitv(this->Buffer_, std::forward<T>(v));
   }
};

struct BitvInArchive : public InArchive, public BitvArchive {
   fon9_NON_COPY_NON_MOVE(BitvInArchive);
   BitvInArchive() = delete;

   DcQueue& Buffer_;
   BitvInArchive(DcQueue& buf) : Buffer_(buf) {
   }

   template <class... ArgsT>
   void operator()(ArgsT&&... args) const {
      SerializeIn(*this, std::forward<ArgsT>(args)...);
   }

   template <class T>
   void Load(T&& v) const {
      BitvTo(this->Buffer_, std::forward<T>(v));
   }
};
fon9_WARN_POP;

} // namespace
#endif//__fon9_BitvArchive_hpp__
