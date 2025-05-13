function(DefineGlobalOptions)
    # Don't redefine standard - it's now set at the root CMakeLists.txt
    # We'll just make sure we're using C++17 here
    if(CMAKE_CXX_STANDARD LESS 17)
        message(WARNING "C++17 or higher is required for this project. Setting CMAKE_CXX_STANDARD to 17.")
        set(CMAKE_CXX_STANDARD 17 PARENT_SCOPE)
        set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
        set(CMAKE_CXX_EXTENSIONS OFF PARENT_SCOPE)
    endif()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

    set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party CACHE STRING "Path to third-party libraries")
    include(${THIRD_PARTY_DIR}/LinkThirdparty.cmake OPTIONAL)

    option(LINK_HIREDIS "Enable hiredis static link" ON)
    option(LINK_POCO "Enable Poco static link" ON)
    option(LINK_LOGURU "Enable Loguru logger" ON)
    option(LINK_NLOHMANN_JSON "Enable nlohmann/json" ON)
    option(LINK_REDIS_PLUS_PLUS "Enable redis-plus-plus static link" ON)
    option(LINK_SPDLOG "Enable spdlog logger" OFF)
    option(LINK_GTEST "Enable Google Test framework" ON)
    option(BUILD_TESTS "Build unit tests" ON)

    message(STATUS "靜態連結選項:")
    message(STATUS " LINK_HIREDIS: ${LINK_HIREDIS}")
    message(STATUS " LINK_POCO: ${LINK_POCO}")
    message(STATUS " LINK_LOGURU: ${LINK_LOGURU}")
    message(STATUS " LINK_NLOHMANN_JSON: ${LINK_NLOHMANN_JSON}")
    message(STATUS " LINK_REDIS_PLUS_PLUS: ${LINK_REDIS_PLUS_PLUS}")
    message(STATUS " LINK_SPDLOG: ${LINK_SPDLOG}")
    message(STATUS " LINK_GTEST: ${LINK_GTEST}")
    message(STATUS " BUILD_TESTS: ${BUILD_TESTS}")

    message(STATUS "C++ 標準: ${CMAKE_CXX_STANDARD}")
    message(STATUS "第三方庫目錄: ${THIRD_PARTY_DIR}")
endfunction()
