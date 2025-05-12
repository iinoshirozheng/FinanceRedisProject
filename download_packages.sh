#!/bin/zsh

# Exit on error
set -e

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Create directories
Create_dir() {
    if [ -d "${SCRIPT_DIR}/third_party_" ]; then
        echo "Removing existing third_party_ directory..."
        rm -rf "${SCRIPT_DIR}/third_party_"
    fi
    if [ -d "${SCRIPT_DIR}/third_party" ]; then
        echo "Removing existing third_party directory..."
        rm -rf "${SCRIPT_DIR}/third_party"
    fi
    mkdir -p "${SCRIPT_DIR}/third_party_"
    mkdir -p "${SCRIPT_DIR}/third_party"
    cd "${SCRIPT_DIR}/third_party_"

    # Initialize CMake file
    cat > "${SCRIPT_DIR}/third_party/LinkThirdparty.cmake" << 'EOL'
function(LinkThirdparty target_name)
    message(STATUS "LinkThirdparty module invoked for target '${target_name}'")
    message(STATUS "Thirdparty Directory: ${THIRD_PARTY_DIR}")

    # 驗證 THIRD_PARTY_DIR 是否存在
    if(NOT EXISTS "${THIRD_PARTY_DIR}")
        message(FATAL_ERROR "Thirdparty directory '${THIRD_PARTY_DIR}' does not exist!")
    endif()
EOL
}

clean_build() {
    if [ -d "${SCRIPT_DIR}/third_party_" ]; then
        echo "Cleaning third_party_ directory..."
        rm -rf "${SCRIPT_DIR}/third_party_"
    fi

    # Close CMake function
    cat >> "${SCRIPT_DIR}/third_party/LinkThirdparty.cmake" << 'EOL'
endfunction()
EOL
}

# Update and clean repo
update_repo() {
    local repo_dir=$1
    if [ -d "$repo_dir" ]; then
        echo "Updating $repo_dir..."
        cd "$repo_dir"
        git pull
        if [ -d "build" ]; then
            echo "Cleaning build directory..."
            rm -rf build
        fi
        cd ..
    fi
}

# Clone and build hiredis
clone_hiredis() {
    if [ ! -d "hiredis" ]; then
        echo "Cloning hiredis..."
        git clone https://github.com/redis/hiredis.git
    else
        update_repo "hiredis"
    fi
    
    cd hiredis
    make -j$(sysctl -n hw.ncpu)
    make PREFIX="${SCRIPT_DIR}/third_party/hiredis" install
    cd ..

    # Add hiredis CMake configuration
    cat >> "${SCRIPT_DIR}/third_party/LinkThirdparty.cmake" << 'EOL'

    # === Hiredis ===
    if(LINK_HIREDIS)
        message(STATUS "Linking hiredis (static)...")
        target_include_directories(${target_name} PRIVATE ${THIRD_PARTY_DIR}/hiredis/include)
        
        set(lib_file "libhiredis.a")
        if(EXISTS "${THIRD_PARTY_DIR}/hiredis/lib/${lib_file}")
            target_link_libraries(${target_name} PRIVATE ${THIRD_PARTY_DIR}/hiredis/lib/${lib_file})
        elseif(EXISTS "${THIRD_PARTY_DIR}/hiredis/lib64/${lib_file}")
            target_link_libraries(${target_name} PRIVATE ${THIRD_PARTY_DIR}/hiredis/lib64/${lib_file})
        else()
            message(FATAL_ERROR "hiredis library not found in lib/ or lib64/ directories")
        endif()
    endif()
EOL
}

