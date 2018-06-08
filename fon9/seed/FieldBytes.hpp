/// \file fon9/seed/FieldBytes.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_FieldBytes_hpp__
#define __fon9_seed_FieldBytes_hpp__
#include "fon9/seed/Field.hpp"
#include "fon9/ByteVector.hpp"
#include "fon9/CharAry.hpp" // 支援 CharAry<sz,byte> 視為 byte[]

namespace fon9 { namespace seed {

/// \ingroup seed
/// 存取 byte[] 的欄位類別.
class fon9_API FieldBytes : public Field {
   fon9_NON_COPY_NON_MOVE(FieldBytes);
   using base = Field;
protected:
   using base::base;
public:
   /// 建構:
   /// - 固定為 FieldType::Bytes; FieldSource::DataMember;
   /// - size 必須大於0, 若有動態調整大小的需求, 應使用 FieldByteVector.
   FieldBytes(Named&& named, int32_t ofs, size_t size)
      : base(std::move(named), FieldType::Bytes, FieldSource::DataMember, ofs, size, 0) {
      assert(size > 0);
   }
   /// 建構:
   /// - 固定為 FieldType::Bytes; FieldSource::DyMem;
   FieldBytes(Named&& named, size_t size)
      : base(std::move(named), FieldType::Bytes, FieldSource::DyMem, 0, size, 0) {
      assert(size > 0);
   }

   /// 傳回: "Bn"; n = this->Size_;
   virtual StrView GetTypeId(NumOutBuf&) const override;

   /// 使用 Base64 字串輸出.
   virtual void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override;
   /// value = Base64 字串.
   virtual OpResult StrToCell(const RawWr& wr, StrView value) const override;

   /// 不支援, 傳回 nullValue;
   virtual FieldNumberT GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const override;
   /// 不支援, 不改變 wr, 傳回 OpResult::not_supported_number.
   virtual OpResult PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const override;

   /// \return 全部 bytes 都是 0 時, 返回 true; 只要有一個不是 0 就返回 false.
   virtual bool IsNull(const RawRd& rd) const override;
   /// 全部填入 0
   virtual OpResult SetNull(const RawWr& wr) const override;

   virtual OpResult Copy(const RawWr& wr, const RawRd& rd) const override;
};

template <size_t arysz>
inline FieldSPT<FieldBytes> MakeField(Named&& named, int32_t ofs, byte(&)[arysz]) {
   return FieldSPT<FieldBytes>{new FieldBytes{std::move(named), ofs, arysz}};
}

template <size_t arysz>
inline FieldSPT<FieldBytes> MakeField(Named&& named, int32_t ofs, const byte(&)[arysz]) {
   return FieldSPT<FieldBytes>{new FieldConst<FieldBytes>{std::move(named), ofs, arysz}};
}

template <size_t arysz>
inline FieldSPT<FieldBytes> MakeField(Named&& named, int32_t ofs, CharAry<arysz, byte>&) {
   return FieldSPT<FieldBytes>{new FieldBytes{std::move(named), ofs, arysz}};
}

template <size_t arysz>
inline FieldSPT<FieldBytes> MakeField(Named&& named, int32_t ofs, const CharAry<arysz, byte>&) {
   return FieldSPT<FieldBytes>{new FieldConst<FieldBytes>{std::move(named), ofs, arysz}};
}

//--------------------------------------------------------------------------//

/// \ingroup seed
/// 存取 ByteVector 的欄位, 只能是 data member.
class fon9_API FieldByteVector : public Field {
   fon9_NON_COPY_NON_MOVE(FieldByteVector);
   typedef Field  base;
public:
   /// 建構:
   /// - 固定為 FieldType::Bytes; FieldSource::DataMember;
   FieldByteVector(Named&& named, int32_t ofs)
      : base{std::move(named), FieldType::Bytes, FieldSource::DataMember, ofs, sizeof(ByteVector), 0} {
   }

   /// 傳回: "B0";
   virtual StrView GetTypeId(NumOutBuf&) const override;

   /// 使用 Base64 字串輸出.
   virtual void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override;
   /// value = Base64 字串.
   virtual OpResult StrToCell(const RawWr& wr, StrView value) const override;
   
   /// 不支援, 傳回 nullValue;
   virtual FieldNumberT GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const override;
   /// 不支援, 不改變 wr, 傳回 OpResult::not_supported_number.
   virtual OpResult PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const override;

   /// \return ByteVector.empty()
   virtual bool IsNull(const RawRd& rd) const override;
   /// ByteVector.clear()
   virtual OpResult SetNull(const RawWr& wr) const override;

   virtual OpResult Copy(const RawWr& wr, const RawRd& rd) const override;
};

inline FieldSPT<FieldByteVector> MakeField(Named&& named, int32_t ofs, ByteVector&) {
   return FieldSPT<FieldByteVector>{new FieldByteVector{std::move(named), ofs}};
}
inline FieldSPT<FieldByteVector> MakeField(Named&& named, int32_t ofs, const ByteVector&) {
   return FieldSPT<FieldByteVector>{new FieldConst<FieldByteVector>{std::move(named), ofs}};
}

} } // namespaces
#endif//__fon9_seed_FieldBytes_hpp__
