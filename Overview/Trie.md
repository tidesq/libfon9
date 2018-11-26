# fon9 library - Trie
* [`fon9/Trie.hpp`](../fon9/Trie.hpp)
---------------------------------------
## [Trie wiki](https://zh.wikipedia.org/wiki/Trie)
---------------------------------------
## 適合使用的地方
* 資料大部分為有序時，與 `std::map` 比較起來， `fon9::Trie` 有更高的效率，甚至比 `std::unordered_map` 的記憶體用量還小!
* 但是當 **資料很亂(無序)** ，`fon9::Trie` 會用掉大量記憶體! 且效率不佳! 因此 **完全沒有實用價值!!**
  * trie 還有很多的變種(例如:結合 hash table)，真的要用到時再來研究吧!
* 所以您必須先了解資料，然後再選用適合的MAP: `std::map`, `std::unordered_map`, `fon9::Trie`...
* 瞭解您要處理的資料，選擇適合的演算法，是非常重要的事!
* 實際應用
  * 委託書號對照表。
  * 商品Id對照表，則要用更多的測試數據來決定。
---------------------------------------
## benchmark
  * 底下是針對「委託書號」的需求，比較 `fon9::Trie`, `std::map`, `std::unordered_map` 的結果  
    key = "xxxxx", x='0'..'9', 'A'..'Z', 'a'..'z'  
    測試的範圍: "00000".."04C91" 共 1,000,000 筆。
  * Hardware: HP ProLiant DL380p Gen8 / E5-2680 v2 @ 2.80GHz
  * OS: ESXi 6.5.0 Update 1 (Build 5969303) / VM: Windows server 2016(1607)  
    Compiler: MSVC 2015(VC 19.00.24215.1)

|sequence result   |   emplace |    find   | lower_bound | upper_bound |     clear | MemUsed(KB)|
|------------------|----------:|----------:|------------:|------------:|----------:|-----------:|
|std::map          |802.741843 |449.052918 |  486.212887 |  431.460726 | 62.050347 |     83,476 |
|std::unordered_map|157.072675 | 68.101009 |   68.096243 |   85.420087 | 47.033490 |     84,045 |
|fon9::Trie        | 89.984570 | 31.244948 |   33.093690 |   48.856203 | 24.401706 |     70,684 |

PS. VC 的 std::unordered_map 有提供 lower_bound()?!
```
===== std::map / sequence =====
emplace:    : 0.802741843 secs / 1,000,000 times = 802.741843000 ns|MemUsed(KB)=83,476
find:       : 0.449052918 secs / 1,000,000 times = 449.052918000 ns
lower_bound:: 0.486212887 secs / 1,000,000 times = 486.212887000 ns
upper_bound:: 0.431460726 secs / 1,000,000 times = 431.460726000 ns
clear:      : 0.062050347 secs / 1,000,000 times =  62.050347000 ns
----- random -----
emplace:    : 0.528697658 secs / 1,000,000 times = 528.697658000 ns|MemUsed(KB)=83,902
find:       : 0.476586818 secs / 1,000,000 times = 476.586818000 ns
lower_bound:: 0.517537753 secs / 1,000,000 times = 517.537753000 ns
upper_bound:: 0.456214731 secs / 1,000,000 times = 456.214731000 ns
clear:      : 0.196555027 secs / 1,000,000 times = 196.555027000 ns

===== std::unordered_map / sequence =====
emplace:    : 0.157072675 secs / 1,000,000 times = 157.072675000 ns|MemUsed(KB)=84,045
find:       : 0.068101009 secs / 1,000,000 times =  68.101009000 ns
lower_bound:: 0.068096243 secs / 1,000,000 times =  68.096243000 ns
upper_bound:: 0.085420087 secs / 1,000,000 times =  85.420087000 ns
clear:      : 0.047033490 secs / 1,000,000 times =  47.033490000 ns
----- random -----
emplace:    : 0.357968561 secs / 1,000,000 times = 357.968561000 ns|MemUsed(KB)=66,269
find:       : 0.194656795 secs / 1,000,000 times = 194.656795000 ns
lower_bound:: 0.195942080 secs / 1,000,000 times = 195.942080000 ns
upper_bound:: 0.252508156 secs / 1,000,000 times = 252.508156000 ns
clear:      : 0.072468629 secs / 1,000,000 times =  72.468629000 ns

===== fon9::Trie / sequence =====
emplace:    : 0.089984570 secs / 1,000,000 times =  89.984570000 ns|MemUsed(KB)=70,684
find:       : 0.031244948 secs / 1,000,000 times =  31.244948000 ns
lower_bound:: 0.033093690 secs / 1,000,000 times =  33.093690000 ns
upper_bound:: 0.048856203 secs / 1,000,000 times =  48.856203000 ns
clear:      : 0.024401706 secs / 1,000,000 times =  24.401706000 ns
----- random -----
emplace:    : 3.729660631 secs / 1,000,000 times =3729.660631    ns|MemUsed(KB)=5,338,128 <<< !!! 5G RAM !!!!
find:       : 0.466949752 secs / 1,000,000 times = 466.949752000 ns
lower_bound:: 0.502332465 secs / 1,000,000 times = 502.332465000 ns
upper_bound:: 1.022260788 secs / 1,000,000 times =1022.260788    ns
clear:      : 4.661768948 secs / 1,000,000 times =4661.768948    ns
```
  * OS: ESXi 6.5.0 Update 1 (Build 5969303) / VM: ubuntu16 4.4.0-62-generic  
    Compiler: g++ (Ubuntu 5.4.0-6ubuntu1~16.04.4) 5.4.0 20160609

