#include <Poco/Net/TCPServer.h>
#include <Poco/Net/TCPServerConnection.h>
#include <Poco/Net/StreamSocket.h>
#include <sstream>
#include <memory>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <future>
#include "json.hpp"
#include <boost/format.hpp>
#include <sw/redis++/redis++.h>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <map>
#include <set>
#include <Poco/Buffer.h>
#include <Poco/DateTime.h>
#include <Poco/Timezone.h>
#include <fstream>
#include <iterator>
#define FMT_HEADER_ONLY 1
#define LOGURU_USE_FMTLIB 1
#include "loguru.h"
#include <cctype>

#define CHECK_MARX_NEW new

using namespace boost::algorithm;

using json = nlohmann::json;

using namespace sw::redis;
using namespace std::literals;
enum
{
    AMOUNT_LEN = 11,
    QTY_LEN = 6,
    MAX_BUF_LEN = 409800,
};
static std::map<std::string, std::string> following_broker_ids =
    {
        {""s, ""s}};

static std::set<std::string> backoffice_ids;
static std::vector<std::string> branches;

static json area_branch_map;
/*
struct hcrtm01_meta_t
{
    std::string area_center;
    std::string stock_id;
    std::string financing_company;
    int64_t short_avaliable_amount;
    int64_t margin_avaliable_amount;
    std::vector<std::string > broker_ids;
};
*/

struct hcrtm01_no_null_data_t
{
    char broker_id[4];
    char area_center[3];
    char stock_id[6];
    char financing_company[4];
    char margin_amount[AMOUNT_LEN];
    char margin_buy_order_amount[AMOUNT_LEN];
    char margin_sell_match_amount[AMOUNT_LEN];
    char margin_qty[QTY_LEN];
    char margin_buy_order_qty[QTY_LEN];
    char margin_sell_match_qty[QTY_LEN];
    char short_amount[AMOUNT_LEN];
    char short_sell_order_amount[AMOUNT_LEN];
    char short_buy_match_amount[AMOUNT_LEN];
    char short_qty[QTY_LEN];
    char short_sell_order_qty[QTY_LEN];
    char short_buy_match_qty[QTY_LEN];
    char popular_margin_mark[1];
    char popular_short_mark[1];
    char remark[12];
    char edit_date[8];
    char edit_time[6];
    char editor[10];
    char margin_buy_match_amount[AMOUNT_LEN];
    char margin_buy_match_qty[QTY_LEN];
    char margin_after_hour_buy_order_amount[AMOUNT_LEN];
    char margin_after_hour_buy_order_qty[QTY_LEN];
    char short_sell_match_amount[AMOUNT_LEN];
    char short_sell_match_qty[QTY_LEN];
    char short_after_hour_sell_order_amount[AMOUNT_LEN];
    char short_after_hour_sell_order_qty[QTY_LEN];
    char day_trade_margin_buy_match_amount[AMOUNT_LEN];
    char day_trade_short_sell_match_amount[AMOUNT_LEN];
};

struct hcrtm05p_no_null_data_t
{
    char dummy[1];
    char broker_id[2];
    char dummy2[1];
    char stock_id[6];
    char financing_company[4];
    char account[7];
    char margin_buy_match_qty[6];
    char short_sell_match_qty[6];
    char day_trade_margin_match_qty[6];
    char day_trade_short_match_qty[6];
    char margin_buy_offset_qty[6];
    char short_sell_offset_qty[6];
    char comment[12];
    char edit_date[8];
    char edit_time[6];
    char author[10];
    char force_margin_buy_match_qty[6];
    char force_short_sell_match_qty[6];
    char in_quota_margin_buy_offset_qty[6];
    char in_quota_short_sell_offset_qty[6];
};

#define STRINGIFY(a) (std::string((a), sizeof(a)))

