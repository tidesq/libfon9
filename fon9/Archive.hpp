/// \file fon9/Archive.hpp
///
/// 序列化: 參數使用一致的呼叫順序, 避免 序列/反序列 時, 順序寫錯的問題.
/// \code
///   template <class Archive>
///   void Serialize(Archive& ar, ArchiveWorker<Archive,MyObject>& rec) {
///      return ar(rec.Name_, rec.Address_, rec.Tel_);
///   }
/// \endcode
///
/// 編碼 Serialize/Encode:
/// \code
///   const MyObject src;
///   BitvOutArchive oar{rbuf};
///   oar(src);
/// \endcode
///
/// 解碼 Deserialize/Decode:
/// \code
///   MyObject       out;
///   BitvInArchive  iar{buf};
///   iar(out);
/// \endcode
///
/// \author fonwinz@gmail.com
#ifndef __fon9_Archive_hpp__
#define __fon9_Archive_hpp__
#include "fon9/Utility.hpp"

namespace fon9 {

struct InArchive {};
struct OutArchive {};

/// ingroup AlNum
/// 用來協助自訂型別定義 Serialize/Deserialize 函式:
/// - 在 InArchive 時, 應為 MyObject;
/// - 在 OutArchive 時, 應為 const MyObject;
/// \code
///    template <class Archive>
///    void Serialize(Archive& ar, ArchiveWorker<Archive,MyObject>& rec);
/// \endcode
template <class Archive, typename T>
using ArchiveWorker = conditional_t<std::is_base_of<InArchive, Archive>::value, decay_t<T>, const T>;

//--------------------------------------------------------------------------//

template <class Archive, class T>
struct Serializer {
   using yes = int8_t;
   using no = int64_t;
   static_assert(sizeof(yes) != sizeof(no), "Serializer type of 'yes' 'no' cannot be same");

   static T&& GetTestValue();

   // 因為使用 *static_cast<T*>(nullptr) 會找不到底下的函式:
   // \code
   //    template <class Archive>
   //    void Serialize(Archive& ar, ArchiveWorker<Archive,MyObject>& rec);
   // \endcode
   // 所以使用 std::forward<T>(GetTestValue())
   template <class A>
   static yes CheckHasFn_Serialize(decltype(Serialize(*static_cast<A*>(nullptr), std::forward<T>(GetTestValue())))*);

   template <class A>
   static no CheckHasFn_Serialize(...);

   enum {
      kHasSerialize = sizeof(decltype(CheckHasFn_Serialize<Archive>(0))) == sizeof(yes)
   };

   template <class A, class B>
   static auto Do(A& ar, B&& v) -> decltype(Serialize(ar, std::forward<T>(v))) {
      return Serialize(ar, std::forward<B>(v));
   }

   template <class A, class B>
   static auto Do(A& ar, B&& v) -> enable_if_t<!kHasSerialize && std::is_base_of<OutArchive, A>::value> {
      return ar.Save(std::forward<B>(v));
   }

   template <class A, class B>
   static auto Do(A& ar, B&& v) -> enable_if_t<!kHasSerialize && std::is_base_of<InArchive, A>::value> {
      return ar.Load(std::forward<B>(v));
   }
};

//--------------------------------------------------------------------------//

/// 當結合版本資訊時, 可透過呼叫舊版的 serialize function 再加上新版的欄位, e.g.:
/// \code
///   template <class Archive>
///   void SerializeV0(Archive& ar, ArchiveWorker<Archive,CompositeRec>& rec);
///
///   template <class Archive>
///   void SerializeVer(Archive& ar, ArchiveWorker<Archive,CompositeRec>& rec, unsigned ver) {
///      switch (ver) {
///      case 0:  return SerializeV0(ar, rec);
///      case 1:  // 版本1: 以 ver 0 為基礎, 加上 V1_, Dec_ 欄位.
///               return ar(std::make_pair(&SerializeV0<Archive>, &rec), rec.V1_, rec.Dec_);
///      default: // 不認識的版本, 丟出異常.
///               throw;
///      }
///   }
///   template <class Archive>
///   fon9_SeriR Serialize(Archive& ar, ArchiveWorker<Archive,CompositeRec>& rec) {
///      CompositeVer<decltype(rec)> ver{rec, 1};
///      return ar(ver);
///   }
/// \endcode
template <class Archive, class T>
inline void SerializeOut(Archive& ar, std::pair<void(*)(Archive&, T&), T*>&& fnv) {
   return (*fnv.first)(ar, *fnv.second);
}

template <class Archive, class T>
inline void SerializeOut(Archive& ar, T&& v) {
   return Serializer<Archive, T>::Do(ar, std::forward<T>(v));
}

template <class Archive, class T, class... ArgsT>
inline void SerializeOut(Archive& ar, T&& v, ArgsT&&... args) {
   SerializeOut(ar, std::forward<ArgsT>(args)...);
   SerializeOut(ar, std::forward<T>(v));
}

//--------------------------------------------------------------------------//

template <class Archive, class T>
inline void SerializeIn(Archive& ar, std::pair<void(*)(Archive&, T&), T*>&& fnv) {
   return (*fnv.first)(ar, *fnv.second);
}

template <class Archive, class T>
inline void SerializeIn(Archive& ar, T&& v) {
   return Serializer<Archive, T>::Do(ar, std::forward<T>(v));
}

template <class Archive, class T, class... ArgsT>
inline void SerializeIn(Archive& ar, T&& v, ArgsT&&... args) {
   SerializeIn(ar, std::forward<T>(v));
   SerializeIn(ar, std::forward<ArgsT>(args)...);
}

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
/// 輔助儲存、取出有版本訊息的資料.
/// 使用範例:
/// \code
///   template <class Archive>
///   void SerializeVer(Archive& ar, ArchiveWorker<Archive, MyObject>& rec, unsigned ver);
///
///   template <class Archive>
///   void Serialize(Archive& ar, ArchiveWorker<Archive, MyObject>& rec) {
///      CompositeVer<decltype(rec)> vrec{rec, 1};
///      ar(vrec);
///   }
/// \endcode
template <class Object>
class CompositeVer {
   fon9_NON_COPY_NON_MOVE(CompositeVer);
public:
   unsigned Ver_{};
   Object&  Obj_;
   CompositeVer(Object& obj, unsigned ver) : Ver_{ver}, Obj_(obj) {
   }

   template <class Archive>
   inline friend auto Serialize(Archive& oar, const CompositeVer& verc)
      -> enable_if_t<std::is_base_of<OutArchive, Archive>::value> {
      SerializeVer(oar, verc.Obj_, verc.Ver_);
      oar(verc.Ver_);
   }
   template <class Archive>
   inline friend auto Serialize(Archive& iar, CompositeVer& verc)
      -> enable_if_t<std::is_base_of<InArchive, Archive>::value> {
      iar(verc.Ver_);
      SerializeVer(iar, verc.Obj_, verc.Ver_);
   }
};
fon9_WARN_POP;

} // namespace
#endif//__fon9_Archive_hpp__
