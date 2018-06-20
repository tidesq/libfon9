libfon9 系統管理工具
=======================

## 基本說明
* 提供一組工具介面，存取 AP 的資料。
  * 用來查看 AP 的現在狀態。
  * 改變執行期間的設定。
  * 執行特定任務。
  * 管理 AP。
* 盡量在不影響既有程式的情況下提供功能。
  * 只要能完成 TreeOp、PodOp... 就能納入管理。

---------------------------------------

## 概念

### 容器

#### Raw
* cell 的容器：一個 Raw instance 可包含 0 到多個 cell，透過 Tab 的 `Fields_` 來描述(存取)。
* Raw：記憶體中儲存一筆資料的基礎類別。
* Raw 交給 Seed 來管理。
* e.g.
```c++
using Pri = Decimal<int64_t,6>;
//  其中的 HHMMSS_, Pri_, Qty_ 都是可透過 Field 存取的資料。
class Deal {
   uint32_t HHMMSS_;
   Pri      Pri_;
   uint32_t Qty_;
};
```
* 資料型別(例如上述例子的 `class Deal;`)不一定要繼承 `class Raw;`。
* 繼承 `class Raw;` 的目的是提供動態欄位：
  * 不必像上述的 class Deal，欄位不用寫在 class 或 struct 裡面，可根據設定建立動態欄位。
  * 但擁有動態欄位的資料物件，必須使用 `MakeDyMemRaw();` 來建立，建立時分配所需的額外儲存空間。

#### Seed
* raw 的容器：Seed 負責管理 Raw，不同類型的 Seed 可能會有 0 到多個 Raw，例如：
  * `class 委託書;` 用多個 `class 委託異動明細 : public Raw;` 來記錄異動歷史。
* Seed 只是一個概念，實際上並 **沒有** `class Seed;`，只在需要時透過 PodOp 來操作這個概念。
* 管理 0 或 1 個 Sapling，如果 Seed 會長成一個樹。

#### Pod
* seed 的容器：Pod 只是一個概念，實際上並 **沒有** `class Pod;`，只在需要時透過 TreeOp、PodOp 來操作這個概念。
* Pod 將一組 Seed 打包起來，然後經由 Tree，透過一個 Key 取得。
* 透過 Layout 的描述，可以知道一個 Pod 裡面有幾個 Seed，及其對應的 Tab。
* e.g. 底下的 Ivac 就是一個 Pod 的概念
```c++
struct Ivac {
   IvacBasic   Basic_;  // 帳號基本資料.
   IvacBalMap  BalMap_; // 帳號庫存表.
   IvacOrdMap  OrdMap_; // 帳號委託表.
};
```

#### Tree
* pod 的容器
* Tree 實際上可以只參考到「既有資料表」，然後實作 TreeOp 對「既有資料表」操作。
* 因此對於 Tree 的使用者而言，Tree 更像是一個容器的代理，透過 Tree 才操作、觀察「既有資料表」。
* 包含一個 `Layout_` 用來描述此樹 Pod 的內容。

---------------------------------------

### 資料描述

#### Layout
* 描述 Tree 所建立的 Pod。
* Layout 包含 1 到多個 Tab。
* Layout 包含 1 個 KeyField。


#### Tab
* 用來描述 Seed(Raw)。
* Tab 包含：
  * `Fields_` 欄位列表：包含 0 到多個 Field，描述 Seed 裡面 Raw 的內容。
  * `SaplingLayout_`：若 Seed 所生成的 Sapling(Tree) 外觀相同，則可將這類 Tree 的描述放在此處。

#### Field
* 透過 Field 可用來存取 Raw instance 裡面的一個 cell(欄位)。
* Fields：欄位列表，描述一個 Raw 的可用欄位。
* e.g. 成交 Fields：
```c++
  fields.Add(new FieldTimeStamp("Time", ...));
  fields.Add(new FieldDecimal<int64_t,6>("Pri", ...));
  fields.Add(new FieldUint32("Qty", ...));
```

---------------------------------------

### 操作

#### TreeOp、PodOp
#### 查看、修改、新增、刪除 Tree、Pod、Seed、Raw、Cell
#### 執行指令
