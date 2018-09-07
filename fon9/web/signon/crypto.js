import * as utils from './utils.js';

const Crypto = window.crypto;
const CryptoSubtle = Crypto.subtle;
if(CryptoSubtle === undefined)
  alert('Cryptography API not Supported. Must use localhost or https.');

/**
 * PBKDF2 運算.
 * @param params = {
 *   password:   password in string
 *   hash:       'SHA-1' or 'SHA-256'
 *   salt:       salt in BufferArray
 *   iterations: i
 *   bitsSize:   the number of bits you want to output
 * }
 * @return {Promise} Uint8Array: PBKDF2 運算後的資料.
 */
export function pbkdf2(params) {
  params.name = 'PBKDF2';
  return CryptoSubtle.importKey(
            'raw',
            utils.strToUint8Array(params.password),
            {name: 'PBKDF2'},
            false,
            ['deriveKey', 'deriveBits']
  ).then(key => CryptoSubtle.deriveBits(params, key, params.bitsSize)
  ).then(bits => new Uint8Array(bits)
  );
}

/**
 * HMAC 運算.
 * @param {string}      hash 'SHA-1' or 'SHA-256'
 * @param {BufferArray} key
 * @param {string}      msg
 * @return {Promise} BufferArray: HMAC 運算後的資料.
 */
export function hmac(hash, key, msg) {
  return CryptoSubtle.importKey(
     'raw', key,
     {name:'HMAC', hash:{name:hash}},
     false, ["sign"]
  ).then(hkey => CryptoSubtle.sign({name:'HMAC'}, hkey, utils.strToUint8Array(msg))
  );
}

/**
 * hash 運算.
 * @param {string}      hash 'SHA-1' or 'SHA-256'
 * @param {BufferArray} msg 要用來 hash 的資料.
 * @return {Promise} BufferArray: hash(msg) 運算後的資料.
 */
export function calcHash(hash, msg) {
  return CryptoSubtle.digest({name:hash}, msg);
}
