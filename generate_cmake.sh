#!/bin/zsh

# Exit on error
set -e


# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Set third party directory
THIRD_PARTY_DIR="/Users/ray/Desktop/CACB/third_party"

# Generate CMakeLists.txt header
generate_cmake_header() {
    cat > "${SCRIPT_DIR}/CMakeLists.txt" << EOL
cmake_minimum_required(VERSION 3.15)
project($(basename "${SCRIPT_DIR}") VERSION 1.0.0)

message(STATUS "CMake 版本: \${CMAKE_VERSION}")
message(STATUS "專案名稱: \${PROJECT_NAME}")

# C++17 設定
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "\${CMAKE_CXX_FLAGS} -fPIC")
message(STATUS "C++ 標準: \${CMAKE_CXX_STANDARD}")

# 第三方庫位置
set(THIRD_PARTY_DIR ${THIRD_PARTY_DIR})
message(STATUS "第三方庫目錄: \${THIRD_PARTY_DIR}")

# 引入外部函數
include(\${THIRD_PARTY_DIR}/LinkThirdparty.cmake OPTIONAL)
message(STATUS "已引入 LinkThirdparty.cmake")
EOL
}

# Generate CMakeLists.txt options
generate_cmake_options() {
    cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL

# 控制開關
EOL

    # Check if hiredis was cloned
    if [ -d "${SCRIPT_DIR}/third_party/hiredis" ]; then
        cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL
option(LINK_HIREDIS "啟用 hiredis 的靜態連結" ON)
EOL
    fi

    # Check if Poco was cloned
    if [ -d "${SCRIPT_DIR}/third_party/poco" ]; then
        cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL
option(LINK_POCO "啟用 Poco 的靜態連結" ON)
EOL
    fi

    # Check if loguru was cloned
    if [ -d "${SCRIPT_DIR}/third_party/loguru" ]; then
        cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL
option(LINK_LOGURU "啟用 Loguru 日誌記錄器" ON)
EOL
    fi

    # Check if nlohmann_json was cloned
    if [ -d "${SCRIPT_DIR}/third_party/nlohmann" ]; then
        cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL
option(LINK_NLOHMANN_JSON "啟用 nlohmann/json 支援" ON)
EOL
    fi

    # Check if redis-plus-plus was cloned
    if [ -d "${SCRIPT_DIR}/third_party/redis-plus-plus" ]; then
        cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL
option(LINK_REDIS_PLUS_PLUS "啟用 redis-plus-plus 的靜態連結" ON)
EOL
    fi

    # Check if spdlog was cloned
    if [ -d "${SCRIPT_DIR}/third_party/spdlog" ]; then
        cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL
option(LINK_SPDLOG "啟用 spdlog 日誌記錄器" ON)
EOL
    fi

    # Check if googletest was cloned
    if [ -d "${SCRIPT_DIR}/third_party/googletest" ]; then
        cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL
option(LINK_GTEST "啟用 Google Test 框架" ON)
option(BUILD_TESTS "建立單元測試" ON)
EOL
    fi

    # Add status messages for enabled options
    cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL

message(STATUS "靜態連結選項:")
EOL

    if [ -d "${SCRIPT_DIR}/third_party/hiredis" ]; then
        cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL
message(STATUS " LINK_HIREDIS: \${LINK_HIREDIS}")
EOL
    fi

    if [ -d "${SCRIPT_DIR}/third_party/poco" ]; then
        cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL
message(STATUS " LINK_POCO: \${LINK_POCO}")
EOL
    fi

    if [ -d "${SCRIPT_DIR}/third_party/loguru" ]; then
        cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL
message(STATUS " LINK_LOGURU: \${LINK_LOGURU}")
EOL
    fi

    if [ -d "${SCRIPT_DIR}/third_party/nlohmann" ]; then
        cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL
message(STATUS " LINK_NLOHMANN_JSON: \${LINK_NLOHMANN_JSON}")
EOL
    fi

    if [ -d "${SCRIPT_DIR}/third_party/redis-plus-plus" ]; then
        cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL
message(STATUS " LINK_REDIS_PLUS_PLUS: \${LINK_REDIS_PLUS_PLUS}")
EOL
    fi

    if [ -d "${SCRIPT_DIR}/third_party/spdlog" ]; then
        cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL
message(STATUS " LINK_SPDLOG: \${LINK_SPDLOG}")
EOL
    fi

    if [ -d "${SCRIPT_DIR}/third_party/googletest" ]; then
        cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL
message(STATUS " LINK_GTEST: \${LINK_GTEST}")
message(STATUS " BUILD_TESTS: \${BUILD_TESTS}")
EOL
    fi
}

