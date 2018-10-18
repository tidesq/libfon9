// 建立一個顯示 grid view 的 table:
// css 請參考 gv.css
// <div class="gvArea">
//    <table class="gvTable">
//       <thead></thead>
//       <tbody></tbody>
//    </table>
// </div>

// 處理 gvTable 的鍵盤操作: ← ↑ → ↓ Move cell focus; 取消編輯、確認編輯內容...
function gvTableKeyDown(gv, ev) {
   if (window.event)
      ev = window.event;
   if (gv.onkeydown && !gv.onkeydown(ev))
      return false;
   let cell = document.activeElement;
   let rowIndex = cell.parentNode.rowIndex;
   if (cell.isContentEditable) { // 在 edit mode 才需要考慮的按鍵.
      if (ev.keyCode == 27)      // esc: 取消編輯, 還原成編輯前內容.
         gv.cancelCellEdit(cell);
      else if (ev.keyCode == 13) // enter: 確認編輯內容.
         gv.confirmCellEdit(cell);
      else
         return true;
      return false;
   }

   // 沒有在 edit mode 才要判斷的按鍵(e.g. 上下左右..).
   if (ev.keyCode == 33)   // PgUp
      return !pageCellFocus(gv, -1, rowIndex, cell.cellIndex);
   if (ev.keyCode == 34)   // PgDn
      return !pageCellFocus(gv, 1, rowIndex, cell.cellIndex);

   if (ev.keyCode == 35) { // End
      if (ev.ctrlKey)
         return !gv.setCellFocus(gv.gvTable.rows.length, cell.cellIndex);
      return !gv.setCellFocus(rowIndex, Number.MAX_SAFE_INTEGER);
   }
   if (ev.keyCode == 36) { // Home
      if (ev.ctrlKey)
         return !gv.setCellFocus(0, cell.cellIndex);
      return !gv.setCellFocus(rowIndex, 0);
   }

   if (ev.keyCode == 37) // left
      return !gv.setCellFocus(rowIndex, cell.cellIndex - 1);
   if (ev.keyCode == 38) // up
      return !gv.setCellFocus(rowIndex - 1, cell.cellIndex);
   if (ev.keyCode == 39) // right
      return !gv.setCellFocus(rowIndex, cell.cellIndex + 1);
   if (ev.keyCode == 40) // down
      return !gv.setCellFocus(rowIndex + 1, cell.cellIndex);

   if (cell.cellIndex == 0) { // @KeyField: click/enter/space
      if (ev.keyCode != 32 && ev.keyCode != 13)
         return true;
      // space or enter key: to sapling gv.
      emitKeyCellClick(gv, cell);
      return false;
   }
   if (gv.isCellEditable && gv.isCellEditable(cell)) {
      if (ev.keyCode == 13 || ev.keyCode == 113) {
         // [Enter] or [F2]: into edit mode.
         gv.startCellEdit(cell);
         return false;
      }
   }
   return true;
}
// PageUp(direction = -1) or PageDown(direction = +1)
function pageCellFocus(gv, direction, rowIndex, cellIndex) {
   let bodyiStart = gv.getFirstBodyRowIndex();
   if (bodyiStart < 0)
      return true;
   let rows = gv.gvTable.rows;
   let hcli = gv.gvArea.clientHeight - rows[bodyiStart].offsetTop;
   let hmov = 0;
   for (;;) {
      if (direction < 0) {
         if (rowIndex <= bodyiStart)
            break;
      }
      else if (rowIndex + 1 >= rows.length)
         break;
      hmov += rows[rowIndex += direction].offsetHeight;
      if (hmov == hcli)
         break;
      if (hmov > hcli) {
         hmov -= rows[rowIndex -= direction].offsetHeight;
         break;
      }
   }
   gv.gvArea.scrollTop += hmov * direction;
   return gv.setCellFocus(rowIndex, cellIndex);
}
// 讓 cell 離開編輯模式.
function setExitEditMode(cell) {
   cell.blur();
   cell.focus();
}

// 處理 cell focus 事件.
function gvTableCellFocus(gv, cell) {
   if (gv.selectedCell) {
      gv.selectedCell.classList.remove("gvSelectedCellEditable");
      gv.selectedCell.classList.remove("gvSelectedCellReadonly");
   }
   gv.selectedCell = cell;
   if (!gv.isCellEditable(cell))
      gv.selectedCell.classList.add("gvSelectedCellReadonly");
   else {
      gv.selectedCell.classList.add("gvSelectedCellEditable");
      // 進入 focus 之後, 再 double click cell 才進入 edit 模式.
      cell.ondblclick = (ev => gv.startCellEdit(ev.target));
      cell.onblur = (ev => gvTableCellBlur(gv, ev.currentTarget));
   }
   if (gv.onSelectedCellChanged)
      gv.onSelectedCellChanged(cell);
}
// 處理 cell blur 事件.
function gvTableCellBlur(gv, cell) {
   if (gv.beforeEditText != undefined)
      emitEditConfirm(gv, cell);
   gv.beforeEditText = undefined;
   cell.ondblclick = undefined;
   cell.onblur = undefined;
   cell.removeAttribute("contentEditable");
   document.execCommand("Unselect");
}

