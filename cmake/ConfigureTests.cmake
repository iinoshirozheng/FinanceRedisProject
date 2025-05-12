function(ConfigureTests)
    if(NOT BUILD_TESTS OR NOT LINK_GTEST)
        return()
    endif()

    enable_testing()
    # Get test sources but exclude compiler ID files that also have main()
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/*.cpp)
    list(FILTER TEST_SOURCES EXCLUDE REGEX ".*CompilerId.*")

    if(TEST_SOURCES)
        message(STATUS "找到的測試源文件:")
        foreach(file IN LISTS TEST_SOURCES)
            message(STATUS " ${file}")
        endforeach()

        # Use project name for test executable
        set(TEST_TARGET ${PROJECT_NAME}_tests)
        add_executable(${TEST_TARGET} ${TEST_SOURCES})

        file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
        list(FILTER LIB_SOURCES EXCLUDE REGEX ".*main\.cpp$")
        target_sources(${TEST_TARGET} PRIVATE ${LIB_SOURCES})

        target_include_directories(${TEST_TARGET} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/tests
        )

        # Make sure tests get the same C++17 settings
        set_target_properties(${TEST_TARGET} PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
        )

        LinkThirdparty(${TEST_TARGET})

        # Find GTest and add the test discover step
        include(GoogleTest)
        gtest_discover_tests(${TEST_TARGET})

        message(STATUS "已建立測試目標: ${TEST_TARGET}")
    else()
        message(STATUS "未找到測試源文件，測試目標未建立")
    endif()
endfunction()
