/// \file fon9/auth/AuthBase.h
///
/// \defgroup auth 認證機制
///
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_AuthBase_h__
#define __fon9_auth_AuthBase_h__
#ifdef __cplusplus
extern "C" {
#endif

/// \ingroup auth
/// 身分認證的結果代碼.
typedef enum fon9_Auth_R : int {
   /// 身分認證成功.
   fon9_Auth_Success = 0,

   /// 認證還需要其他資料.
   /// - 多階段認證: 部分成功.
   /// - 還需要依傳回的提示訊息, 處理後續的認證作業.
   /// - 需提供額外認證資料, 例: "請輸入簡訊的認證碼"
   fon9_Auth_NeedsMore,
   /// 等候額外(外部)認證, 例: "請檢查email並確認登入"
   fon9_Auth_Waiting,

   /// 密碼更改成功.
   fon9_Auth_PassChanged,
   /// 已認證成功的 Request, 後續可能的通知訊息.
   fon9_Auth_OnlineNotify,
   /// 強制登出通知.
   fon9_Auth_ForceLogout,
   /// AuthSession::Dispose() 處理完畢後的通知.
   /// 收到此通知後 AuthSession 就不會再有任何訊息了.
   fon9_Auth_Disposed,

   /// 不認識的認證機制.
   fon9_Auth_EArgs_Mechanism = -1,
   /// 新密碼錯誤.
   fon9_Auth_EArgs_NewPass = -2,
   /// 認證訊息格式錯誤.
   fon9_Auth_EArgs_Format = -3,

   /// 不認識的 User 或 Pass.
   fon9_Auth_EUserPass = -100,
   /// 使用者尚未生效.
   fon9_Auth_ENotBefore = -101,
   /// 使用者已失效.
   fon9_Auth_ENotAfter = -102,
   /// 使用者已鎖定(禁止登入: 密碼錯誤次數超過? 短時間登入次數超過?).
   fon9_Auth_EUserLocked = -103,
   /// 使用者必須改密碼.
   fon9_Auth_ENeedChgPass = -104,
   /// 不允許的使用者來源(例:ip黑名單).
   fon9_Auth_EUserFrom = -105,

   /// 不支援的功能.
   fon9_Auth_ENoSupport = -200,
   /// 不支援改密碼.
   fon9_Auth_ENoSupport_ChgPass = -201,
   /// 認證失敗, 參考字串訊息.
   fon9_Auth_EOther = -202,
   /// 認證訊息不正確.
   fon9_Auth_EProof = -203,

   /// AuthMgr已死亡, 程式正在結束中?
   fon9_Auth_EInternal_NoAuthMgr = -300,
} fon9_Auth_R;

inline bool fon9_IsAuthError(fon9_Auth_R res) {
   return res < fon9_Auth_Success;
}

#ifdef __cplusplus
}// extern "C"
#endif
#endif//__fon9_auth_AuthBase_h__
