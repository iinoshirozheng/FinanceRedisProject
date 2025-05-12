cmake_minimum_required(VERSION 3.15)
project(CACB VERSION 1.0.0)

message(STATUS "CMake 版本: ${CMAKE_VERSION}")
message(STATUS "專案名稱: ${PROJECT_NAME}")

# C++17 設定
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# macOS 上使用 libc++ 是關鍵
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
message(STATUS "C++ 標準: ${CMAKE_CXX_STANDARD}")

# 第三方庫位置
set(THIRD_PARTY_DIR /Users/ray/Desktop/CACB/third_party)
message(STATUS "第三方庫目錄: ${THIRD_PARTY_DIR}")

# 引入外部函數
include(${THIRD_PARTY_DIR}/LinkThirdparty.cmake OPTIONAL)
message(STATUS "已引入 LinkThirdparty.cmake")

# 控制開關
option(LINK_HIREDIS "啟用 hiredis 的靜態連結" ON)
option(LINK_POCO "啟用 Poco 的靜態連結" ON)
option(LINK_LOGURU "啟用 Loguru 日誌記錄器" ON)
option(LINK_NLOHMANN_JSON "啟用 nlohmann/json 支援" ON)
option(LINK_REDIS_PLUS_PLUS "啟用 redis-plus-plus 的靜態連結" ON)
option(LINK_SPDLOG "啟用 spdlog 日誌記錄器" ON)
option(LINK_GTEST "啟用 Google Test 框架" ON)
option(BUILD_TESTS "建立單元測試" ON)

message(STATUS "靜態連結選項:")
message(STATUS " LINK_HIREDIS: ${LINK_HIREDIS}")
message(STATUS " LINK_POCO: ${LINK_POCO}")
message(STATUS " LINK_LOGURU: ${LINK_LOGURU}")
message(STATUS " LINK_NLOHMANN_JSON: ${LINK_NLOHMANN_JSON}")
message(STATUS " LINK_REDIS_PLUS_PLUS: ${LINK_REDIS_PLUS_PLUS}")
message(STATUS " LINK_SPDLOG: ${LINK_SPDLOG}")
message(STATUS " LINK_GTEST: ${LINK_GTEST}")
message(STATUS " BUILD_TESTS: ${BUILD_TESTS}")

# 建立模組化目錄結構
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
add_subdirectory(cmake)