# Clone and build spdlog (header-only)
clone_spdlog() {
    if [ ! -d "spdlog" ]; then
        echo "Cloning spdlog..."
        git clone https://github.com/gabime/spdlog.git
    else
        update_repo "spdlog"
    fi

    cd spdlog
    mkdir build && cd build
    cmake -DCMAKE_INSTALL_PREFIX="${SCRIPT_DIR}/third_party/spdlog" \
          -DSPDLOG_BUILD_SHARED=OFF \
          -DSPDLOG_BUILD_EXAMPLE=OFF \
          -DSPDLOG_BUILD_TESTS=OFF \
          -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
          -DCMAKE_BUILD_TYPE=Release \
          ..
    make -j$(sysctl -n hw.ncpu)
    make install
    cd ../../
    rm -rf spdlog

    # Add spdlog CMake configuration
    cat >> "${SCRIPT_DIR}/third_party/LinkThirdparty.cmake" << 'EOL'

    # === spdlog ===
    if(LINK_SPDLOG)
        message(STATUS "Linking spdlog (static)...")
        target_include_directories(${target_name} PRIVATE ${THIRD_PARTY_DIR}/spdlog/include)
        
        set(lib_file "libspdlog.a")
        if(EXISTS "${THIRD_PARTY_DIR}/spdlog/lib/${lib_file}")
            target_link_libraries(${target_name} PRIVATE ${THIRD_PARTY_DIR}/spdlog/lib/${lib_file})
        elseif(EXISTS "${THIRD_PARTY_DIR}/spdlog/lib64/${lib_file}")
            target_link_libraries(${target_name} PRIVATE ${THIRD_PARTY_DIR}/spdlog/lib64/${lib_file})
        else()
            message(FATAL_ERROR "spdlog library not found in lib/ or lib64/ directories")
        endif()
    endif()
EOL
}

clone_gtest() {
    if [ ! -d "googletest" ]; then
        echo "Cloning googletest..."
        git clone https://github.com/google/googletest.git
    else
        update_repo "googletest"
    fi

    cd googletest
    mkdir -p build && cd build
    cmake -DCMAKE_INSTALL_PREFIX="${SCRIPT_DIR}/third_party/googletest" \
          -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
          -DBUILD_GMOCK=ON \
          -DBUILD_GTEST=ON \
          -DCMAKE_BUILD_TYPE=Release \
          ..
    make -j$(sysctl -n hw.ncpu)
    make install
    cd ../../
    rm -rf googletest

    # Add gtest CMake configuration
    cat >> "${SCRIPT_DIR}/third_party/LinkThirdparty.cmake" << 'EOL'

    # === GoogleTest ===
    if(LINK_GTEST)
        message(STATUS "Linking GoogleTest...")
        target_include_directories(${target_name} PRIVATE ${THIRD_PARTY_DIR}/googletest/include)
        
        set(gtest_lib "libgtest.a")
        set(gtest_main_lib "libgtest_main.a")
        
        # Check for gtest library
        if(EXISTS "${THIRD_PARTY_DIR}/googletest/lib/${gtest_lib}")
            set(gtest_lib_path "${THIRD_PARTY_DIR}/googletest/lib/${gtest_lib}")
        elseif(EXISTS "${THIRD_PARTY_DIR}/googletest/lib64/${gtest_lib}")
            set(gtest_lib_path "${THIRD_PARTY_DIR}/googletest/lib64/${gtest_lib}")
        else()
            message(FATAL_ERROR "Google Test library not found in lib/ or lib64/ directories")
        endif()
        
        # Check for gtest_main library
        if(EXISTS "${THIRD_PARTY_DIR}/googletest/lib/${gtest_main_lib}")
            set(gtest_main_lib_path "${THIRD_PARTY_DIR}/googletest/lib/${gtest_main_lib}")
        elseif(EXISTS "${THIRD_PARTY_DIR}/googletest/lib64/${gtest_main_lib}")
            set(gtest_main_lib_path "${THIRD_PARTY_DIR}/googletest/lib64/${gtest_main_lib}")
        else()
            message(FATAL_ERROR "Google Test Main library not found in lib/ or lib64/ directories")
        endif()
        
        target_link_libraries(${target_name} PRIVATE
            ${gtest_lib_path}
            ${gtest_main_lib_path}
            pthread)
    endif()
EOL
}