// 當 key cell 被按下, 觸發 onKeyCellClick() 事件.
function emitKeyCellClick(gv, cell) {
   if (gv.onKeyCellClick)
      gv.onKeyCellClick(cell);
}
// 觸發Edit異動訊息.
function emitEditConfirm(gv, cell) {
   if (gv.onEditConfirm && gv.beforeEditText != cell.textContent)
      gv.onEditConfirm(cell, gv.beforeEditText);
}

function onclickSelectCell(gv, ev) {
   if (gv.selectedCell)
      gv.selectedCell.focus();
   else
      gv.setFirstCellFocus();
}

/** 建立 gv table.
 *
 * \code
 *  let gv = new GridView(document.body);
 * \endcode
 *
 * properties:
 *  - selectedCell;     // 被選中的 cell, 與 browser 是否在 focus 無關.
 *  - beforeEditText;   // 進入編輯模式前所保留的「編輯前資料」.
 *
 * events:
 *  - onkeydown(ev);
 *    - 同一般元素的 onkeydown(ev); 傳回 true 表示按鍵沒有被處理, false 表示按鍵已被處理.
 *    - document.activeElement 為 cell 或 gvTable(當 body 沒有 rows 時)
 *    - if (cell.isContentEditable) { 表示正在編輯中; }
 *  - onKeyCellClick(keycell);
 *    - key cell 被按下(click, enter, space).
 *  - onSelectedCellChanged(cell);
 *    - 選中另一個 cell 時的通知.
 *  - onEditConfirm(cell, beforeText);
 *    - 編輯完成且有異動的通知, 此時 cell.textContent 已是編輯後的資料.
 *  - isCellEditable(cell);
 *    - 檢查 cell 是否允許編輯.
 *
 */
class GridView {
   constructor(parent) {
      this.gvArea = document.createElement("div");
      this.gvArea.className = "gvArea";
      this.gvArea.onmouseup = (ev => onclickSelectCell(this, ev));

      this.gvTable = document.createElement("table");
      this.gvTable.className = "gvTable";

      this.gvTable.appendChild(document.createElement("thead"));
      this.gvTable.appendChild(document.createElement("tbody"));
      this.gvTable.onkeydown = (ev => gvTableKeyDown(this, ev));
      this.gvArea.appendChild(this.gvTable);
      parent.appendChild(this.gvArea);
   }