# Generate CMakeLists.txt source files
generate_cmake_sources() {
    cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL

# 掃描源碼
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS \${CMAKE_SOURCE_DIR}/src/*.cpp)
message(STATUS "找到的 C++ 源文件:")
foreach(SOURCE_FILE IN LISTS SOURCES)
    message(STATUS " \${SOURCE_FILE}")
endforeach()
EOL
}

# Generate CMakeLists.txt target
generate_cmake_target() {
    cat >> "${SCRIPT_DIR}/CMakeLists.txt" << EOL

# 建立 target
add_executable($(basename "${SCRIPT_DIR}") \${SOURCES})
message(STATUS "已建立可執行目標: $(basename "${SCRIPT_DIR}")")

# 加入 include 路徑（精簡）
target_include_directories($(basename "${SCRIPT_DIR}") PRIVATE \${CMAKE_SOURCE_DIR}/src)

# 第三方靜態連結
LinkThirdparty($(basename "${SCRIPT_DIR}"))
message(STATUS "已通過 LinkThirdparty 函數連結第三方庫。")

# 設置單元測試
if(BUILD_TESTS AND LINK_GTEST)
    message(STATUS "啟用單元測試...")
    enable_testing()
    
    # 掃描測試源文件
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS \${CMAKE_SOURCE_DIR}/tests/*.cpp)
    
    if(TEST_SOURCES)
        message(STATUS "找到的測試源文件:")
        foreach(TEST_FILE IN LISTS TEST_SOURCES)
            message(STATUS " \${TEST_FILE}")
        endforeach()
        
        # 添加測試可執行文件
        add_executable(run_tests \${TEST_SOURCES})
        
        # 添加源文件（排除main.cpp）
        file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS \${CMAKE_SOURCE_DIR}/src/*.cpp)
        list(FILTER LIB_SOURCES EXCLUDE REGEX ".*main\\.cpp$")
        
        if(LIB_SOURCES)
            target_sources(run_tests PRIVATE \${LIB_SOURCES})
        endif()
        
        # 包含頭文件路徑
        target_include_directories(run_tests PRIVATE
            \${CMAKE_SOURCE_DIR}/src
            \${CMAKE_SOURCE_DIR}/tests
        )
        
        # 連結第三方庫
        LinkThirdparty(run_tests)
        
        # 添加測試
        add_test(NAME AllTests COMMAND run_tests)
        message(STATUS "已建立測試目標: run_tests")
    else()
        message(STATUS "未找到測試源文件，測試目標未建立")
    endif()
endif()
EOL
}

# Generate CMakeLists.txt
generate_cmake() {
    echo "Generating CMakeLists.txt..."
    
    # Check and remove existing CMakeLists.txt
    if [ -f "${SCRIPT_DIR}/CMakeLists.txt" ]; then
        echo "發現已存在的 CMakeLists.txt，正在刪除..."
        rm "${SCRIPT_DIR}/CMakeLists.txt"
    fi

    # Generate CMakeLists.txt in parts
    generate_cmake_header
    generate_cmake_options
    generate_cmake_sources
    generate_cmake_target

    echo "已生成 CMakeLists.txt 模板，專案名稱: $(basename "${SCRIPT_DIR}")"
    echo "第三方庫目錄: ${SCRIPT_DIR}/third_party"
}

# Execute main function
generate_cmake 