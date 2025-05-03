cmake_minimum_required(VERSION 3.15)

# Project definition with proper versioning and language specification
project(FinanceSystem 
    VERSION 1.0.0 
    DESCRIPTION "Financial Management System"
    LANGUAGES CXX
)

# Build configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(
        -Wall 
        -Wextra 
        -Wpedantic
        # Temporarily disable -Werror until we fix all warnings
        # -Werror
    )
    
    # Suppress C99 extension warnings for third-party code
    add_compile_options(
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-c99-extensions>
    )
endif()

# Required system packages
find_package(Threads REQUIRED)

# Set Poco paths
set(Poco_DIR ${CMAKE_SOURCE_DIR}/lib/poco/lib/cmake/Poco)
find_package(Poco REQUIRED COMPONENTS Net Foundation)
find_package(nlohmann_json REQUIRED)

# Third-party library paths
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/lib)

# Include directories for third-party libraries
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${THIRD_PARTY_DIR}/boost/include
    ${THIRD_PARTY_DIR}/hiredis/include
    ${THIRD_PARTY_DIR}/nlohmann_json/include
    ${THIRD_PARTY_DIR}/loguru
    ${THIRD_PARTY_DIR}/poco/include
)

# Library paths for linking
link_directories(
    ${THIRD_PARTY_DIR}/hiredis/lib
    ${THIRD_PARTY_DIR}/poco/lib
)

# Domain library (header-only)
add_library(finance_domain INTERFACE)
target_include_directories(finance_domain INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# Common library (header-only)
add_library(finance_common INTERFACE)
target_include_directories(finance_common INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# Infrastructure library
add_library(finance_infrastructure STATIC
    src/infrastructure/network/TcpServiceAdapter.cpp
    src/infrastructure/network/FinancePackageHandler.cpp
    src/infrastructure/storage/RedisSummaryAdapter.cpp
    src/infrastructure/logging/LoguruLogger.cpp
    src/infrastructure/config/AreaBranchProvider.cpp
)

target_include_directories(finance_infrastructure PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(finance_infrastructure
    PUBLIC
        finance_domain
        finance_common
    PRIVATE
        hiredis
        PocoFoundation
        PocoNet
        Threads::Threads
)

set_target_properties(finance_infrastructure PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# Application library
add_library(finance_application STATIC
    src/application/FinanceService.cpp
)

target_include_directories(finance_application PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(finance_application
    PUBLIC
        finance_domain
    PRIVATE
        finance_infrastructure
)

set_target_properties(finance_application PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# Add loguru if available
if(EXISTS ${CMAKE_SOURCE_DIR}/lib/loguru/loguru.cpp)
    target_sources(finance_infrastructure PRIVATE
        ${CMAKE_SOURCE_DIR}/lib/loguru/loguru.cpp
    )
    target_include_directories(finance_infrastructure PRIVATE
        ${CMAKE_SOURCE_DIR}/lib/loguru
    )
    target_link_libraries(finance_infrastructure PRIVATE dl)
endif()

# Main executable
add_executable(finance_manager
    src/entry/main.cpp
)

target_link_libraries(finance_manager
    PRIVATE
        finance_application
        finance_infrastructure
        finance_domain
        Poco::Net
        Poco::Foundation
        hiredis
        nlohmann_json::nlohmann_json
)

set_target_properties(finance_manager PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Installation configuration
include(GNUInstallDirs)
install(TARGETS 
    finance_domain
    finance_common
    finance_infrastructure
    finance_application
    finance_manager
    EXPORT ${PROJECT_NAME}Targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Testing configuration
if(EXISTS "${CMAKE_SOURCE_DIR}/tests")
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

# Generate package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
) 