   /** 清除包含 head, body 的表格內容. */
   clearAll() {
      this.selectedCell = undefined;
      this.gvTable.tHead.innerHTML = "";
      this.gvTable.tBodies[0].innerHTML = "";
   }
   /** 清除 table body, 保留 table head. */
   clearBody() {
      this.gvTable.tBodies[0].innerHTML = "";
   }
   /** head 加上一行,  strHeadRow = <th></th>... */
   addHeadRow(strHeadRow) {
      this.gvTable.tHead.insertRow(-1).innerHTML = strHeadRow;
   }
   /** 取得 head 的行數 */
   getHeadRowCount() {
      return this.gvTable.rows.length - this.gvTable.tBodies[0].rows.length;
   }
   /** 取得 table body 第一行在 this.gvTable.rows[i] 的位置, 如果沒有 body 則傳回 -1 */
   getFirstBodyRowIndex() {
      let bodyRowCount = this.gvTable.tBodies[0].rows.length;
      return bodyRowCount <= 0 ? -1 : (this.gvTable.rows.length - bodyRowCount);
   }
   /** 在 table body 加上 row (其中包含使用 <th> 加入的 key cell), 傳回 row 物件.
    *  - 您必須再呼叫 addBodyCell(row, fldstr) 加入 cell.
    *  - 設定 row.id = keyText;
    *  - 設定 key cell 的必要屬性及事件處理函式.
    */
   addBodyRow(keyText) {
      let row = this.gvTable.tBodies[0].insertRow(-1);
      let cell = row.insertCell(-1);
      cell.outerHTML = "<th>" + keyText + "</th>";
      cell = row.cells[0]; // 因為使用 outerHTML, cell 已經不是 row 的 cell, 所以要重新取得.
      cell.tabIndex = 0;
      cell.onfocus = (ev => gvTableCellFocus(this, ev.currentTarget));
      cell.ondblclick = (ev => emitKeyCellClick(this, ev.currentTarget));
      row.id = keyText;
      return row;
   }
   /** 在 body row 尾端增加一個 UI cell, 並設定相關事件及屬性.
    *  返回新增加的 cell.
    */
   addBodyCell(row, fldValue) {
      let cell = row.insertCell(-1);
      cell.textContent = fldValue;
      cell.tabIndex = 0;
      cell.onfocus = (ev => gvTableCellFocus(this, ev.currentTarget));
      return cell;
   }
   /** 從 gvTable 移除 row, 若有需要會自動調整 focus cell. */
   removeRow(row) {
      if (!row)
         return;
      let rowParent = row.parentNode;
      let ridx = row.rowIndex;
      if (ridx == this.selectedCell.parentNode.rowIndex) {
         this.setCellFocus(ridx + 1 >= rowParent.rows.length ? (ridx - 1) : (ridx + 1),
                           this.selectedCell.cellIndex);
      }
      rowParent.removeChild(row);
      if (this.gvTable.tBodies[0].rows.length <= 0)
         this.setFirstCellFocus();
   }
   /** 用 key 尋找 row. */
   searchRow(key) {
      return this.gvTable.rows.namedItem(key);
   }
   /** 取得 cell 所在行的 key text. */
   getKeyText(cell) {
      if (!cell)
         return "";
      let ridx = cell.parentNode.rowIndex;
      return ridx <= 0 ? "" : this.gvTable.rows[ridx].cells[0].textContent;
   }
   /** 取得 table body 左上角第一個 cell 的 textContent.
    *  若 body 為空, 則傳回 undefined.
    */
   getFirstCellText() {
      let rowsBody = this.gvTable.tBodies[0].rows;
      return(rowsBody.length > 0 ? rowsBody[0].cells[0].textContent : undefined);
   }
   /** 將第一個 cell 設定為 focus.
    *  如果沒有 body, 只有 head, 則會將 gvTable 設為 focus, 因為這樣才能收到 keydown 事件.
    */
   setFirstCellFocus() {
      let rowsBody = this.gvTable.tBodies[0].rows;
      if (rowsBody.length > 0) {
         rowsBody[0].cells[0].focus();
         this.gvTable.removeAttribute("tabIndex");
      }
      else {
         this.selectedCell = undefined;
         this.gvTable.tabIndex = 0;
         this.gvTable.focus();
      }
   }
   /** 設定 gvTable 的焦點 cell;
    *  rowIndex 的計算包含 head row, 但 head 無法 focus.
    *  也就是如果 head 只有一行, 則最左上角的 cell 為 (1,0);
    */
   setCellFocus(rowIndex, cellIndex) {
      let bodyiStart = this.getFirstBodyRowIndex();
      if (bodyiStart < 0)
         return true;
      if (rowIndex < bodyiStart)
         rowIndex = bodyiStart;
      if (cellIndex < 0)
         cellIndex = 0
      let rows = this.gvTable.rows;
      if (rowIndex >= rows.length)
         rowIndex = rows.length - 1;
      let row = rows[rowIndex];
      if (cellIndex >= row.cells.length)
         cellIndex = row.cells.length - 1;
      // 如果 focus row 往上移動(且有需要捲動),
      // 因為最上面的那行可能被 head row 遮住,
      // 但 scroll bar 可能不認為需要往下捲,
      // 所以這裡必須自己計算, 排除被 head row 遮住的情況.
      let headHeight = rows[bodyiStart].offsetTop;
      if (row.offsetTop < this.gvArea.scrollTop + headHeight)
         this.gvArea.scrollTop = row.offsetTop - headHeight;
      if (cellIndex <= 0)
         this.gvArea.scrollLeft = 0;
      rows[rowIndex].cells[cellIndex].focus();
      return true;
   }
   /** 讓 cell 進入編輯模式, 開始編輯 cell 內容. */
   startCellEdit(cell) {
      if (this.selectedCell && this.selectedCell !== cell)
         this.selectedCell.blur();
      this.beforeEditText = cell.textContent;
      cell.ondblclick = undefined;
      cell.contentEditable = true;
      cell.focus();
      // 啟用輸入 & 將輸入游標移動到最後.
      let range = document.createRange();
      range.selectNodeContents(cell);
      range.collapse(false);
      let sel = window.getSelection();
      sel.removeAllRanges();
      sel.addRange(range);
   }
   /** 確認 cell 的編輯內容, 若有異動則觸發 onEditConfirm(cell, beforeText), 然後離開編輯模式. */
   confirmCellEdit(cell) {
      emitEditConfirm(this, cell);
      setExitEditMode(cell);
   }
   /** 取消編輯內容, 並離開編輯模式. */
   cancelCellEdit(cell) {
      cell.textContent = this.beforeEditText;
      setExitEditMode(cell);
   }
};

export {GridView, GridView as default};
