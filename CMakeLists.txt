cmake_minimum_required(VERSION 3.14)
project(FinanceManager VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set include paths for Boost
if(EXISTS "${CMAKE_SOURCE_DIR}/lib/boost/include")
    include_directories(${CMAKE_SOURCE_DIR}/lib/boost/include)
endif()

# Include directories for third-party libraries
include_directories(${CMAKE_SOURCE_DIR}/lib/hiredis/include)
include_directories(${CMAKE_SOURCE_DIR}/lib/nlohmann_json/include)
include_directories(${CMAKE_SOURCE_DIR}/lib/loguru)
include_directories(${CMAKE_SOURCE_DIR}/lib/poco/include)

# Library paths
link_directories(${CMAKE_SOURCE_DIR}/lib/hiredis/lib)
link_directories(${CMAKE_SOURCE_DIR}/lib/poco/lib)

# Add subdirectories for libraries
add_subdirectory(src)

# Create main executable
add_executable(finance_manager src/entry/main.cpp)

# Link against libraries
target_link_libraries(finance_manager 
    PRIVATE 
    finance_lib
    hiredis
    pthread
    dl
    PocoFoundation
    PocoNet
)

# Installation
install(TARGETS finance_manager DESTINATION bin)

# Testing configuration
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/main_test.cpp" AND EXISTS "${CMAKE_SOURCE_DIR}/tests/adapter_tests.cpp")
    include(CTest)
    enable_testing()
    add_executable(main_tests tests/main_test.cpp)
    add_executable(adapter_tests tests/adapter_tests.cpp)
    
    # Link test executables with the main library and test libraries
    target_link_libraries(main_tests PRIVATE finance_lib)
    target_link_libraries(adapter_tests PRIVATE finance_lib)
    
    # Add tests
    add_test(NAME MainTest COMMAND main_tests)
    add_test(NAME AdapterTest COMMAND adapter_tests)
endif() 