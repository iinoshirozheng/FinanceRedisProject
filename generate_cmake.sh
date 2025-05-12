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
set(CMAKE_CXX_EXTENSIONS OFF)

# macOS 上使用 libc++ 是關鍵
set(CMAKE_CXX_FLAGS "\${CMAKE_CXX_FLAGS} -fPIC -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "\${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
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

# 建立模組化目錄結構
set(CMAKE_MODULE_PATH "\${CMAKE_SOURCE_DIR}/cmake" \${CMAKE_MODULE_PATH})
add_subdirectory(cmake)
EOL
}

# Generate CMake module structure for better organization
generate_cmake_modules() {
    # Ensure cmake directory exists
    mkdir -p "${SCRIPT_DIR}/cmake"
    
    # Create cmake/CMakeLists.txt
    cat > "${SCRIPT_DIR}/cmake/CMakeLists.txt" << EOL
# cmake/CMakeLists.txt
message(STATUS "載入 CMake 自定義模組...")

# 將目前資料夾加入 module path
list(APPEND CMAKE_MODULE_PATH "\${CMAKE_CURRENT_SOURCE_DIR}")

# 引入各功能模組
include(GlobalOptions)
include(BuildMainExecutable)
include(ConfigureTests)

# 執行
DefineGlobalOptions()
BuildMainExecutable()
ConfigureTests()
EOL

    # Create GlobalOptions.cmake
    cat > "${SCRIPT_DIR}/cmake/GlobalOptions.cmake" << EOL
function(DefineGlobalOptions)
    # Don't redefine standard - it's now set at the root CMakeLists.txt
    # We'll just make sure we're using C++17 here
    if(CMAKE_CXX_STANDARD LESS 17)
        message(WARNING "C++17 or higher is required for this project. Setting CMAKE_CXX_STANDARD to 17.")
        set(CMAKE_CXX_STANDARD 17 PARENT_SCOPE)
        set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
        set(CMAKE_CXX_EXTENSIONS OFF PARENT_SCOPE)
    endif()

    set(CMAKE_CXX_FLAGS "\${CMAKE_CXX_FLAGS} -fPIC")

    set(THIRD_PARTY_DIR \${CMAKE_SOURCE_DIR}/third_party CACHE STRING "Path to third-party libraries")
    include(\${THIRD_PARTY_DIR}/LinkThirdparty.cmake OPTIONAL)

    option(LINK_HIREDIS "Enable hiredis static link" ON)
    option(LINK_POCO "Enable Poco static link" ON)
    option(LINK_LOGURU "Enable Loguru logger" ON)
    option(LINK_NLOHMANN_JSON "Enable nlohmann/json" ON)
    option(LINK_REDIS_PLUS_PLUS "Enable redis-plus-plus static link" ON)
    option(LINK_SPDLOG "Enable spdlog logger" ON)
    option(LINK_GTEST "Enable Google Test framework" ON)
    option(BUILD_TESTS "Build unit tests" ON)

    message(STATUS "C++ 標準: \${CMAKE_CXX_STANDARD}")
    message(STATUS "第三方庫目錄: \${THIRD_PARTY_DIR}")
endfunction()
EOL

    # Create BuildMainExecutable.cmake
    cat > "${SCRIPT_DIR}/cmake/BuildMainExecutable.cmake" << EOL
function(BuildMainExecutable)
    # Find all source files
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS \${CMAKE_SOURCE_DIR}/src/*.cpp)
    message(STATUS "找到的 C++ 源文件:")
    foreach(file IN LISTS SOURCES)
        message(STATUS " \${file}")
    endforeach()
    
    # Build executable with project name
    add_executable(\${PROJECT_NAME} \${SOURCES})
    target_include_directories(\${PROJECT_NAME} PRIVATE \${CMAKE_SOURCE_DIR}/src)
    
    LinkThirdparty(\${PROJECT_NAME})
    message(STATUS "已建立可執行目標: \${PROJECT_NAME}")
endfunction()
EOL

    # Create ConfigureTests.cmake
    cat > "${SCRIPT_DIR}/cmake/ConfigureTests.cmake" << EOL
function(ConfigureTests)
    if(NOT BUILD_TESTS OR NOT LINK_GTEST)
        return()
    endif()

    enable_testing()
    # Get test sources but exclude compiler ID files that also have main()
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS \${CMAKE_SOURCE_DIR}/tests/*.cpp)
    list(FILTER TEST_SOURCES EXCLUDE REGEX ".*CompilerId.*")

    if(TEST_SOURCES)
        message(STATUS "找到的測試源文件:")
        foreach(file IN LISTS TEST_SOURCES)
            message(STATUS " \${file}")
        endforeach()

        # Use project name for test executable
        set(TEST_TARGET \${PROJECT_NAME}_tests)
        add_executable(\${TEST_TARGET} \${TEST_SOURCES})

        file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS \${CMAKE_SOURCE_DIR}/src/*.cpp)
        list(FILTER LIB_SOURCES EXCLUDE REGEX ".*main\\.cpp$")
        target_sources(\${TEST_TARGET} PRIVATE \${LIB_SOURCES})

        target_include_directories(\${TEST_TARGET} PRIVATE
            \${CMAKE_SOURCE_DIR}/src
            \${CMAKE_SOURCE_DIR}/tests
        )

        # Make sure tests get the same C++17 settings
        set_target_properties(\${TEST_TARGET} PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
        )

        LinkThirdparty(\${TEST_TARGET})

        # Find GTest and add the test discover step
        include(GoogleTest)
        gtest_discover_tests(\${TEST_TARGET})

        message(STATUS "已建立測試目標: \${TEST_TARGET}")
    else()
        message(STATUS "未找到測試源文件，測試目標未建立")
    endif()
endfunction()
EOL
}

# Generate CMakeLists.txt
generate_cmake() {
    echo "Generating CMakeLists.txt and modular CMake structure..."
    
    # Check and remove existing CMakeLists.txt
    if [ -f "${SCRIPT_DIR}/CMakeLists.txt" ]; then
        echo "發現已存在的 CMakeLists.txt，正在刪除..."
        rm "${SCRIPT_DIR}/CMakeLists.txt"
    fi

    # Generate CMakeLists.txt in parts
    generate_cmake_header
    generate_cmake_options
    generate_cmake_sources
    
    # Generate modular CMake structure
    generate_cmake_modules

    echo "已生成 CMakeLists.txt 和模組化 CMake 結構，專案名稱: $(basename "${SCRIPT_DIR}")"
    echo "第三方庫目錄: ${SCRIPT_DIR}/third_party"
}

# Execute main function
generate_cmake 