/// \file fon9/crypto/CryptoTools.hpp
///
/// 您應該使用 OpenSSL or Windows Bcrypt.dll
/// 這裡僅提供一些簡單的包裝, 並降低相依姓.
///
/// \author fonwinz@gmail.com
#ifndef __fon9_crypto_CryptoTools_hpp__
#define __fon9_crypto_CryptoTools_hpp__
#include "fon9/Utility.hpp"
#include "fon9/Endian.hpp"
#include "fon9/buffer/MemBlock.hpp"

namespace fon9 { namespace crypto {

template <class AlgContext>
inline void CalcHash(const void* data, size_t len, byte output[AlgContext::kOutputSize]) {
   AlgContext alg;
   alg.Init();
   alg.Update(data, len);
   alg.Final(output);
}

template <class AlgContext>
inline void HashBlockUpdate(AlgContext& alg, const void* data, size_t len) {
   if (len <= 0)
      return;
   uint32_t bufferUsed = static_cast<uint32_t>(alg.Count_ & (alg.kBlockSize - 1));
   alg.Count_ += len;
   if (bufferUsed) {
      uint32_t bufferFill = static_cast<uint32_t>(alg.kBlockSize - bufferUsed);
      if (len >= bufferFill) {
         memcpy(alg.Buffer_ + bufferUsed, data, bufferFill);
         alg.Transform(alg.State_, alg.Buffer_);
         if ((len -= bufferFill) <= 0)
            return;
         bufferUsed = 0;
         data = reinterpret_cast<const byte*>(data) + bufferFill;
      }
   }
   while (len >= alg.kBlockSize) {
      alg.Transform(alg.State_, reinterpret_cast<const byte*>(data));
      data = reinterpret_cast<const byte*>(data) + alg.kBlockSize;
      len -= alg.kBlockSize;
   }
   if (len > 0)
      memcpy(alg.Buffer_ + bufferUsed, data, len);
}

template <class AlgContext>
inline void HashBlockFinal(AlgContext& alg, byte output[AlgContext::kOutputSize]) {
   static const byte kFinalPadding[alg.kBlockSize] = {0x80};
   auto     bitCount = alg.Count_ << 3;
   uint32_t bufferUsed = static_cast<uint32_t>(alg.Count_ & (alg.kBlockSize - 1));
   constexpr uint32_t kExpLastSize = alg.kBlockSize - sizeof(bitCount);
   uint32_t padCount = static_cast<uint32_t>((bufferUsed < kExpLastSize)
                                             ? (kExpLastSize - bufferUsed)
                                             : (alg.kBlockSize + kExpLastSize - bufferUsed));
   alg.Update(kFinalPadding, padCount);
   PutBigEndian(alg.Buffer_ + kExpLastSize, bitCount);
   alg.Transform(alg.State_, alg.Buffer_);

   for (size_t L = 0; L < alg.kStateSize; ++L) {
      PutBigEndian(output, alg.State_[L]);
      output += sizeof(alg.State_[L]);
   }
}

//--------------------------------------------------------------------------//

template <class AlgContext>
class HmacContext {
   AlgContext  Alg_;
   byte        OuterPad_[AlgContext::kBlockSize];
public:
   static_assert(AlgContext::kOutputSize <= AlgContext::kBlockSize, "HmacContext: Unsupport alg.");
   enum : size_t {
      kOutputSize = AlgContext::kOutputSize,
      kBlockSize = AlgContext::kBlockSize,
   };

   void Init(const void* key, size_t keyLen) {
      byte  ipad[AlgContext::kBlockSize];
      if (keyLen > AlgContext::kBlockSize) {
         this->Alg_.Init();
         this->Alg_.Update(key, keyLen);
         this->Alg_.Final(ipad);
         keyLen = AlgContext::kOutputSize;
      }
      else {
         memcpy(ipad, key, keyLen);
         memset(ipad + keyLen, 0x36, sizeof(ipad) - keyLen);
         memcpy(this->OuterPad_, key, keyLen);
         memset(this->OuterPad_ + keyLen, 0x5c, sizeof(this->OuterPad_) - keyLen);
      }
      for (size_t i = 0; i < keyLen; i++) {
         ipad[i] ^= 0x36;
         this->OuterPad_[i] ^= 0x5c;
      }
      this->Alg_.Init();
      this->Alg_.Update(ipad, AlgContext::kBlockSize);
   }
   void Update(const void* data, size_t len) {
      this->Alg_.Update(data, len);
   }
   void Final(byte output[kOutputSize]) {
      this->Alg_.Final(output);
      this->Alg_.Init();
      this->Alg_.Update(this->OuterPad_, AlgContext::kBlockSize);
      this->Alg_.Update(output, AlgContext::kOutputSize);
      this->Alg_.Final(output);
   }
};

template <class AlgContext>
void CalcHmac(const void *text, size_t textLen,
              const void *key, size_t keyLen,
              byte output[AlgContext::kOutputSize])
{
   HmacContext<AlgContext> context;
   context.Init(key, keyLen);
   context.Update(text, textLen);
   context.Final(output);
}

template <class AlgContext>
bool CalcPbkdf2(const void* pass, size_t passLen,
                const void* salt, size_t saltLen,
                size_t iter,
                size_t outLen, void* out)
{
   using Context = HmacContext<AlgContext>;
   uint32_t iBlk = 1;
   Context  preKeyCtx;
   preKeyCtx.Init(pass, passLen);
   MemBlock mem{static_cast<MemBlockSize>(saltLen + sizeof(iBlk))};
   byte*    pFirstUpdate = mem.begin();
   if (!pFirstUpdate)
      return false;
   memcpy(pFirstUpdate, salt, saltLen);

   while (outLen > 0) {
      PutBigEndian(pFirstUpdate + saltLen, iBlk);

      Context  hmacCtx{preKeyCtx};
      hmacCtx.Update(pFirstUpdate, saltLen + sizeof(iBlk));

      byte     digtmp[1024];
      hmacCtx.Final(digtmp);

      size_t cplen = (outLen > AlgContext::kOutputSize ? AlgContext::kOutputSize : outLen);
      memcpy(out, digtmp, cplen);

      for (size_t i = 1; i < iter; ++i) {
         hmacCtx = preKeyCtx;
         hmacCtx.Update(digtmp, AlgContext::kOutputSize);
         hmacCtx.Final(digtmp);
         for (size_t k = 0; k < cplen; ++k)
            reinterpret_cast<byte*>(out)[k] ^= digtmp[k];
      }
      ++iBlk;
      outLen -= cplen;
      out = reinterpret_cast<byte*>(out) + cplen;
   }
   return true;
}

} } // namespaces
#endif//__fon9_crypto_CryptoTools_hpp__
