/// \file fon9/Bitv.h
///
/// Bitv = Binary type value encoding protocol.
///
/// \author fonwinz@gmail.com
#ifndef __fon9_Bitv_h__
#define __fon9_Bitv_h__
#include "fon9/sys/Config.h"
#include <stdint.h>

/// \ingroup AlNum
/// - 假設 v = first byte of package.
/// - if ((v & 0x80) == 0): 表示之後接著 (v + 1) 個 bytes 的自訂資料(或 byte[]、或字串).
/// - 若為「數值欄位(Integer or Decimal)」, 則使用「最少用量」能表達的型別.
///   - e.g. Decimal<uint32_t,2> num(0); 則使用 fon9_BitvV_Number0
///      - 使用 1 byte: `1111 1010` 來表示此值
///   - e.g. Decimal<uint32_t,2> num(123.00); 則使用 Integer(正整數): fon9_BitvT_IntegerPos
///      - v = `1001 0xxx`; 之後接著 `0x78`;
///      - (xxx=000)+1 = 正整數的值使用 1 byte 儲存.
///      - 總共使用 2 bytes: `1001 0000` + `0x78`
///   - e.g. Decimal<int32_t,2> num(-9876.50); 則使用 Decimal(負數): fon9_BitvT_Decimal
///      - v = `1011 0xxx` + `sssss S mm`; (mm=01); 之後接著 `0x81 0xcc`;
///      - (xxx=001)+1 = 使用 2 bytes 儲存數值.
///      - S=1 = 使用負整數.
///      - (sssss=00000)+1 = 小數 1 位. 最多可表示32位小數。
///      - 總共使用 4 bytes: `1011 0001` + `00000 1 01` + `0x81` + `0xcc`
///      - 0x181cc = ~(-98765 = 0xfffe7e33)
enum fon9_BitvT fon9_ENUM_underlying(uint8_t) {
   fon9_BitvT_Mask = 0xf8,

   /// 儲存 char[] 或 byte[] 或 int8_t[] 或 uint8_t[]... 之類
   /// - sizeof(T)==1 && T==傳統型別, 的陣列.
   /// - v = `1000 0 xx m` 或 `0x00..0x7f`
   /// - v 之後額外使用 (xx+1)=(1..4) bytes 儲存 length.
   /// - 其中的 `m` 為 length 的 MSB, e.g. (xx=00)+1 = 1 byte(8 bits), 所以 length 有 9 bits 可用.
   /// - e.g. byte[]: `0x83 0x23 0x45 ...`
   ///   - v = `1000 0 01 1`;
   ///   - v 之後接著用 (xx=01)+1 = 2 bytes 記錄 length, 加上(m=1): 0x1, 0x23, 0x45
   ///   - length = (0x1 0x23 0x45 = 74565) + 129 = 74694
   ///      - 加上 129 是因為 byte[1..128]; 使用 v = 0x00..0x7f 來表示.
   ///        - 所以 v = `1000 0 00 0`; + 0x00; 表示: byte[129];
   ///        - 因此 v = `1000 0 00 1`; + 0xff; 表示: byte[640];
   ///   - 完整資料 = v(1000 0 01 1); length(0x23,0x45); byte[74694];
   fon9_BitvT_ByteArray = 0x80,

   /// 儲存動態大小的 container:
   /// - 先儲存 fon9_BitvT_Container + 正整數 N: 表示資料筆數.
   /// - 接著儲存 N 個元素.
   /// - 固定大小的 container **不使用此型別**:
   ///   std::array<T,N> 或 T ary[N] 之類,
   ///   直接儲存 N 個元素, 因 N 為固定大小, 所以不需包含筆數.
   fon9_BitvT_Container = 0x88,

   /// - v = `1001 0xxx`
   /// - v 之後接著用 (xxx+1)=(1..8) bytes 記錄一個正整數.
   fon9_BitvT_IntegerPos = 0x90,
   // - v = `1001 1xxx`
   // - v 之後接著用 (xxx+9)=(9..16) bytes 記錄一個正整數.
   // - 目前不支援.
   //fon9_BitvT_IntegerP16 = 0x98,

   /// - v = `1010 0xxx`
   /// - v 之後接著用 (xxx+1)=(1..8) bytes 記錄一個負整數.
   /// - v 之後的整數取出後, 需使用 ~(value) 才是實際的負整數.
   fon9_BitvT_IntegerNeg = 0xa0,
   // - v = `1010 1xxx`
   // - v 之後接著用 (xxx+9)=(9..16) bytes 記錄一個負整數.
   // - 目前不支援.
   //fon9_BitvT_IntegerN16 = 0xa8,

   /// - v = `1011 0xxx` + `sssss S mm`;
   /// - v 使用 2 bytes; 之後接著用 (xxx+1)=(1..8) bytes 記錄一個整數.
   /// - (sssss+1)=(1..32) 表示小數位數.
   /// - S=0:正數; N=1:負數;
   /// - mm 整數的最前面 2 bits, 也就是說若 xxx=000, 則可用到 10 bits 儲存整數.
   fon9_BitvT_Decimal = 0xb0,
   //fon9_BitvT_Decimal16 = 0xb8

   // float(double, long double): 4,8,16... bytes?
   // 0xc0
   // 0xc8
   // 0xd0
   // 0xd8
   // 0xe0
   // 0xe8

   /// - v = `1111 0xxx`; 表明某些資料為 zero.
   /// - fon9_BitvV_Number0
   /// - fon9_BitvV_Char0
   /// - fon9_BitvV_BoolFalse
   fon9_BitvT_Zero = 0xf0,

   /// - v = `1111 1xxx`; 表明某些資料為 null.
   /// - fon9_BitvV_ByteArrayEmpty
   /// - fon9_BitvV_NumberNull
   fon9_BitvT_Null = 0xf8,
};

/// \ingroup AlNum
/// 與「資料型別」放在同一個 byte 的資料值.
enum fon9_BitvV fon9_ENUM_underlying(uint8_t) {
   /// fon9_BitvT_Zero: v = `1111 0001` = Number 0.
   fon9_BitvV_Number0 = fon9_BitvT_Zero + 0,
   /// one char = '\0';
   fon9_BitvV_Char0 = fon9_BitvT_Zero + 1,
   fon9_BitvV_BoolFalse = fon9_BitvT_Zero + 2,

   /// fon9_BitvT_Null: v = `1111 1000` = byte[0] = 空的 ByteArray.
   fon9_BitvV_ByteArrayEmpty = fon9_BitvT_Null + 0,
   /// fon9_BitvT_Null: v = `1111 1001` = Number Null.
   fon9_BitvV_NumberNull = fon9_BitvT_Null + 1,
   fon9_BitvV_ContainerEmpty = fon9_BitvT_Null + 2,
   fon9_BitvV_BoolTrue = fon9_BitvT_Null + 3,
};

enum fon9_BitvNumT {
   fon9_BitvNumT_Zero,
   fon9_BitvNumT_Pos,
   fon9_BitvNumT_Neg,
   fon9_BitvNumT_Null,
};

fon9_WARN_DISABLE_PADDING;
struct fon9_BitvNumR {
   fon9_BitvNumT  Type_;
   /// 小數位數.
   uint8_t        Scale_;
   /// if (Type_ == fon9_BitvNumT_Neg) 則實際的數值為 (intmax_t)(Num_);
   uintmax_t      Num_;
};
fon9_WARN_POP;

#endif//__fon9_Bitv_h__
