function(BuildMainExecutable)
    # Find all source files
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
    message(STATUS "找到的 C++ 源文件:")
    foreach(file IN LISTS SOURCES)
        message(STATUS " ${file}")
    endforeach()
    
    # Build executable with project name
    add_executable(${PROJECT_NAME} ${SOURCES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    
    LinkThirdparty(${PROJECT_NAME})
    message(STATUS "已建立可執行目標: ${PROJECT_NAME}")
endfunction()