# Clone nlohmann json (header-only)
clone_nlohmann_json() {
    if [ ! -d "nlohmann_json" ]; then
        echo "Cloning nlohmann/json..."
        git clone https://github.com/nlohmann/json.git nlohmann_json
    else
        update_repo "nlohmann_json"
    fi
    
    mkdir -p "${SCRIPT_DIR}/third_party/nlohmann"
    cp -r nlohmann_json/single_include/nlohmann "${SCRIPT_DIR}/third_party/"

    # Add nlohmann/json CMake configuration
    cat >> "${SCRIPT_DIR}/third_party/LinkThirdparty.cmake" << 'EOL'

    # === nlohmann/json (Header-only) ===
    if(LINK_NLOHMANN_JSON)
        message(STATUS "Adding nlohmann/json support...")
        target_include_directories(${target_name} PRIVATE ${THIRD_PARTY_DIR})
    endif()
EOL
}

# Clone loguru (header-only)
clone_loguru() {
    if [ ! -d "loguru" ]; then
        echo "Cloning loguru..."
        git clone https://github.com/emilk/loguru.git
    else
        update_repo "loguru"
    fi
    
    mkdir -p "${SCRIPT_DIR}/third_party/loguru"
    cp -r loguru/loguru.hpp "${SCRIPT_DIR}/third_party/loguru"
    cp -r loguru/loguru.cpp "${SCRIPT_DIR}/third_party/loguru"

    # Add loguru CMake configuration
    cat >> "${SCRIPT_DIR}/third_party/LinkThirdparty.cmake" << 'EOL'

    # === Loguru (Header-only) ===
    if(LINK_LOGURU)
        message(STATUS "Using Loguru for logging...")
        target_sources(${target_name} PRIVATE ${THIRD_PARTY_DIR}/loguru/loguru.cpp)
        target_include_directories(${target_name} PRIVATE ${THIRD_PARTY_DIR}/loguru)
    endif()
EOL
}

# Clone and build Poco
clone_poco() {
    if [ ! -d "poco" ]; then
        echo "Cloning Poco..."
        git clone https://github.com/pocoproject/poco.git
    else
        update_repo "poco"
    fi
    
    cd poco
    mkdir cmake-build && cd cmake-build
    cmake -DCMAKE_INSTALL_PREFIX="${SCRIPT_DIR}/third_party/poco" \
          -DENABLE_TESTS=OFF \
          -DENABLE_SAMPLES=OFF \
          -DBUILD_SHARED_LIBS=OFF \
          -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
          ..
    make -j$(sysctl -n hw.ncpu)
    make install
    cd ../../..

    # Add Poco CMake configuration
    cat >> "${SCRIPT_DIR}/third_party/LinkThirdparty.cmake" << 'EOL'

    # === Poco ===
    if(LINK_POCO)
        message(STATUS "Linking Poco libraries (static)...")

        # 注意依賴順序由上往下
        set(PocoModules
            Net
            JSON
            Util
            XML
            Crypto
            Data
            Encodings
            Foundation
        )

        set(DETECTED_POCO_MODULES "")
        foreach(module ${PocoModules})
            set(lib_file "libPoco${module}.a")
            if(EXISTS "${THIRD_PARTY_DIR}/poco/lib/${lib_file}")
                list(APPEND DETECTED_POCO_MODULES ${module})
                set(POCO_${module}_LIB_PATH "${THIRD_PARTY_DIR}/poco/lib/${lib_file}")
            elseif(EXISTS "${THIRD_PARTY_DIR}/poco/lib64/${lib_file}")
                list(APPEND DETECTED_POCO_MODULES ${module})
                set(POCO_${module}_LIB_PATH "${THIRD_PARTY_DIR}/poco/lib64/${lib_file}")
            endif()
        endforeach()

        message(STATUS "Detected Poco Modules (ordered): ${DETECTED_POCO_MODULES}")
        target_include_directories(${target_name} PRIVATE ${THIRD_PARTY_DIR}/poco/include)

        foreach(module ${DETECTED_POCO_MODULES})
            target_link_libraries(${target_name} PRIVATE "${POCO_${module}_LIB_PATH}")
            message(STATUS "Linked Poco${module} (static)")
        endforeach()

        target_link_libraries(${target_name} PRIVATE pthread dl)
    endif()
EOL
}