struct ap_data_no_null_t
{
    char jrnseqn[10];
    char system[8];
    char lib[10];
    char file[10];
    char member[10];
    char file_rrnc[10];
    char enttype[1];
    char rcd_len_cnt[10];
    union
    {
        hcrtm01_no_null_data_t hcrtm01;
        hcrtm05p_no_null_data_t hcrtm05p;
        char buf[4000];
    } ap_data;
};

struct finance_bill_no_null_t
{
    char pcode[4];
    char tcode[6];
    char srcid[3];
    char timestamp[26];
    char filler[61];
    ap_data_no_null_t ap_no_null;
};

static std::queue<char *> finance_bill_queue;
static std::mutex queue_lock;
static std::mutex packet_lock;
static std::condition_variable cv;

void Enqueue(char *const data)
{
    { // unlock before notify condition variable
        std::unique_lock<std::mutex> lock1(queue_lock);
        finance_bill_queue.push(data);
    }
    cv.notify_one();
}

bool TryDequeue(char *&data)
{
    using namespace std::literals;
    std::unique_lock<std::mutex> lock1(queue_lock);

    if (!cv.wait_for(lock1, 1ms, []
                     { return !finance_bill_queue.empty(); }))
        return false;

    data = finance_bill_queue.front();
    finance_bill_queue.pop();
    return true;
}

Poco::Buffer<char> buffered(MAX_BUF_LEN);
class FinanceBillQuotaServerConnection : public Poco::Net::TCPServerConnection
{
public:
    FinanceBillQuotaServerConnection(const Poco::Net::StreamSocket &s) : Poco::Net::TCPServerConnection(s)
    {
    }
    void run()
    {
        try
        {

            Poco::Buffer<char> buffer(sizeof(finance_bill_no_null_t));
            buffer.resize(0);
            while (true)
            {
                auto recv_len = socket().receiveBytes(buffer);
                if (recv_len > 0)
                {
                    std::unique_lock<std::mutex> lock1(packet_lock);
                    LOG_F(INFO, "receive data = {}", buffer.size());
                    buffered.append(buffer);
                    buffer.resize(0);
                }
            }
        }
        catch (std::exception &ex)
        {
            LOG_F(ERROR, "TCP connection error : {}", ex.what());
        }
    }
};

class FinanceBillQuotaServer : public Poco::Net::TCPServerConnectionFactory
{
public:
    inline Poco::Net::TCPServerConnection *
    createConnection(const Poco::Net::StreamSocket &socket)
    {
        return CHECK_MARX_NEW FinanceBillQuotaServerConnection(socket);
    }
};

std::string GetKeyBy(const hcrtm01_no_null_data_t &hcrtm01)
{
    auto stock_id = STRINGIFY(hcrtm01.stock_id);
    auto area_center = STRINGIFY(hcrtm01.area_center);
    trim_right(stock_id);
    trim_right(area_center);
    return "summary:"s +
           area_center +
           ":" +
           stock_id;
}

std::string GetAreaCenterByBranchID(const std::string &branch_id)
{
    auto i = following_broker_ids.find(branch_id);
    if (i == following_broker_ids.end())
    {
        return "";
    }
    return i->second;
}

std::string GetKeyBy(const hcrtm05p_no_null_data_t &hcrtm05p)
{
    auto broker_id = STRINGIFY(hcrtm05p.broker_id);
    auto stock_id = STRINGIFY(hcrtm05p.stock_id);
    trim_right(broker_id);
    trim_right(stock_id);
    return "summary:"s +
           broker_id +
           ":" +
           stock_id;
}

struct summary_data_t
{
    int64_t margin_available_amount;
    int64_t margin_available_qty;
    int64_t short_available_amount;
    int64_t short_available_qty;
    int64_t after_margin_available_amount;
    int64_t after_margin_available_qty;
    int64_t after_short_available_amount;
    int64_t after_short_available_qty;
    std::string stock_id;
    std::string area_center;
    std::vector<std::string> belong_branches;

    // 新增資買賣互抵張數
    int64_t margin_buy_offset_qty;
    int16_t short_sell_offset_qty;
};

