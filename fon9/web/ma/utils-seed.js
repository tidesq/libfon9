/**
 * @return 若 key 為 undefined || null 傳回 ''; 若為空字串傳回 '""';
 *         若 key 裡面有特殊字元('/', ' ', '^'), 則傳回 addQuots(key);
 *         其餘傳回 key;
 */
function normalizeKeyText(key) {
  if (key == undefined || key == null)
    return '';
  if (key.length <= 0)
    return '""';
  if (key[0]=='.' || key.indexOf('/') >= 0 || key.indexOf(' ') >= 0 || key.indexOf('^') >= 0)
    return addQuotes(key);
  return key;
}

/**
 * @return 若 value 為 undefined || null 傳回 ''; 若為空字串傳回 '""';
 *         若 value 裡面有「空白 或 ','」或 value[0]為引號,
 *         則傳回沒用到的引號框起的 value, 例如: '`' + value '`';
 *         其餘傳回 value;
 */
function normalizeValue(value) {
  if (value == undefined || value == null)
    return '';
  if (value.length <= 0)
    return '""';
  if (value.indexOf(' ') >= 0 || value.indexOf(',') >= 0)
    return addQuotes(value);
  switch(value[0]) {
  case '"':
  case '`':
  case "'":
    return addQuotes(value);
  }
  return value;
}

/**
 * @return 將 value 頭尾加上引號.
 */
function addQuotes(value) {
  if (value.indexOf('`') < 0)
    return '`' + value + '`';
  if (value.indexOf('"') < 0)
    return '"' + value + '"';
  return "'" + value + "'";
}

/**
 * @return 合併 path + normalizeKeyText(key) + tabName
 *         若 path 尾端沒有 '/' 則加上,
 *         若 tabName 不是空的, 則加上 '^'
 */
function mergePathKeyTab(path, key, tabName) {
  if (path.length <= 0 || path[path.length - 1] != '/')
     path += '/';
  path += normalizeKeyText(key);
  if (tabName != '')
    path += '^' + tabName;
  return path;
}

/**
 * @return 將 path 解析成 [{seed:'', tab:''}]
 */
function parsePath(path) {
  let res = [];
  let start = 0;
  while (start < path.length) {
    let ch = path[start];
    if (ch == '/') {
      if (++start >= path.length)
        break;
      ch = path[start];
    }
    let node = {seed:'', tab:''};
    switch(ch) {
    case "'":
    case '"':
    case '`':
      let pquot = path.indexOf(ch, ++start);
      if (pquot < 0) {
        res.push({seed:path.substring(start), tab:''});
        break;
      }
      node.seed = path.substring(start, pquot);
      ch = path[start = pquot + 1];
      break;
    }
    let pspl = path.indexOf('/', start);
    let subs = pspl < 0 ? path.substring(start) : path.substring(start, pspl);
    let ptab = subs.indexOf('^');
    if (ptab >= 0) {
      node.tab = subs.substring(ptab+1);
      subs = subs.substring(0, ptab);
    }
    node.seed += subs;
    res.push(node);
    if (pspl < 0)
      break;
    start = pspl + 1;
  }
  return res;
}
