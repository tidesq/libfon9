libfon9 基礎建設程式庫 - Bitv
===============================
Binary type value encoding protocol

## basic format
* [Bitv.h](../fon9/Bitv.h)
* 基本格式如下：
```
+- 1 byte -+-----------+
|1 tttt nnn|value      |
+----------+-----------+
tttt = type
nnn = 由 type 解釋 nnn 的內容。
value 不定長度，由 type 及 nnn 決定 value 的長度及內容。
```
* 數字採用 big endian 方式儲存

## null
```
+- 1 byte -+
|1 1111 nnn|
+----------+
```
* nnn=000: empty byte array (byte array length = 0)
* nnn=001: null number

## byte array
* 儲存可變長度的：
  * 字串
  * bytes
  * 自訂資料
* 長度為 0, 使用 empty  byte array 表示法 `fon9_BitvV_ByteArrayEmpty`
```
+- 1 byte -+
|1 1111 000|
+----------+
```
* 長度 = 1..128:
```
+- 1 byte -+-----------+
|0 nnnnnnn |byte array|
+----------+-----------+
bytes array: 長度 = (nnnnnnn)+1 = 1..128
```
* 長度 > 128:
```
+- 1 byte -+------+-----------+
|1 0000 xxm|length|byte array |
+----------+------+-----------+
(xx)+1=額外使用多少 bytes 儲存 length, 實際使用 1(m bit) + (xx+1)*8 bits 儲存 length。

例如: xx=00, m=1, length=0 表示使用 1 byte 儲存 length:
+-- 1 byte --+- 1 byte -+- 385 bytes -+
|1 0000 00 1 | 00000000 | byte array  |
+--------- \_實際長度_/ +-------------+
實際長度=385=bin(1 0000 0000) + 129(因為 0..128 使用其他方式處理了)
```

## container
* 固定大小的 container: `T ary[N];` 或 `array<T,N>;` 直接儲存 N 個 T，沒有額外的註記。
* 可變大小的 container:
  * N == 0: `fon9_BitvV_ContainerEmpty = 0xfa`
```
+- 1 byte -+
|1111 1 010|
+----------+
```
  * N > 0: `fon9_BitvT_Container = 0x88`
```
+- 1 byte -+- nnn+1 bytes -+-----------+
|10001  nnn|  N            | N 個 T    |
+----------+---------------+-----------+
(nnn)+1=使用多少 bytes 儲存 N。
```

## integer
* `fon9_BitvT_IntegerPos = 0x90`
* `fon9_BitvT_IntegerNeg = 0xa0`

## decimal
* `fon9_BitvT_Decimal = 0xb0`