std::map<char, int> backoffice_int_map =
    {
        {'J', 1},
        {'K', 2},
        {'L', 3},
        {'M', 4},
        {'N', 5},
        {'O', 6},
        {'P', 7},
        {'Q', 8},
        {'R', 9},
        {'}', 0}};

long long BackOfficeInt(const std::string &value)
{
    auto last_char = value.back();
    if (isdigit(last_char))
    {
        return std::stoi(value);
    }

    auto part1 = std::stoi(value.substr(0, value.length() - 1));
    auto part2 = backoffice_int_map[last_char];
    return -1 * (part1 * 10 + part2);
}

void FillBelongBranches(std::vector<std::string> &vec, const std::string &area_center)
{
    if (vec.size() > 0)
    {
        return;
    }

    if (auto i = backoffice_ids.find(area_center);
        backoffice_ids.end() == i)
    {
        return;
    }
    try
    {
        vec = area_branch_map[area_center].get<std::vector<std::string>>();
    }
    catch (const std::exception &ex)
    {
        LOG_F(ERROR, "illegal area center:{}", area_center);
    }
}

class DataHandler final
{
    std::map<std::string, summary_data_t> summary_datas;
    std::mutex map_lock;
    std::string _redis_url;

    void update_company_summary(const std::string &stock_id)
    {

        struct summary_data_t company_summary;
        for (auto &backoffice_id : backoffice_ids)
        {
            auto key = "summary:"s + backoffice_id + ":" + stock_id;

            auto i = summary_datas.find(key);

            if (summary_datas.end() == i)
            {
                continue;
            }
            auto &[_, area_summary_data] = *i;
            company_summary.stock_id = area_summary_data.stock_id;
            company_summary.area_center = "ALL";
            company_summary.belong_branches = branches;
            company_summary.margin_available_amount += area_summary_data.margin_available_amount;
            company_summary.margin_available_qty += area_summary_data.margin_available_qty;
            company_summary.short_available_amount += area_summary_data.short_available_amount;
            company_summary.short_available_qty += area_summary_data.short_available_qty;
            company_summary.after_margin_available_amount += area_summary_data.after_margin_available_amount;
            company_summary.after_margin_available_qty += area_summary_data.after_margin_available_qty;
            company_summary.after_short_available_amount += area_summary_data.after_short_available_amount;
            company_summary.after_short_available_qty += area_summary_data.after_short_available_qty;
        }
        auto all_key = "summary:ALL:" + stock_id;
        sync_to_redis(all_key, company_summary);
    }

