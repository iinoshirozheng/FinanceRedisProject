cmake_minimum_required(VERSION 3.10)

# Source files
set(DOMAIN_SOURCES
)

set(DOMAIN_HEADERS
    domain/FinanceDataStructure.h
    domain/IFinanceRepository.h
    domain/IPacketHandler.h
)

set(COMMON_HEADERS
    common/FinanceUtils.hpp
)

set(APPLICATION_SOURCES
    application/FinanceService.cpp
)

set(INFRASTRUCTURE_STORAGE_SOURCES
    infrastructure/storage/RedisSummaryAdapter.cpp
)

set(INFRASTRUCTURE_NETWORK_SOURCES
    infrastructure/network/TcpServiceAdapter.cpp
    infrastructure/network/PacketQueue.cpp
)

set(SOURCES
    ${DOMAIN_SOURCES}
    ${APPLICATION_SOURCES}
    ${INFRASTRUCTURE_STORAGE_SOURCES}
    ${INFRASTRUCTURE_NETWORK_SOURCES}
)

# Third-party libraries
set(THIRD_PARTY_SOURCES
)

# Check if loguru.cpp exists
if(EXISTS ${CMAKE_SOURCE_DIR}/lib/loguru/loguru.cpp)
    list(APPEND THIRD_PARTY_SOURCES ${CMAKE_SOURCE_DIR}/lib/loguru/loguru.cpp)
    message(STATUS "loguru.cpp found, including it")
else()
    message(STATUS "loguru.cpp not found, skipping")
endif()

# Finance library
add_library(finance_lib STATIC
    ${SOURCES}
    ${THIRD_PARTY_SOURCES}
)

target_include_directories(finance_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib/hiredis/include
    ${CMAKE_SOURCE_DIR}/lib/nlohmann_json/single_include
    ${CMAKE_SOURCE_DIR}/lib/loguru
    ${CMAKE_SOURCE_DIR}/lib/poco/Foundation/include
    ${CMAKE_SOURCE_DIR}/lib/poco/Net/include
)

target_link_libraries(finance_lib
    hiredis
    pthread
    dl
    PocoFoundation
    PocoNet
) 