|sequence result   |   emplace |    find   | lower_bound | upper_bound |     clear | MemUsed(KB)|
|------------------|----------:|----------:|------------:|------------:|----------:|-----------:|
|std::map          |692.118592 |338.259092 |  315.363225 |  316.052648 |  37.277879|     78,144 |
|std::unordered_map| 96.016328 | 45.392071 |             |             |  16.116401|     70,700 |
|fon9::Trie        | 58.471083 | 17.124011 |   14.237141 |   22.987077 |   6.484917|     65,076 |

```
===== std::map / sequence =====
emplace:    : 0.692118592 secs / 1,000,000 times = 692.118592000 ns|MemUsed(KB)=78,144
find:       : 0.338259092 secs / 1,000,000 times = 338.259092000 ns
lower_bound:: 0.315363225 secs / 1,000,000 times = 315.363225000 ns
upper_bound:: 0.316052648 secs / 1,000,000 times = 316.052648000 ns
clear:      : 0.037277879 secs / 1,000,000 times =  37.277879000 ns
----- random -----
emplace:    : 0.401727078 secs / 1,000,000 times = 401.727078000 ns|MemUsed(KB)=0
find:       : 0.359170193 secs / 1,000,000 times = 359.170193000 ns
lower_bound:: 0.347638025 secs / 1,000,000 times = 347.638025000 ns
upper_bound:: 0.346473082 secs / 1,000,000 times = 346.473082000 ns
clear:      : 0.092911807 secs / 1,000,000 times =  92.911807000 ns

===== std::unordered_map / sequence =====
emplace:    : 0.096016328 secs / 1,000,000 times =  96.016328000 ns|MemUsed(KB)=70,700
find:       : 0.045392071 secs / 1,000,000 times =  45.392071000 ns
clear:      : 0.016116401 secs / 1,000,000 times =  16.116401000 ns
----- random -----
emplace:    : 0.095964429 secs / 1,000,000 times =  95.964429000 ns|MemUsed(KB)=0
find:       : 0.094167182 secs / 1,000,000 times =  94.167182000 ns
clear:      : 0.016466146 secs / 1,000,000 times =  16.466146000 ns

===== fon9::Trie / sequence =====
emplace:    : 0.058471083 secs / 1,000,000 times =  58.471083000 ns|MemUsed(KB)=65,076
find:       : 0.017124011 secs / 1,000,000 times =  17.124011000 ns
lower_bound:: 0.014237141 secs / 1,000,000 times =  14.237141000 ns
upper_bound:: 0.022987077 secs / 1,000,000 times =  22.987077000 ns
clear:      : 0.006484917 secs / 1,000,000 times =   6.484917000 ns
----- random -----
emplace:    : 1.838889597 secs / 1,000,000 times =1838.889597    ns|MemUsed(KB)=4,864,992 <<< !!! 4.8G RAM !!!!
find:       : 0.180241002 secs / 1,000,000 times = 180.241002000 ns
lower_bound:: 0.178123713 secs / 1,000,000 times = 178.123713000 ns
upper_bound:: 0.514068714 secs / 1,000,000 times = 514.068714000 ns
clear:      : 0.857940227 secs / 1,000,000 times = 857.940227000 ns
```