    void dump_summary_data(const summary_data_t &sdt)
    {
        LOG_F(INFO, "margin amount:{}, margin qty:{}, short amount:{}, short qty:{}, stock_id:{}, area_code:{}",
              sdt.margin_available_amount, sdt.margin_available_qty,
              sdt.short_available_amount, sdt.short_available_qty,
              sdt.stock_id, sdt.area_center);
    }
    void sync_to_redis(const std::string &key, const summary_data_t &sdt)
    {
        json j;
        j["stock_id"] = sdt.stock_id;
        j["area_center"] = sdt.area_center;
        j["margin_available_amount"] = sdt.margin_available_amount;
        j["margin_available_qty"] = sdt.margin_available_qty;
        j["short_available_amount"] = sdt.short_available_amount;
        j["short_available_qty"] = sdt.short_available_qty;
        j["after_margin_available_amount"] = sdt.after_margin_available_amount;
        j["after_margin_available_qty"] = sdt.after_margin_available_qty;
        j["after_short_available_amount"] = sdt.after_short_available_amount;
        j["after_short_available_qty"] = sdt.after_short_available_qty;
        j["belong_branches"] = sdt.belong_branches;
        auto json_string = j.dump();
        try
        {
            auto redis = Redis(this->_redis_url);
            redis.command<void>(
                "JSON.SET",
                key,
                "$",
                json_string);
        }
        catch (const std::exception &ex)
        {
            LOG_F(ERROR, "redis fail {}", ex.what());
        }
    }

public:
    void reload_data()
    {

        try
        {
            std::vector<std::string> keys;
            auto redis = Redis(this->_redis_url);
            redis.keys(
                "summary:*",
                std::back_inserter(keys));
            summary_data_t sdt;
            for (auto &key : keys)
            {
                auto json_str = redis.command<std::string>(
                    "JSON.GET",
                    key,
                    "$");

                std::stringstream sstream;
                LOG_F(INFO, "redis data : {}", json_str);
                sstream << json_str;
                json j;
                sstream >> j;

                sdt.stock_id = j[0]["stock_id"].get<std::string>();
                sdt.area_center = j[0]["area_center"].get<std::string>();

                auto i = area_branch_map.find(sdt.area_center);
                if (i == area_branch_map.end())
                {
                    continue;
                }

                sdt.margin_available_amount = j[0].value("margin_available_amount", 0ll);
                sdt.margin_available_qty = j[0].value("margin_available_qty", 0ll);
                sdt.short_available_amount = j[0].value("short_available_amount", 0ll);
                sdt.short_available_qty = j[0].value("short_available_qty", 0ll);
                sdt.after_margin_available_amount = j[0].value("after_margin_available_amount", 0ll);
                sdt.after_margin_available_qty = j[0].value("after_margin_available_qty", 0ll);
                sdt.after_short_available_amount = j[0].value("after_short_available_amount", 0ll);
                sdt.after_short_available_qty = j[0].value("after_short_available_qty", 0ll);
                sdt.belong_branches = j[0]["belong_branches"].get<std::vector<std::string>>();
                summary_datas.emplace(key, sdt);
            }
        }
        catch (const std::exception &ex)
        {
            LOG_F(ERROR, "redis fail {}", ex.what());
        }
    }
    void handle(hcrtm01_no_null_data_t &hcrtm01)
    {
        auto margin_amount = BackOfficeInt(STRINGIFY(hcrtm01.margin_amount));
        auto margin_buy_order_amount = BackOfficeInt(STRINGIFY(hcrtm01.margin_buy_order_amount));
        auto margin_sell_match_amount = BackOfficeInt(STRINGIFY(hcrtm01.margin_sell_match_amount));
        auto margin_qty = BackOfficeInt(STRINGIFY(hcrtm01.margin_qty));
        auto margin_buy_order_qty = BackOfficeInt(STRINGIFY(hcrtm01.margin_buy_order_qty));
        auto margin_sell_match_qty = BackOfficeInt(STRINGIFY(hcrtm01.margin_sell_match_qty));
        auto short_amount = BackOfficeInt(STRINGIFY(hcrtm01.short_amount));
        auto short_sell_order_amount = BackOfficeInt(STRINGIFY(hcrtm01.short_sell_order_amount));
        auto short_qty = BackOfficeInt(STRINGIFY(hcrtm01.short_qty));
        auto short_sell_order_qty = BackOfficeInt(STRINGIFY(hcrtm01.short_sell_order_qty));
        auto short_after_hour_sell_order_amount = BackOfficeInt(STRINGIFY(hcrtm01.short_after_hour_sell_order_amount));
        auto short_after_hour_sell_order_qty = BackOfficeInt(STRINGIFY(hcrtm01.short_after_hour_sell_order_qty));
        auto short_sell_match_amount = BackOfficeInt(STRINGIFY(hcrtm01.short_sell_match_amount));
        auto short_sell_match_qty = BackOfficeInt(STRINGIFY(hcrtm01.short_sell_match_qty));
        auto margin_after_hour_buy_order_amount = BackOfficeInt(STRINGIFY(hcrtm01.margin_after_hour_buy_order_amount));
        auto margin_after_hour_buy_order_qty = BackOfficeInt(STRINGIFY(hcrtm01.margin_after_hour_buy_order_qty));
        auto margin_buy_match_amount = BackOfficeInt(STRINGIFY(hcrtm01.margin_buy_match_amount));
        auto margin_buy_match_qty = BackOfficeInt(STRINGIFY(hcrtm01.margin_buy_match_qty));
        auto stock_id = STRINGIFY(hcrtm01.stock_id);
        auto area_center = STRINGIFY(hcrtm01.area_center);
        trim_right(stock_id);
        trim_right(area_center);

        Poco::DateTime now_time;
        now_time.makeLocal(Poco::Timezone::tzd());
        Poco::DateTime TRADE_BEGIN_TIME(2022, 10, 10, 8);
        Poco::DateTime TRADE_END_TIME(2022, 10, 10, 15);

        auto after_margin_available_amount = margin_amount - margin_buy_match_amount + margin_sell_match_amount - margin_after_hour_buy_order_amount;
        auto after_margin_available_qty = margin_qty - margin_buy_match_qty + margin_sell_match_qty - margin_after_hour_buy_order_qty;
        auto after_short_available_amount = short_amount - short_sell_match_amount - short_after_hour_sell_order_amount;
        auto after_short_available_qty = short_qty - short_sell_order_qty - short_after_hour_sell_order_qty;

        LOG_F(INFO,
              "margin_qty={}, margin_buy_match_qty={}, margin_sell_match_qty={}, margin_after_hour_buy_order_qty={}, "
              ", short_qty={}, short_sell_match_qty={}, short_after_hour_sell_order_qty={}, short_sell_order_qty={},",
              margin_qty, margin_buy_match_qty, margin_sell_match_qty, margin_after_hour_buy_order_qty, short_qty, short_sell_match_qty, short_after_hour_sell_order_qty, short_sell_order_qty);
        LOG_F(INFO, "margin_buy_order_qty={}", margin_buy_order_qty);

        LOG_F(INFO, "now time:{} TRADE_BEGIN_TIME:{} TRADE_END_TIME:{}", now_time.hour(), TRADE_BEGIN_TIME.hour(), TRADE_END_TIME.hour());
        auto margin_available_amount = margin_amount - margin_buy_order_amount + margin_sell_match_amount;
        auto margin_available_qty = margin_qty - margin_buy_order_qty + margin_sell_match_qty;
        auto short_available_amount = short_amount - short_sell_order_amount;
        auto short_available_qty = short_qty - short_sell_order_qty;

        auto key = GetKeyBy(hcrtm01);
        struct summary_data_t summary_data;

        {
            std::unique_lock<std::mutex> lock1(map_lock);
            auto i = summary_datas.find(key);
            if (i == summary_datas.end())
            {
                summary_datas.emplace(key, summary_data);
            }
            i = summary_datas.find(key);

            auto &element_reference = i->second;
            element_reference.stock_id = stock_id;
            element_reference.area_center = area_center;
            element_reference.margin_available_amount = margin_available_amount;
            // 加上 資買互抵 temp 值
            int64_t buy_offset = element_reference.margin_buy_offset_qty;
            int64_t sell_offset = element_reference.short_sell_offset_qty;

            element_reference.margin_available_qty = margin_available_qty + buy_offset;
            element_reference.after_margin_available_qty = after_margin_available_qty + buy_offset;
            element_reference.short_available_qty = short_available_qty + sell_offset;
            element_reference.after_short_available_qty = after_short_available_qty + sell_offset;

            element_reference.short_available_amount = short_available_amount;
            element_reference.after_margin_available_amount = after_margin_available_amount;
            element_reference.after_short_available_amount = after_short_available_amount;

            FillBelongBranches(element_reference.belong_branches, area_center);
            dump_summary_data(element_reference);
            sync_to_redis(key, element_reference);

            update_company_summary(stock_id);
        }
    }

