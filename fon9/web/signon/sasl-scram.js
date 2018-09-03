import * as utils from './utils.js';
import * as crypto from './crypto.js';

const CLIENT_KEY = 'Client Key';
const SERVER_KEY = 'Server Key';

function parseChallenge(chal) {
  let dtives = {};
  let tokens = chal.split(/,(?=(?:[^"]|"[^"]*")*$)/);
  for (let tk of tokens) {
    let dtiv = /(\w+)=["]?([^"]+)["]?$/.exec(tk);
    if (dtiv) {
      dtives[dtiv[1]] = dtiv[2];
    }
  }
  return dtives;
}

async function makeProof(mech) {
  let clientKey = await crypto.hmac(mech.hash, mech._saltedPassword, CLIENT_KEY);
  let storedKey = await crypto.calcHash(mech.hash, clientKey);
  let clientSignature = await crypto.hmac(mech.hash, storedKey, mech._authMessage);
  return ',p=' + utils.b64Encode(utils.xor(clientKey, clientSignature));
}
async function makeVerify(mech) {
  let serverKey = await crypto.hmac(mech.hash, mech._saltedPassword, SERVER_KEY);
  return utils.b64Encode(await crypto.hmac(mech.hash, serverKey, mech._authMessage));
}

async function feed1stChallenge(mech, chal) {
  let values = parseChallenge(chal);
  mech.salt = utils.b64Decode(values.s || '');
  mech.iterations = values.i;
  if (values.hasOwnProperty('e'))
    return {err: values.e};

  mech._clientFinalMessageWithoutProof = 'c=' + utils.b64Encode(mech._gs2Header) + ',r=' + values.r;
  if (mech.hasOwnProperty('NewPass')) // fon9: for change password.
    mech._clientFinalMessageWithoutProof += ',s=';
  mech._saltedPassword = await crypto.pbkdf2(mech);
  mech._authMessage = mech._clientFirstMessageBare + ',' +
                      chal + ',' +
                      mech._clientFinalMessageWithoutProof;
  return mech._clientFinalMessageWithoutProof + await makeProof(mech);
}
async function feed2ndChallenge(mech, chal) {
  let values = parseChallenge(chal);
  if (values.hasOwnProperty('e'))
    return {err: values.e};
  if (!mech.hasOwnProperty('NewPass')) {
    if (values.v != await makeVerify(mech))
      return {err: 'Unknown SASL challenge: ' + chal};
    return {signed: values.v};
  }
  // change pass: "s=NewSALT,i=NewITERATOR,v=verifier"
  // fon9 自己的擴充, for change password.
  // 送出: 新密碼訊息.
  mech._authMessage += values.s + ',' + 'i=' + values.i;
  if (values.v != await makeVerify(mech))
    return {err: 'ChangePass: err verify for old pass'};
  mech.password = mech.NewPass;
  mech.iterations = values.i;
  mech.salt = utils.b64Decode(values.s);
  let newSaltedPass = await crypto.pbkdf2(mech);
  let climsg = 'h=' + utils.b64Encode(utils.xor(newSaltedPass, mech._saltedPassword));
  mech._saltedPassword = newSaltedPass;
  return climsg + await makeProof(mech);
}
async function feed3rdChallenge(mech, chal) {
  let values = parseChallenge(chal);
  if (values.hasOwnProperty('e'))
    return {err: values.e};
  if (!mech.hasOwnProperty('NewPass'))
    return {err: 'Unknown SASL challenge: ' + chal};
  if (values.h != await makeVerify(mech))
    return {err: 'ChangePass: err verify for new pass'};
  return {passChanged: values.h};
}

class SaslScram {
  constructor(hashName) {
    this.hash = hashName;
    this.genNonce = utils.genNonce;
    if (hashName=='SHA-1')
      this.bitsSize = 160;
    else {
       let ns = hashName.split('-');
       if(ns.length >= 2) {
          ns = ns[1].split('/');
          this.bitsSize = ns.length >= 2 ? ns[1] : ns[0];
       }
    }
  }
  /**
   * @cred {username:'user', password:'pass'}
   *       如果要改密碼, 則須設定 cred.NewPass
   * @return 返回要傳遞給 server 的字串.
   */
  initial(cred) {
    if (cred.hasOwnProperty('NewPass'))
       this.NewPass = cred.NewPass;
    this.password = cred.password || '';
    this._stage = 0;
    this._cnonce = this.genNonce();
    let authzid = '';
    if (cred.authzid)
        authzid = 'a=' + utils.saslname(cred.authzid);
    this._gs2Header = 'n,' + authzid + ',';
    let nonce = 'r=' + this._cnonce;
    let username = 'n=' + (cred.username || '');
    this._clientFirstMessageBare = username + ',' + nonce;
    return this._gs2Header + this._clientFirstMessageBare;
  }
  /**
   * @return {Promise:string} 要傳遞給 server 的字串.
   * @return {Promise:object} retval.hasOwnProperty('signed') 登入成功.
   *                          retval.hasOwnProperty('passChanged') 改密碼成功.
   *                          retval.hasOwnProperty('err') 登入失敗 retval.err 為錯誤訊息.
   */
  challenge(chal) {
    this._stage++;
    switch(this._stage) {
    case 1: return feed1stChallenge(this, chal);
    case 2: return feed2ndChallenge(this, chal);
    case 3: return feed3rdChallenge(this, chal);
    }
    return 'Unknown challenge stage';
  }
};

export {SaslScram, SaslScram as default};
