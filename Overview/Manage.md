libfon9 基礎建設程式庫 - 系統管理基礎建設
=========================================

## 基本說明

---------------------------------------

## User Auth 使用者管理機制
* 使用 seed 機制管理資料表: user、role、policy... 
* 提供 Acl(Access Control List) policy
  * Rights 使用 bit OR 運算合併底下權限:
    * 0x01 Read
    * 0x02 Write
    * 0x04 Exec
    * 0x80 Apply
    * 例: 僅可編輯,不可套用: 0x03
    * 例: 不可編輯,僅能套用: 0x81
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
* 由於 RFC 沒有提供改密碼機制(?)，所以 fon9 自行擴充:
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
  * 可以參考 [實際的範例](../fon9/crypto/Crypto_UT.cpp)

---------------------------------------

## 擴充模組管理

---------------------------------------

## 通訊管理
* IoManager
  * 那些東西有記錄 log?
    * DeviceState/dtor: 使用 TRACE 紀錄.
    * SessionState: 使用 INFO 紀錄.
  * Id 可為任意 ASCII 字串.

---------------------------------------

## 執行程式 Fon9Co
* `Fon9Co` 是使用 fon9 建立的執行程式。
* 啟動參數:
  * 設定工作目錄: `-w dir` or `--workdir dir`
  * 設定檔路徑:   `-c cfgpath` or `--cfg cfgpath` or `getenv("fon9cfg");` or 預設值="fon9cfg"
    * 其他啟動參數放在: fon9local.cfg, fon9common.cfg:
    * Log檔設定, 如果沒設定 $LogFileFmt, 則 log 就輸出在 console
      * $LogFileFmt=./logs/{0:f+'L'}/fon9sys-{1:04}.log  # 超過 {0:f+'L'}=YYYYMMDD(localtime), {1:04}=檔案序號.
      * $LogFileSizeMB=n                                 # 超過 n MB 就換檔.
    * $HostId     沒有預設值, 如果沒設定, 就不會設定 LocalHostId_
    * $SyncerPath 指定 InnSyncerFile 的路徑, 預設 = "fon9syn"
    * $MaAuthName 預設 "MaAuth"
    * $MemLock    預設 "N"
  * 啟動時進入 admin 模式: `Fon9Co --admin`
  * SysEnv: 揭示啟動時的各項參數, 啟動後執行查看指令 `gv /SysEnv` 輸出範例:
```
Name       |Title                           |Description                   |Value                              
CommandLine|                                |                              |./release/fon9/Fon9Co              
ConfigPath |                                |                              |fon9cfg                            
ExecPath   |                                |                              |/home/fonwin/devel/output/fon9     
HostId     |Local HostId                    |                              |16                                 
LogFileFmt |                                |                              |./logs/{0:f+'L'}/fon9sys-{1:04}.log
MemLock    |mlockall(MCL_CURRENT|MCL_FUTURE)|err=:12:Cannot allocate memory|Y                                  
SyncerPath |                                |                              |fon9syn/                           
= 7/7 ==== the end ===
```

* Fon9Co 操作指令:

  | command                              | description
  |--------------------------------------|----------------------------
  | seedPath(第一個字元為「~ . / ' "」)  | change current seed path
  | ss,fld1=val1,fld2=val2... [seedPath] | set seed field value, seed 若不存在則會建立, 建立後再設定 field 的值.
  | rs                        [seedPath] | remove seed(or pod)
  | ps                        [seedPath] | print seed field values
  | pl                        [treePath] | print layout
  | gv[,rowCount,startKey]    [treePath] | grid view
  | gv+                                  | continue previous grid view

* 例如首次啟動, 建立一個管理員Id = "fonwin":
```console
$ ~/devel/output/fon9/release/fon9/Fon9Co --admin
Fon9Co admin mode.
[[AdminMode]] />ss,RoleId=admin,Flags=0 /MaAuth/UserSha256/fonwin
UserId=fonwin
RoleId=admin
AlgParam=0
Flags=x0
NotBefore=
NotAfter=
ChgPassTime=
ChgPassFrom=
LastAuthTime=
LastAuthFrom=
LastErrTime=
LastErrFrom=
ErrCount=0

[[AdminMode]] />/MaAuth/UserSha256/fonwin repw test
The new password is: test
[[AdminMode]] />ss,HomePath=/    /MaAuth/PoAcl/admin
PolicyId=admin
HomePath=/

[[AdminMode]] />ss,Rights=xff    /MaAuth/PoAcl/admin/'/'
Path=/
Rights=xff

[[AdminMode]] />ss,Rights=xff    /MaAuth/PoAcl/admin/'/..'
Path=/..
Rights=xff
[[AdminMode]] />exit


Fon9Co login: fonwin
Password: ****

[fonwin] />
[fonwin] />quit

```