    void handle(hcrtm05p_no_null_data_t &hcrtm05p)
    {
        auto key = GetKeyBy(hcrtm05p);

        struct summary_data_t summary_data;
        auto margin_buy_offset_qty = BackOfficeInt(STRINGIFY(hcrtm05p.margin_buy_offset_qty));
        auto short_sell_offset_qty = BackOfficeInt(STRINGIFY(hcrtm05p.short_sell_offset_qty));
        auto stock_id = STRINGIFY(hcrtm05p.stock_id);
        auto branch_id = STRINGIFY(hcrtm05p.broker_id);
        trim_right(stock_id);
        trim_right(branch_id);
        LOG_F(INFO, "margin_buy_offset_qty={}, short_sell_offset_qty={}", margin_buy_offset_qty, short_sell_offset_qty);

        auto area_center = branch_id;
        {
            std::unique_lock<std::mutex> lock1(map_lock);
            auto i = summary_datas.find(key);
            if (i == summary_datas.end())
            {
                summary_datas.emplace(key, summary_data);
            }
            i = summary_datas.find(key);

            auto &element_reference = i->second;
            element_reference.stock_id = stock_id;
            element_reference.area_center = area_center;
            element_reference.margin_available_qty += margin_buy_offset_qty;
            element_reference.short_available_qty += short_sell_offset_qty;
            element_reference.after_margin_available_qty += margin_buy_offset_qty;
            element_reference.after_short_available_qty += short_sell_offset_qty;
            // 暫存 資買互抵
            element_reference.margin_buy_offset_qty = margin_buy_offset_qty;
            element_reference.short_sell_offset_qty = short_sell_offset_qty;
            dump_summary_data(element_reference);  // LOG
            sync_to_redis(key, element_reference); // only redis set
        }
    }
    void set_redis_url(const std::string &url, bool init_idx = false)
    {
        LOG_F(INFO, "start create inputIdx ");
        this->_redis_url = url;
        if (false == init_idx)
        {
            return;
        }
        Redis redis(this->_redis_url);
        try
        {
            redis.command<void>(
                "FT.CREATE",
                "outputIdx",
                "ON",
                "JSON",
                "PREFIX",
                "1",
                "summary:",
                "SCHEMA",
                "$.stock_id",
                "AS",
                "stock_id",
                "TEXT",
                "$.area_center",
                "AS",
                "area_center",
                "TEXT",
                "$.belong_branches.*",
                "AS",
                "branches",
                "TAG");
        }
        catch (const std::exception &ex)
        {

            redis.command<void>(
                "FT.DROP",
                "outputIdx");
            redis.command<void>(
                "FT.CREATE",
                "outputIdx",
                "ON",
                "JSON",
                "PREFIX",
                "1",
                "summary:",
                "SCHEMA",
                "$.stock_id",
                "AS",
                "stock_id",
                "TEXT",
                "$.area_center",
                "AS",
                "area_center",
                "TEXT",
                "$.belong_branches.*",
                "AS",
                "branches",
                "TAG");
            LOG_F(ERROR, "create index {}", ex.what());
        }
        LOG_F(INFO, "create inputIdx for Redisearch");
    }
};

