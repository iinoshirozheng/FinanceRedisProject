function(ConfigureTests)
    if(NOT BUILD_TESTS OR NOT LINK_GTEST)
        return()
    endif()

    enable_testing()
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/*.cpp)
    
    if(TEST_SOURCES)
        message(STATUS "找到的測試源文件:")
        foreach(file IN LISTS TEST_SOURCES)
            message(STATUS " ${file}")
        endforeach()
    
    
        # 加入 src/ 中的實作（排除 main.cpp）
        file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
        list(FILTER LIB_SOURCES EXCLUDE REGEX ".*main\\.cpp$")
        add_executable(run_tests ${TEST_SOURCES})
        target_sources(run_tests PRIVATE ${LIB_SOURCES})
    
        # 加入必要的 include path
        target_include_directories(run_tests PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/tests
        )
    
        # 第三方靜態連結
        LinkThirdparty(run_tests)
    
        # 自動註冊 Google Test 測試案例
        include(GoogleTest)
        gtest_discover_tests(run_tests)
    
        add_test(NAME run_tests COMMAND run_tests)
        message(STATUS "已建立測試目標: run_tests")
    else()
        message(STATUS "未找到測試源文件，測試目標未建立")
    endif()
    
endfunction()
