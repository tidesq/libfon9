// \file fon9/crypto/Sha256.cpp
#include "fon9/crypto/Sha256.hpp"

#if defined(_MSC_VER)
#pragma intrinsic(_lrotr,_lrotl)
#define RORc(x,n) _lrotr(x,n)

#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)) && !defined(INTEL_CC)
#define RORc(word,i) ({ \
   uint32_t __RORc_tmp = (word); \
   __asm__ ("rorl %2, %0" : \
            "=r" (__RORc_tmp) : \
            "0" (__RORc_tmp), \
            "I" (i)); \
            __RORc_tmp; \
   })

#else
// PPC32
// static inline uint32_t RORc(uint32_t word, const int i) {
//    asm ("rotrwi %0,%0,%2"
//         :"=r" (word)
//         : "0" (word), "I" (i));
//    return word;
// }
static inline uint32_t RORc(uint32_t x, int y) {
   return static_cast<uint32_t>((x >> (y & 31)) | (x << ((32 - (y & 31)) & 31)));
}

#endif

namespace fon9 { namespace crypto {

static const uint32_t K[64] = {
   0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
   0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
   0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
   0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
   0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
   0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
   0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
   0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
   0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
   0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
   0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
   0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
   0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};
#define Ch(x,y,z)       (z ^ (x & (y ^ z)))
#define Maj(x,y,z)      (((x | y) & z) | (x & y))
#define S(x, n)         RORc((x),(n))
#define R(x, n)         (((x)&0xFFFFFFFFUL)>>(n))
#define Sigma0(x)       (S(x, 2) ^ S(x, 13) ^ S(x, 22))
#define Sigma1(x)       (S(x, 6) ^ S(x, 11) ^ S(x, 25))
#define Gamma0(x)       (S(x, 7) ^ S(x, 18) ^ R(x, 3))
#define Gamma1(x)       (S(x, 17) ^ S(x, 19) ^ R(x, 10))
// Hash a single block. This is the core of the algorithm.
// 原始來源 https://github.com/libtom/libtomcrypt/blob/develop/src/hashes/sha2/sha256.c
void ContextSha256::Transform(uint32_t state[kStateSize], const byte buffer[kBlockSize]) {
   uint32_t S[8], W[64], t0, t1, t, i;
   memcpy(S, state, sizeof(S));

   uint32_t* pW = W;
   for (i = 0; i < 16; ++i) {
      memcpy(pW, buffer + (4 * i), sizeof(*pW));
      PutBigEndian(pW, *pW);
      ++pW;
   }
   for (i = 16; i < 64; ++i) {
      *pW = static_cast<uint32_t>(Gamma1(*(pW- 2)) + *(pW - 7) + Gamma0(*(pW - 15)) + *(pW - 16));
      ++pW;
   }

   #define RND(a,b,c,d,e,f,g,h,i)                         \
        t0 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i];   \
        t1 = Sigma0(a) + Maj(a, b, c);                    \
        d += t0;                                          \
        h  = t0 + t1;

   for (i = 0; i < 64; ++i) {
      RND(S[0], S[1], S[2], S[3], S[4], S[5], S[6], S[7], i);
      t = S[7]; S[7] = S[6]; S[6] = S[5]; S[5] = S[4];
      S[4] = S[3]; S[3] = S[2]; S[2] = S[1]; S[1] = S[0]; S[0] = t;
   }

   for (i = 0; i < 8; ++i) {
      state[i] = state[i] + S[i];
   }
}

void ContextSha256::Init() {
   this->State_[0] = 0x6A09E667;
   this->State_[1] = 0xBB67AE85;
   this->State_[2] = 0x3C6EF372;
   this->State_[3] = 0xA54FF53A;
   this->State_[4] = 0x510E527F;
   this->State_[5] = 0x9B05688C;
   this->State_[6] = 0x1F83D9AB;
   this->State_[7] = 0x5BE0CD19;
   this->Count_ = 0;
}

void ContextSha256::Update(const void* data, size_t len) {
   HashBlockUpdate(*this, data, len);
}

void ContextSha256::Final(byte output[kOutputSize]) {
   HashBlockFinal(*this, output);
}

//--------------------------------------------------------------------------//

void Sha256::Hash(const void* data, size_t len, byte output[kOutputSize]) {
   CalcHash<ContextSha256>(data, len, output);
}

void Sha256::Hmac(const void* text, size_t textLen,
                  const void* key, size_t keyLen,
                  byte output[kOutputSize])
{
   CalcHmac<ContextSha256>(text, textLen, key, keyLen, output);
}

bool Sha256::CalcSaltedPassword(const void* pass, size_t passLen,
                                const void* salt, size_t saltLen,
                                size_t iter,
                                size_t outLen, void* out)
{
   return CalcPbkdf2<ContextSha256>(pass, passLen, salt, saltLen, iter, outLen, out);
}

} } // namespace
