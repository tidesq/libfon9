* [signal slot benchmarks 測試方法](https://github.com/NoAvailableAlias/signal-slot-benchmarks)
* Hardware: HP ProLiant DL380p Gen8 / E5-2680 v2 @ 2.80GHz
* OS: ESXi 6.5.0 Update 1 (Build 5969303) / VM: Windows server 2016(1607)
* Compiler: MSVC 2015(VC 19.00.24215.1)
```
|Library             |construct| destruct|  connect| emission| combined| threaded|    total|remark              |
|--------------------|--------:|--------:|--------:|--------:|--------:|--------:|--------:|--------------------|
|fon9 Subr(unsafe)   |   142481|    23947|    91249|   102294|    21286|        0|   381256|fon9::SortedVector  |
|jeffomatic jl_signal|    88905|    18115|    75864|   102838|    14533|        0|   300255|- doubly linked list|
|Wink-Signals        |   140437|    14633|     9224|   104196|     5973|        0|   274463|- std::vector       |
|* fon9 Subr         |   135997|    13230|    22643|    75526|     8781|     8730|   264907|fon9::SortedVector  |
|fr00b0 nod(unsafe)  |   130654|    17912|     9250|    99259|     5831|        0|   262906|std::vector         |
|Yassi               |   138509|    10212|     6586|    98377|     3898|        0|   257582|- std::vector       |
|nano-signal-slot    |   129170|     9844|    10728|   101943|     5135|        0|   256819|singly linked list  |
|mwthinker Signal    |   112989|     8174|     5708|    99572|     3353|        0|   229796|- std::list         |
|amc522 Signal11     |   107979|     9318|     6136|    90435|     3605|        0|   217473|?                   |
|* fr00b0 nod        |   112100|    11088|     6714|    76774|     3949|     6395|   217019|std::vector         |
|pbhogan Signals     |   113128|     8372|     6587|    70468|     3795|        0|   202348|std::set            |
|joanrieu signal11   |    94092|    12640|     7866|    79183|     4358|        0|   198138|- std::list         |
|* Kosta signals-cpp |   134793|     8008|     1693|    22216|     1269|       13|   167993|std::vector         |
|* lsignal           |    72778|     4853|     2532|    74512|     1607|     1748|   158031|- std::list         |
|EvilTwin Observer   |   110881|     5552|     2299|    36245|     1550|        0|   156527|- std::vector       |
|supergrover sigslot |    13941|     2535|     3058|    98130|     1241|        0|   118905|- std::list         |
|* winglot Signals   |    12718|     3870|     4727|    72682|     1860|     1907|    97764|- std::list         |
|* neosigslot        |    12446|     4401|     3658|     9618|     1451|     1777|    33351|std::map            |
|Boost Signals       |     9562|     2908|     1068|    14051|      672|        0|    28260|?                   |
|* Boost Signals2    |     8975|     3125|     1667|     7811|      839|      995|    23413|?                   |
```
* remark 欄位有標註「-」的 library ，表示不支援在 emitting 時取消註冊。
* fon9 Subr 的限制：收到發行訊息時，允許重進入發行，但不允許：在重進入發行時取消註冊。
* fon9 Subr 不考慮 Subject 比 Subscriber 早死。
