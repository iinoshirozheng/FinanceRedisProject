cmake_minimum_required(VERSION 3.14)
project(finance_tests)

# 啟用測試
enable_testing()

# 查找 GTest 包
find_package(GTest REQUIRED)

# 添加測試可執行文件
add_executable(ring_buffer_test
    network/RingBufferTest.cpp
)

# 設置包含目錄
target_include_directories(ring_buffer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${GTEST_INCLUDE_DIRS}
)

# 鏈接 GTest 庫
target_link_libraries(ring_buffer_test PRIVATE
    GTest::GTest
    GTest::Main
    finance_infrastructure
    finance_domain
    finance_common
    pthread
)

# 添加測試
add_test(NAME ring_buffer_test COMMAND ring_buffer_test)

# 設置輸出目錄
set_target_properties(ring_buffer_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
) 