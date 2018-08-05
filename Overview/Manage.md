libfon9 基礎建設程式庫 - 系統管理基礎建設
=========================================

## 基本說明

---------------------------------------

## User Auth 使用者管理機制
* 使用 seed 機制管理資料表: user、role、policy... 
* 提供 ACL(Access Control List) policy
* 使用 InnDbf 儲存資料表

### 使用 SASL 處理認證協商
* [SASL](https://tools.ietf.org/html/rfc5802)
* [SCRAM-SHA-256](https://tools.ietf.org/html/rfc7677)
  * 不支援 channel binding.
  * 不支援 SASLprep.
  * 一般認證訊息範例: The username 'user' and password 'pencil' are being used.
   ```
   C: n,,n=user,r=rOprNGfwEbeRWgbNEkqO
   S: r=rOprNGfwEbeRWgbNEkqO%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0,
      s=W22ZaJ0SNY7soEsUEjb6gQ==,i=4096
   C: c=biws,r=rOprNGfwEbeRWgbNEkqO%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0,
      p=dHzbZapWIk4jUhN+Ute9ytag9zjfMHgsqmmiz7AndVQ=
   S: v=6rriTRBi23WpRR/wtup+mMhUZUn/dB5nLTJRsjl95G4=
   ```
* 額外擴充改密碼機制:
  * 改密碼訊息:
   ```
   C: n,,n=user,r=rOprNGfwEbeRWgbNEkqO
   S: r=rOprNGfwEbeRWgbNEkqO%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0,
      s=W22ZaJ0SNY7soEsUEjb6gQ==,i=4096
   C: c=biws,r=rOprNGfwEbeRWgbNEkqO%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0,
      s=,p=OldProof
   S: s=NewSALT,i=NewITERATOR,v=OldVerify
   C: h=XSaltedPassword,p=NewProof
   S: h=NewVerify
   ```
  * OldProof        := SASL 計算 Proof 的算法, 但此時 AuthMessage := (SASL 的 AuthMessage) + ",s="
  * OldVerify       := SASL 計算 Proof 的算法, 但此時 AuthMessage := (SASL 的 AuthMessage) + ",s=NewSALT,i=NewITERATOR"
  * XSaltedPassword := NewSaltedPassword XOR OldSaltedPassword
  * NewAuthMessage  := (SASL 的 AuthMessage) + ",s=NewSALT,i=NewITERATOR"
  * NewProof        := SASL 計算 Proof 的算法:  使用 NewSaltedPassword 及 NewAuthMessage.
  * NewVerify       := SASL 計算 Verify 的算法: 使用 NewSaltedPassword 及 NewAuthMessage.
  * 可以參考 [實際的範例](../fon9/fon9/crypto/Crypto_UT.cpp)

---------------------------------------

## 擴充模組管理

---------------------------------------

## 通訊管理

