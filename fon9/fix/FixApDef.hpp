/// \file fon9/fix/FixApDef.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_FixApDef_hpp__
#define __fon9_fix_FixApDef_hpp__
#include "fon9/fix/FixBase.hpp"

namespace fon { namespace fix {

#define f9fix_kTAG_Account                     1
#define f9fix_kTAG_Symbol                     55
#define f9fix_kTAG_Side                       54
#define f9fix_kTAG_TransactTime               60
#define f9fix_kTAG_OrderQty                   38
#define f9fix_kTAG_OrdType                    40 // Price Type.
#define f9fix_kTAG_TimeInForce                59
#define f9fix_kTAG_Price                      44
#define f9fix_kTAG_HandlInst                  21
#define f9fix_kTAG_OrderID                    37
#define f9fix_kTAG_ClOrdID                    11
#define f9fix_kTAG_OrigClOrdID                41

#define f9fix_kTAG_ExecID                     17
#define f9fix_kTAG_LastPx                     31
#define f9fix_kTAG_LastQty                    32
#define f9fix_kTAG_LeavesQty                 151
#define f9fix_kTAG_CumQty                     14
#define f9fix_kTAG_AvgPx                       6

#define f9fix_kTAG_CxlRejResponseTo          434
#define f9fix_kTAG_CxlRejReason              102
#define f9fix_kTAG_OrdRejReason              103

#define f9fix_kTAG_ExecTransType             20 // FIX.4.2
#define f9fix_kVAL_ExecTransType_New         "0"
#define f9fix_kVAL_ExecTransType_Status      "3"

#define f9fix_kTAG_ExecType                  150
#define f9fix_kVAL_ExecType_New              "0"
#define f9fix_kVAL_ExecType_PartialFill_42   "1"
#define f9fix_kVAL_ExecType_Fill_42          "2"
#define f9fix_kVAL_ExecType_DoneForDay       "3"
#define f9fix_kVAL_ExecType_Canceled         "4"
#define f9fix_kVAL_ExecType_Replace          "5"
#define f9fix_kVAL_ExecType_PendingCancel    "6"
#define f9fix_kVAL_ExecType_Stopped          "7"
#define f9fix_kVAL_ExecType_Rejected         "8"
#define f9fix_kVAL_ExecType_Suspended        "9"
#define f9fix_kVAL_ExecType_PendingNew       "A"
#define f9fix_kVAL_ExecType_Calculated       "B"
#define f9fix_kVAL_ExecType_Expired          "C"
#define f9fix_kVAL_ExecType_Restated         "D"
#define f9fix_kVAL_ExecType_PendingReplace   "E"
#define f9fix_kVAL_ExecType_Trade_44         "F"//FIX.4.4: Trade (partial fill or fill)
#define f9fix_kVAL_ExecType_TradeCorrect     "G"//FIX.4.4: Trade Correct (formerly an ExecTransType <20> (20))
#define f9fix_kVAL_ExecType_TradeCancel      "H"//FIX.4.4: (formerly an ExecTransType <20>)
#define f9fix_kVAL_ExecType_OrderStatus      "I"//FIX.4.4: (formerly an ExecTransType <20>)

/// \ingroup fix
/// OrderStatus, 台灣證交所:
/// - FIX.4.2: = ExecType(Tag#150).
/// - FIX.4.4: 除底下說明, 其餘與 ExecType(Tag#150) 相同:
///   - 改單成功: ExecType=Replace; OrdStatus=New
///   - 查詢成功: ExecType=Status;  OrdStatus=New
///   - 成交回報: ExecType=Trade;   部份成交 OrdStatus=PartialFill; 全部成交 OrdStatus=Fill
#define f9fix_kTAG_OrdStatus                 39
#define f9fix_kVAL_OrdStatus_New             "0"
#define f9fix_kVAL_OrdStatus_PartialFill     "1"
#define f9fix_kVAL_OrdStatus_Fill            "2"
#define f9fix_kVAL_OrdStatus_DoneForDay      "3"
#define f9fix_kVAL_OrdStatus_Canceled        "4"
#define f9fix_kVAL_OrdStatus_Replaced_42     "5"
#define f9fix_kVAL_OrdStatus_PendingCancel   "6"
#define f9fix_kVAL_OrdStatus_Stopped         "7"
#define f9fix_kVAL_OrdStatus_Rejected        "8"
#define f9fix_kVAL_OrdStatus_Suspended       "9"
#define f9fix_kVAL_OrdStatus_PendingNew      "A"
#define f9fix_kVAL_OrdStatus_Calculated      "B"
#define f9fix_kVAL_OrdStatus_Expired         "C"
#define f9fix_kVAL_OrdStatus_AcceptedForBidding "D"
#define f9fix_kVAL_OrdStatus_PendingReplace  "E"

// TWSE fields.
#if 1
#define f9fix_kTAG_TwseIvacnoFlag            10000
#define f9fix_kTAG_TwseOrdType               10001

// '0'=Regular, '3'=Foreign stock’s order price over up/down limit flag.
#define f9fix_kTAG_TwseExCode                10002

// Regular Checks the TransactTime to verify that it is within a given seconds of the system time.
// Y: if not, reject it.
// N: don’t check TransactTime.
#define f9fix_kTAG_TwseRejStaleOrd           10004
#endif// TWSE fields.

#define f9fix_kMSGTYPE_NewOrderSingle        "D"
#define f9fix_kMSGTYPE_OrderReplaceRequest   "G"
#define f9fix_kMSGTYPE_OrderCancelRequest    "F"
#define f9fix_kMSGTYPE_OrderStatusRequest    "H"

#define f9fix_kMSGTYPE_ExecutionReport       "8"
#define f9fix_kMSGTYPE_OrderCancelReject     "9"

} } // namespaces
#endif//__fon9_fix_FixApDef_hpp__
