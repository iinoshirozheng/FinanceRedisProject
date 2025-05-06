#pragma once
#include <loguru.hpp>
#include <string_view>

#define LOG_CTX(tcode, sid, area, level, fmt, ...) \
    LOG_F(level, "[t=%.*s,s=%s,a=%s] " fmt,        \
          int((tcode).size()), (tcode).data(),     \
          (sid).c_str(), (area).c_str(), ##__VA_ARGS__)