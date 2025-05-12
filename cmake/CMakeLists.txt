# cmake/CMakeLists.txt
message(STATUS "載入 CMake 自定義模組...")

# 將目前資料夾加入 module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

# 引入各功能模組
include(GlobalOptions)
include(BuildMainExecutable)
include(ConfigureTests)

# 執行
DefineGlobalOptions()
BuildMainExecutable()
ConfigureTests()
