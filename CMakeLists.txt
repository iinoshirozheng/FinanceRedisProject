cmake_minimum_required(VERSION 3.14)

# Project definition with proper versioning and language specification
project(FinanceManager 
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

# Third-party library paths
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/lib)

# Include directories for third-party libraries
include_directories(
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

# Add main source directory
add_subdirectory(src)

# Installation configuration
include(GNUInstallDirs)
install(TARGETS finance_manager 
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
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