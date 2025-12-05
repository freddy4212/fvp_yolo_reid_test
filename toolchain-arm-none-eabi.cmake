# toolchain-arm-none-eabi.cmake
# ARM Cortex-M55 工具鏈配置

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 設置編譯器
find_program(CMAKE_C_COMPILER arm-none-eabi-gcc)
find_program(CMAKE_CXX_COMPILER arm-none-eabi-g++)
find_program(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

if(NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR "arm-none-eabi-gcc not found. Please install ARM GNU Toolchain.")
endif()

# 編譯器標誌
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m55 -mthumb -mfloat-abi=hard -mfpu=auto")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-m55 -mthumb -mfloat-abi=hard -mfpu=auto")

# 防止 CMake 測試編譯器
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

# 設置 sysroot (如果需要)
# set(CMAKE_SYSROOT /path/to/arm-none-eabi)

# 搜尋程式的位置
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)