const encUtf8 = new TextEncoder();

/**
 * @return {Uint8Array} 字串 str 轉為 Uint8Array.
*/
export function strToUint8Array(str) {
  return encUtf8.encode(str);
}

const b64KeyStr = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

/**
 * @param  {(string|Uint8Array)} 要轉成 Base64 的原始資料.
 * @return {string} 返回 Base64 字串.
 */
export function b64Encode(input) {
  if(typeof input === 'string' || input instanceof String)
     input = strToUint8Array(input);
  else if(typeof input != 'Uint8Array')//ArrayBuffer
     input = new Uint8Array(input);
  let output = "";
  let chr1, chr2, chr3, enc1, enc2, enc3, enc4;
  let i = 0;
  const len = input.length;
 
  while (i < len) {
    chr1 = input[i++];
    chr2 = i < len ? input[i++] : Number.NaN;
    chr3 = i < len ? input[i++] : Number.NaN;
 
    enc1 = chr1 >> 2;
    enc2 = ((chr1 & 3) << 4) | (chr2 >> 4);
    enc3 = ((chr2 & 15) << 2) | (chr3 >> 6);
    enc4 = chr3 & 63;
 
    if (isNaN(chr2)) {
      enc3 = enc4 = 64;
    } else if (isNaN(chr3)) {
      enc4 = 64;
    }
    output += b64KeyStr.charAt(enc1) + b64KeyStr.charAt(enc2) + 
              b64KeyStr.charAt(enc3) + b64KeyStr.charAt(enc4);
  }
  return output;
}

/**
 * @param  {string} input 為 Base64 字串.
 * @return {Uint8Array} 返回解碼後的 bytes.
 */
export function b64Decode(input) {
  let bytes = (input.length / 4) * 3;
  let res = new Uint8Array(bytes);
  let chr1, chr2, chr3;
  let enc1, enc2, enc3, enc4;
  let i = 0;
  let j = 0;
  for (i=0; i < bytes; i += 3) {	
    enc1 = b64KeyStr.indexOf(input.charAt(j++));
    enc2 = b64KeyStr.indexOf(input.charAt(j++));
    enc3 = b64KeyStr.indexOf(input.charAt(j++));
    enc4 = b64KeyStr.indexOf(input.charAt(j++));
    chr1 = (enc1 << 2) | (enc2 >> 4);
    chr2 = ((enc2 & 15) << 4) | (enc3 >> 2);
    chr3 = ((enc3 & 3) << 6) | enc4;
    res[i] = chr1;
    //若尾端為'=', 則應刪減 res 的大小.
    if (enc3 != 64) res[i+1] = chr2;
    else return res.slice(0,i+1);
    if (enc4 != 64) res[i+2] = chr3;
    else return res.slice(0,i+2);
  }
  return res;	
}

/**
 * @param  {Integer} uint8ArraySize 亂碼的 bytes 數量.
 * @return {string}  將亂碼轉為 Base64 後的字串.
 */
export function genNonce(uint8ArraySize = 12) {
  let rnds = new Uint8Array(uint8ArraySize);
  window.crypto.getRandomValues(rnds);
  return b64Encode(rnds);
}

/**
 * @param  {BufferArray} b1
 * @param  {BufferArray} b2
 * @return {BufferArray} 將 b1, b2 的內容做 xor 運算後返回.
 */
export function xor(b1, b2) {
  let i = 0;
  let v1 = new Uint8Array(b1);
  let v2 = new Uint8Array(b2);
  return v1.map(x => x ^ v2[i++]);
}
