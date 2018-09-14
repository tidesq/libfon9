import SaslScram from './sasl-scram.js'

/**
 * .onclose       = function(evt);
 * .onmessage     = function(msg);  // msg=WebSocket message.
 * .onerror       = function(msg);  // msg=SASL error.
 * .onpasschanged = function(msg);  // success pass changed.
 * .onsigned      = function(msg);  // success signed.
*/
export default function SaslWebSocket() {
// private:
  let connection;
  let cred;
  let mech; // = 'SCRAM-SHA-256';
  let scram;// = new SaslScram('SHA-256');
  let challenging = false;
  let pthis = this;

  let oncloseHandler = function(evt) {
    // 因為可能在處理 '認證失敗' 的訊息時, 收到斷線事件.
    // 此時應先處理 '認證失敗', 所以延遲一會兒再處理斷線事件.
    connection = undefined;
    if (pthis.onclose)
      setTimeout(() =>  pthis.onclose(evt), 50);
  }
  let onSaslChallenge = function(msg) {
    if (challenging) {
      // 在 challenging 過程收到的訊息, 必定是已認證成功後的 [立即訊息].
      pthis.onmessage(msg);
      return;
    }
    if (pthis.onchallenge)
       pthis.onchallenge(msg.data);
    challenging = true;
    let saslRes;
    if (!scram) {
      // msg.data = "SASL: mechList" 使用 ' ' 分隔 mechList
      // 目前固定使用 'SCRAM-SHA-256'
      mech = 'SCRAM-SHA-256';
      connection.send(mech);
      if (pthis.onresponse)
         pthis.onresponse(mech);
      scram = new SaslScram('SHA-256');
      saslRes = scram.initial(cred);
    }
    else {
      saslRes = scram.challenge(msg.data);
    }
    Promise.resolve(saslRes).then((res) => {
      if (typeof res === 'string') {
        challenging = false; // 在送出 response 之前必須先將 challenging 旗標關閉.
        connection.send(res);
        if (pthis.onresponse)
           pthis.onresponse(res);
        return;
      }
      if (res.hasOwnProperty('signed')) {
        connection.onmessage = pthis.onmessage;
        challenging = false; // 認證成功, 設定新的 onmessage 之後, 才能關閉 challenging 旗標.
        if (pthis.onsigned)
           pthis.onsigned(res.signed);
        return;
      }
      challenging = false;
      if (res.hasOwnProperty('passChanged')) {
        if (pthis.onpasschanged)
          pthis.onpasschanged(res.passChanged);
      }
      else if (res.hasOwnProperty('err')) {
        pthis.err = res.err;
        connection.close();
        if (pthis.onerror)
           pthis.onerror(res.err);
      }
    });
  }

// public:
  /**
   * @param credit {username:'user', password:'pass'}
   *        如果要改密碼, 則須設定 credit.NewPass
   */
  this.open = function(url, credit) {
    cred = credit;
    connection = new WebSocket(url);
    connection.onmessage = (msg => onSaslChallenge(msg));
    connection.onclose = (evt => oncloseHandler(evt));
  }
  this.close = function() {
    if (connection)
      connection.close();
  }
  this.send = function(msg) {
    connection.send(msg);
  }
};
