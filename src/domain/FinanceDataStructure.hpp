#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace finance::domain
{
    /*  @ Struct Name: SummaryData
    @ Description:
        * 代表融資融券交易的摘要數據
    */
    struct SummaryData
    {
        int64_t margin_available_amount = 0;
        int64_t margin_available_qty = 0;
        int64_t short_available_amount = 0;
        int64_t short_available_qty = 0;
        int64_t after_margin_available_amount = 0;
        int64_t after_margin_available_qty = 0;
        int64_t after_short_available_amount = 0;
        int64_t after_short_available_qty = 0;
        std::string stock_id;
        std::string area_center;
        std::vector<std::string> belong_branches;

        // 新增資買賣互抵張數 (temp data)
        int64_t margin_buy_offset_qty;
        int16_t short_sell_offset_qty;
    };

    /* @ Struct Name: MessageDataHCRTM01
    @ Description:
        * Transaction code ELD001-> HCRTM01
        - 描述交易代碼 ELD001 映射至 HCRTM01 的帳單額度數據結構。
        - 包含有關證券 融資券額度 與 成交金額 的詳細資料。
    */
    struct MessageDataHCRTM01
    {
        char broker_id[4];                           // 券商代號broker_id
        char area_center[3];                         // 區中心代號area_center
        char stock_id[6];                            // 證券代號stock_id
        char financing_company[4];                   // 證金公司financing_company
        char margin_amount[11];                      // 融資控管額度margin_amount
        char margin_buy_order_amount[11];            // 資買委託融資金margin_buy_order_amount
        char margin_sell_match_amount[11];           // 資賣成交融資金margin_sell_match_amount
        char margin_qty[6];                          // 融資控管張數margin_qty
        char margin_buy_order_qty[6];                // 資買委託張數	margin_buy_order_qty
        char margin_sell_match_qty[6];               // 資賣成交張數margin_sell_match_qty
        char short_amount[11];                       // 融券控管額度short_amount
        char short_sell_order_amount[11];            // 券賣委託價金short_sell_order_amount
        char short_buy_match_amount[11];             // 券買成交價金short_buy_match_amount
        char short_qty[6];                           // 融券控管張數short_qty
        char short_sell_order_qty[6];                // 券賣委託張數short_sell_order_qty
        char short_buy_match_qty[6];                 // 券買成交張數	short_buy_match_qty
        char popular_margin_mark[1];                 // 融資熱門股註記popular_margin_mark
        char popular_short_mark[1];                  // 融券熱門股註記popular_short_mark
        char remark[12];                             // 備註remark
        char edit_date[8];                           // 異動日期	edit_date
        char edit_time[6];                           // 異動時間	edit_time
        char editor[10];                             // 異動人員editor
        char margin_buy_match_amount[11];            // 資買成交融資金margin_buy_match_amount
        char margin_buy_match_qty[6];                // 資買成交張數margin_buy_match_qty
        char margin_after_hour_buy_order_amount[11]; // 資買盤後委託融資金margin_after_hour_buy_order_amount
        char margin_after_hour_buy_order_qty[6];     // 資買盤後委託張數margin_after_hour_buy_order_qty
        char short_sell_match_amount[11];            // 券賣成交價金short_sell_match_amount
        char short_sell_match_qty[6];                // 券賣成交張數short_sell_match_qty
        char short_after_hour_sell_order_amount[11]; // 券賣盤後委託價金short_after_hour_sell_order_amount
        char short_after_hour_sell_order_qty[6];     // 券賣盤後委託張數short_after_hour_sell_order_qty
        char day_trade_margin_buy_match_amount[11];  // 當沖資買成交金額day_trade_margin_buy_match_amount
        char day_trade_short_sell_match_amount[11];  // 當沖券賣成交金額day_trade_short_sell_match_amount
    };

    /*  @ Struct Name: MessageDataHCRTM05P
    @ Description:
        * Transaction code ELD002-> HCRTM05P
        - 描述交易代碼 ELD002 映射至 HCRTM05P 的帳單額度數據結構。
        - 包含有關證券 資券成交張數 與 資券可互抵數 的詳細資料。
    */
    struct MessageDataHCRTM05P
    {
        char dummy[1];                          // dummy
        char broker_id[2];                      // 券商代號 broker_id
        char dummy2[1];                         // dummy2
        char stock_id[6];                       // 證券代號 stock_id
        char financing_company[4];              // 證金公司 financing_company
        char account[7];                        // 客戶帳號 account + 帳號檢碼 check
        char margin_buy_match_qty[6];           // 資買成交張數 margin_buy_match_qty
        char short_sell_match_qty[6];           // 券賣成交張數 short_sell_match_qty
        char day_trade_margin_match_qty[6];     // 資沖成交張數 day_trade_margin_match_qty
        char day_trade_short_match_qty[6];      // 券沖成交張數 day_trade_short_match_qty
        char margin_buy_offset_qty[6];          // 資買互抵張數 margin_buy_offset_qty
        char short_sell_offset_qty[6];          // 券賣互抵張數 short_sell_offset_qty
        char comment[12];                       // 備註 comment
        char edit_date[8];                      // 異動日期 edit_date
        char edit_time[6];                      // 異動時間 edit_time
        char author[10];                        // 登錄人員 author
        char force_margin_buy_match_qty[6];     // 資買成交張數－強制 force_margin_buy_match_qty
        char force_short_sell_match_qty[6];     // 券賣成交張數－強制 force_short_sell_match_qty
        char in_quota_margin_buy_offset_qty[6]; // 資可互抵數－佔配額 in_quota_margin_buy_offset_qty
        char in_quota_short_sell_offset_qty[6]; // 券可互抵數－佔配額 in_quota_short_sell_offset_qty
    };

    /*  @ Struct Name: ApData
    @ Description:
        * Ap Data
        - 包括文件操作、系統識別碼和交易詳細資料的元資料。
        - 利用union存資料結構（`MessageDataHCRTM01`、`MessageDataHCRTM05P` 或原始緩衝區資料）。
    */
    struct ApData
    {
        char jrnseqn[10];
        char system[8];
        char lib[10];
        char file[10];
        char member[10];
        char file_rrnc[10];
        char entry_type[1];   // 操作 -> ('F' : 清盤) , ('C' : 新增) , ('A' : 更新) , ('D' : 刪除)
        char rcd_len_cnt[10]; // Data所帶電文長度

        union
        {
            MessageDataHCRTM01 hcrtm01;   // HCRTM01 資券個股額度檔
            MessageDataHCRTM05P hcrtm05p; // HCRTM05P 資券個股額度檔
            char buffer[4000];            // 71~7000 Raw Data
        } data;
    };

    /*  @ Struct Name: ApData
    @ Description:
        * 傳輸完整電文
        - 包括輸入輸出、Transaction code和 來源主機(應改為CB) ，Ap資料。
    */
    struct FinancePackageMessage
    {
        char p_code[4];     // 0200: Input, 0210: Output
        char t_code[6];     // 前3碼：系統代碼 末3碼：格式代碼 -> ELD001-> HCRTM01, ELD002-> HCRTM05P
        char srcid[3];      // source : CA/CB -> 應該為 CB
        char timestamp[26]; // 時間搓
        char filler[61];    // 空白
        ApData ap_data;     // AP 資料
    };

} // namespace finance::domain