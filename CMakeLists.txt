cmake_minimum_required(VERSION 3.15)
project(CACB VERSION 1.0.0)

message(STATUS "CMake 版本: ${CMAKE_VERSION}")
message(STATUS "專案名稱: ${PROJECT_NAME}")

# C++17 設定
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
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

message(STATUS "靜態連結選項:")
message(STATUS " LINK_HIREDIS: ${LINK_HIREDIS}")
message(STATUS " LINK_POCO: ${LINK_POCO}")
message(STATUS " LINK_LOGURU: ${LINK_LOGURU}")
message(STATUS " LINK_NLOHMANN_JSON: ${LINK_NLOHMANN_JSON}")

# 掃描源碼
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
message(STATUS "找到的 C++ 源文件:")
foreach(SOURCE_FILE IN LISTS SOURCES)
    message(STATUS " ${SOURCE_FILE}")
endforeach()

# 建立 target
add_executable(CACB ${SOURCES})
message(STATUS "已建立可執行目標: CACB")

# 加入 include 路徑（精簡）
target_include_directories(CACB PRIVATE ${CMAKE_SOURCE_DIR}/src)

# 第三方靜態連結
LinkThirdparty(CACB)
message(STATUS "已通過 LinkThirdparty 函數連結第三方庫。")
