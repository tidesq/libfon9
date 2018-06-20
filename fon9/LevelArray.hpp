/// \file fon9/LevelArray.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_LevelArray_hpp__
#define __fon9_LevelArray_hpp__
#include "fon9/Endian.hpp"
#include <array>

namespace fon9 {

/// \ingroup Misc
/// - 實際資料筆數不多, 但 Key 的值域很大, 的陣列.
///   - 例如: FIX Message, Key = uint32_t, 但實際會用到的 tag 其實不多.
/// - 資料依序增加的陣列.
///   - 例如: Request Map: RequestKey = index = 每次依序加 1
/// - 如果資料量很大, 且 Key 非常分散, 則 std::unordered_map 可能會是更好的選擇.
/// - DeepN: 實際深度, e.g. uint32_t:
///   - 正常而言深度為 4, 但可設為 3 表示 Key 的值域儘可能是 0..0xffffff;
///   - 但不應 <= 2, 如果值域確實是 0..0xffff, 則應使用 uint16_t.
template <class KeyT, class ValueT, byte DeepN = sizeof(KeyT)>
class LevelArray {
   fon9_NON_COPYABLE(LevelArray);

   static constexpr size_t kDeepStart = sizeof(KeyT) - DeepN;
   static_assert(kDeepStart < sizeof(KeyT), "LevelArray<...DeepN>: Bad DeepN.");

   using ValueBlock = std::array<ValueT, 0x100>;
   struct LevelRec {
      void* NextLevel_{nullptr};
      void Clear(byte lv);
      ValueT& Fetch(byte lv, const byte* keys);
      ValueT* Get(byte lv, const byte* keys);
   };
   using LevelBlock = std::array<LevelRec, 0x100>;
   mutable LevelRec  Head_;
   ValueT& Fetch(KeyT key) const {
      PutBigEndian(&key, key);
      assert(this->CheckKeyDeep(0, reinterpret_cast<byte*>(&key)));
      return this->Head_.Fetch(kDeepStart, reinterpret_cast<byte*>(&key) + kDeepStart);
   }
   static constexpr bool CheckKeyDeep(byte lv, const byte* keys) {
      return (lv == kDeepStart) || ((*keys == 0) && CheckKeyDeep(static_cast<byte>(lv + 1), keys + 1));
   }

public:
   using key_type = KeyT;
   using value_type = ValueT;

   LevelArray() = default;
   ~LevelArray() {
      this->Head_.Clear(kDeepStart);
   }

   LevelArray(LevelArray&& rhs) : Head_{rhs.Head_} {
      rhs.Head_.NextLevel_ = nullptr;
   }
   LevelArray& operator=(LevelArray&& rhs) {
      LevelArray tmp{std::move(rhs)};
      this->swap(tmp);
      return *this;
   }
   void swap(LevelArray& rhs) {
      std::swap(this->Head_.NextLevel_, rhs.Head_.NextLevel_);
   }

   ValueT& operator[](KeyT key) {
      return this->Fetch(key);
   }
   const ValueT& operator[](KeyT key) const {
      return this->Fetch(key);
   }

   ValueT* Get(KeyT key) {
      PutBigEndian(&key, key);
      if (this->CheckKeyDeep(0, reinterpret_cast<byte*>(&key)))
         return this->Head_.Get(kDeepStart, reinterpret_cast<byte*>(&key) + kDeepStart);
      return nullptr;
   }
   const ValueT* Get(KeyT key) const {
      return const_cast<LevelArray*>(this)->Get(key);
   }
};

template <class KeyT, class ValueT, byte DeepN>
void LevelArray<KeyT,ValueT,DeepN>::LevelRec::Clear(byte lv) {
   if (this->NextLevel_ == nullptr)
      return;
   if (lv < sizeof(KeyT) - 1) {
      LevelBlock* b = reinterpret_cast<LevelBlock*>(this->NextLevel_);
      for (LevelRec& r : *b)
         r.Clear(static_cast<byte>(lv + 1));
      delete b;
   }
   else
      delete reinterpret_cast<ValueBlock*>(this->NextLevel_);
   this->NextLevel_ = nullptr;
}

template <class KeyT, class ValueT, byte DeepN>
ValueT& LevelArray<KeyT,ValueT,DeepN>::LevelRec::Fetch(byte lv, const byte* keys) {
   if (lv < sizeof(KeyT) - 1) {
      if (fon9_UNLIKELY(this->NextLevel_ == nullptr))
         this->NextLevel_ = new LevelBlock;
      return (*reinterpret_cast<LevelBlock*>(this->NextLevel_))[*keys]
         .Fetch(static_cast<byte>(lv + 1), keys + 1);
   }
   if (fon9_UNLIKELY(this->NextLevel_ == nullptr))
      this->NextLevel_ = reinterpret_cast<LevelRec*>(new ValueBlock);
   return (*reinterpret_cast<ValueBlock*>(this->NextLevel_))[*keys];
}

template <class KeyT, class ValueT, byte DeepN>
ValueT* LevelArray<KeyT,ValueT,DeepN>::LevelRec::Get(byte lv, const byte* keys) {
   if (lv < sizeof(KeyT) - 1) {
      if (fon9_LIKELY(this->NextLevel_ != nullptr))
         return (*reinterpret_cast<LevelBlock*>(this->NextLevel_))[*keys]
                  .Get(static_cast<byte>(lv + 1), keys + 1);
   }
   else if (fon9_LIKELY(this->NextLevel_ != nullptr))
      return &(*reinterpret_cast<ValueBlock*>(this->NextLevel_))[*keys];
   return nullptr;
}

} // namespace
#endif//__fon9_LevelArray_hpp__