static DataHandler data_handler;
void consumer()
{
    while (true)
    {
        char *data_ptr = nullptr;
        if (false == TryDequeue(data_ptr))
        {
            continue;
        }
        if (nullptr == data_ptr)
        {
            LOG_F(ERROR, "dequeue fail!");
            continue;
        }
        auto fb = (finance_bill_no_null_t *)data_ptr;
        auto &ap = fb->ap_no_null;
        auto packet_format = STRINGIFY(fb->tcode);
        LOG_F(INFO, "packet format tcode:{} enttype:{}", packet_format, ap.enttype[0]);
        if (ap.enttype[0] != 'A' && ap.enttype[0] != 'C')
        {
            continue;
        }
        if (packet_format == "ELD001")
        {
            auto &hcrtm01 = ap.ap_data.hcrtm01;
            auto header_area_center = STRINGIFY(ap.system);
            trim_right(header_area_center);
            auto data_area_center = STRINGIFY(hcrtm01.area_center);
            trim_right(data_area_center);
            if (header_area_center == data_area_center)
            {
                data_handler.handle(hcrtm01); // synchronous method call
            }
        }
        if (packet_format == "ELD002")
        {
            auto &hcrtm05p = ap.ap_data.hcrtm05p;
            data_handler.handle(hcrtm05p); // synchronous method call
        }
        delete[] data_ptr;
        data_ptr = nullptr;
    }
}

