cmake_minimum_required(VERSION 3.14)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Domain library (header-only)
add_library(finance_domain INTERFACE)
target_include_directories(finance_domain INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Common library (header-only)
add_library(finance_common INTERFACE)
target_include_directories(finance_common INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Infrastructure library
add_library(finance_infrastructure STATIC
    infrastructure/storage/RedisSummaryAdapter.cpp
    infrastructure/storage/RedisSummaryAdapter.h
    infrastructure/network/TcpServiceAdapter.cpp
    infrastructure/network/TcpServiceAdapter.h
    infrastructure/network/PacketQueue.cpp
    infrastructure/network/PacketQueue.h
    infrastructure/network/FinancePackageHandler.cpp
    infrastructure/network/FinancePackageHandler.h
)

target_include_directories(finance_infrastructure PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_sources(finance_infrastructure PRIVATE
    infrastructure/config/ConnectionConfigProvider.hpp
    infrastructure/config/AreaBranchProvider.hpp
    infrastructure/config/JsonProviderBase.hpp
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

# Set properties for infrastructure library
set_target_properties(finance_infrastructure PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# Application library
add_library(finance_application STATIC
    application/FinanceService.cpp
    application/FinanceService.h
)

target_include_directories(finance_application PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(finance_application
    PUBLIC
        finance_domain
    PRIVATE
        finance_infrastructure
)

# Set properties for application library
set_target_properties(finance_application PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# Main executable
add_executable(finance_manager
    entry/main.cpp
)

target_link_libraries(finance_manager
    PRIVATE
        finance_application
        finance_infrastructure
        finance_domain
)

# Set properties for executable
set_target_properties(finance_manager PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
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

# Installation rules
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