# Clone and build redis-plus-plus
clone_redis_plus_plus() {
    if [ ! -d "redis-plus-plus" ]; then
        echo "Cloning redis-plus-plus..."
        git clone https://github.com/sewenew/redis-plus-plus.git
    else
        update_repo "redis-plus-plus"
    fi
    
    cd redis-plus-plus
    mkdir -p build && cd build
    cmake -DCMAKE_INSTALL_PREFIX="${SCRIPT_DIR}/third_party/redis-plus-plus" \
          -DCMAKE_PREFIX_PATH="${SCRIPT_DIR}/third_party/hiredis" \
          -DREDIS_PLUS_PLUS_CXX_STANDARD=17 \
          -DREDIS_PLUS_PLUS_BUILD_TEST=OFF \
          -DREDIS_PLUS_PLUS_BUILD_SHARED=OFF \
          -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
          -DCMAKE_BUILD_TYPE=Release \
          ..
    make -j$(sysctl -n hw.ncpu)
    make install
    cd ../../
    rm -rf redis-plus-plus

    # Add redis-plus-plus CMake configuration
    cat >> "${SCRIPT_DIR}/third_party/LinkThirdparty.cmake" << 'EOL'

    # === redis-plus-plus ===
    if(LINK_REDIS_PLUS_PLUS)

        message(STATUS "Linking hiredis (static)...")
        target_include_directories(${target_name} PRIVATE ${THIRD_PARTY_DIR}/hiredis/include)
        
        set(hiredis_lib "libhiredis.a")
        if(EXISTS "${THIRD_PARTY_DIR}/hiredis/lib/${hiredis_lib}")
            target_link_libraries(${target_name} PRIVATE ${THIRD_PARTY_DIR}/hiredis/lib/${hiredis_lib})
        elseif(EXISTS "${THIRD_PARTY_DIR}/hiredis/lib64/${hiredis_lib}")
            target_link_libraries(${target_name} PRIVATE ${THIRD_PARTY_DIR}/hiredis/lib64/${hiredis_lib})
        else()
            message(FATAL_ERROR "hiredis library not found in lib/ or lib64/ directories")
        endif()

        message(STATUS "Linking redis-plus-plus (static)...")
        # Make sure hiredis is linked first as it's a dependency
        if(NOT LINK_HIREDIS)
            message(FATAL_ERROR "redis-plus-plus requires hiredis, please enable LINK_HIREDIS")
        endif()
        
        target_include_directories(${target_name} PRIVATE ${THIRD_PARTY_DIR}/redis-plus-plus/include)
        
        set(redis_lib "libredis++.a")
        if(EXISTS "${THIRD_PARTY_DIR}/redis-plus-plus/lib/${redis_lib}")
            target_link_libraries(${target_name} PRIVATE ${THIRD_PARTY_DIR}/redis-plus-plus/lib/${redis_lib})
        elseif(EXISTS "${THIRD_PARTY_DIR}/redis-plus-plus/lib64/${redis_lib}")
            target_link_libraries(${target_name} PRIVATE ${THIRD_PARTY_DIR}/redis-plus-plus/lib64/${redis_lib})
        else()
            message(FATAL_ERROR "redis-plus-plus library not found in lib/ or lib64/ directories")
        endif()
    endif()
EOL
}

# Main function
main() {
    Create_dir

    # Clone and build all dependencies
    clone_hiredis
    clone_nlohmann_json
    clone_loguru
    clone_poco
    clone_redis_plus_plus
    clone_spdlog
    clone_gtest
    
    clean_build

    echo "All dependencies successfully built and installed to ${SCRIPT_DIR}/third_party/{package_name}!"
}

# Execute main function
main 