constexpr size_t least_parse_size = sizeof(ap_data_no_null_t);

void packet_dispatcher()
{
    int searched_index = 0;
    while (true)
    {
        std::unique_lock<std::mutex> lock1(packet_lock);
        if (!buffered.empty())
        {
            auto i = std::find(buffered.begin() + searched_index, buffered.end(), '\n');
            if (i == buffered.end())
            {
                continue;
            }

            auto len = (size_t)(i - buffered.begin());
            if (len == 2)
            {
                // content too large, drop packet
                LOG_F(INFO, "keep alive");
                buffered.resize(0);
                searched_index = 0;
                continue;
            }

            char *data_ptr = CHECK_MARX_NEW char[len + 1]{0};
            // use '\0' as string terminator
            std::copy(buffered.begin(), i, data_ptr);
            LOG_F(INFO, " data ={}", std::string(data_ptr));

            // i is '\n' position , + 1 for discord '\n'
            std::copy(i + 1, buffered.end(), buffered.begin());
            buffered.resize(buffered.size() - len - 1);
            // buffered.swap(remain_data);
            searched_index = 0;
            Enqueue(data_ptr);

            data_ptr = nullptr;
        }
    }
}

void InitOfficeIDs()
{
    std::ifstream ifs("area_branch.json");

    if (!ifs)
    {
        LOG_F(ERROR, "file open error");
        return;
    }
    ifs >> area_branch_map;
    for (auto &[key, value] : area_branch_map.items())
    {
        std::cout << "key:" << key << "value:" << value << std::endl;
        backoffice_ids.insert(key);
        for (auto &branch : value)
        {
            auto branch_id = branch.get<std::string>();
            following_broker_ids.emplace(branch_id, key);
            branches.emplace_back(branch_id);
        }
    }
}

void InitConnectionFromJson(std::string &redis_url, int &port, bool init_idx)
{
    std::ifstream ifs("connection.json");
    redis_url = "tcp://127.0.0.1:6479";
    if (!ifs)
    {
        LOG_F(ERROR, "connection json open error");
        return;
    }
    json j;
    try
    {
        ifs >> j;
        redis_url = j.at("redis_url").get<std::string>();
        port = j.at("server_port").get<int>();
    }
    catch (const std::exception &ex)
    {
        LOG_F(ERROR, "connection.json unformed.");
    }
    data_handler.set_redis_url(redis_url, init_idx);
}

void ReloadDataFromRedis()
{

    data_handler.reload_data();
}

int main(int argc, char **argv)
{
    try
    {
        buffered.resize(0);
        buffered.setCapacity(10000000);
        InitOfficeIDs();
        std::string redis_url;
        int port = 9516;
        InitConnectionFromJson(redis_url, port, (argc > 1));
        ReloadDataFromRedis();
        Poco::Net::ServerSocket serverSocket(port);
        auto param = CHECK_MARX_NEW Poco::Net::TCPServerParams;
        param->setMaxThreads(1);
        Poco::Net::TCPServer server(CHECK_MARX_NEW FinanceBillQuotaServer, serverSocket);
        auto packet_dispatcher_thd = std::async(packet_dispatcher);
        auto consumer_thd = std::async(consumer);
        server.start();
        while (true)
        {
            sleep(10);
        }
        server.stop();
    }
    catch (const std::exception &ex)
    {
        LOG_F(ERROR, "ex:{}", ex.what());
    }
}