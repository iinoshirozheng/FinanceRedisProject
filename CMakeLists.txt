cmake_minimum_required(VERSION 3.15)
project(finance_stock_quota VERSION 1.0.0)

message(STATUS "CMake 版本: ${CMAKE_VERSION}")
message(STATUS "專案名稱: ${PROJECT_NAME}")

# C++17 設定
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# 第三方庫位置
message(STATUS "第三方庫目錄: ${THIRD_PARTY_DIR}")

# 引入外部函數
include(${THIRD_PARTY_DIR}/LinkThirdparty.cmake OPTIONAL)
message(STATUS "已引入 ${THIRD_PARTY_DIR}/LinkThirdparty.cmake")

# 建立模組化目錄結構
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
add_subdirectory(cmake)
