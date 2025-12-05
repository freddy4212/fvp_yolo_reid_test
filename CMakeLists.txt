cmake_minimum_required(VERSION 3.21)

# 設置交叉編譯工具鏈
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

# 使用 ARM GCC 工具鏈
set(TOOLCHAIN_PATH "/Users/freddy/Documents/250727_ARM_FVP/FVPs-on-Mac-Podman/arm-gnu-toolchain-14.3/bin")
find_program(ARM_GCC_COMPILER arm-none-eabi-gcc PATHS ${TOOLCHAIN_PATH} REQUIRED)
find_program(ARM_GXX_COMPILER arm-none-eabi-g++ PATHS ${TOOLCHAIN_PATH} REQUIRED)
find_program(ARM_AR arm-none-eabi-ar PATHS ${TOOLCHAIN_PATH} REQUIRED)

# 設置編譯器
set(CMAKE_C_COMPILER ${ARM_GCC_COMPILER})
set(CMAKE_CXX_COMPILER ${ARM_GXX_COMPILER})
set(CMAKE_ASM_COMPILER ${ARM_GCC_COMPILER})
set(CMAKE_AR ${ARM_AR})

# 禁用編譯器檢查（交叉編譯）
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_ASM_COMPILER_WORKS 1)

project(fvp_yolo_reid_test VERSION 1.0.0 LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# 配置選項
# ============================================================
set(YOLO_MODEL "yolov8n_pose_256_vela.tflite" CACHE STRING "YOLO model file")
set(REID_MODEL "person_reid_int8_vela_64.tflite" CACHE STRING "Re-ID model file")
set(VIDEO_INPUT "illit_dance_short.mp4" CACHE STRING "Input video file")
set(TFLM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/tensorflow)

# ============================================================
# 尋找依賴
# ============================================================

# TensorFlow Lite Micro (假設已安裝)
find_path(TFLM_INCLUDE_DIR 
    NAMES tensorflow/lite/micro/micro_interpreter.h
    PATHS 
        ${CMAKE_CURRENT_SOURCE_DIR}/external/tensorflow
        /usr/local/include
        /usr/include
)

if(NOT TFLM_INCLUDE_DIR)
    message(WARNING "TensorFlow Lite Micro not found, using mock implementation")
    set(USE_MOCK_TFLM ON)
endif()

# ============================================================
# 源文件
# ============================================================
file(GLOB_RECURSE TFLM_SOURCES 
    "${TFLM_INCLUDE_DIR}/tensorflow/lite/micro/*.cc"
    "${TFLM_INCLUDE_DIR}/tensorflow/lite/micro/kernels/*.cc"
    "${TFLM_INCLUDE_DIR}/tensorflow/lite/micro/memory_planner/*.cc"
    "${TFLM_INCLUDE_DIR}/tensorflow/lite/micro/arena_allocator/*.cc"
    "${TFLM_INCLUDE_DIR}/tensorflow/lite/core/api/*.cc"
    "${TFLM_INCLUDE_DIR}/tensorflow/lite/kernels/internal/*.cc"
)

# 排除測試文件和特定平台文件
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*_test\\.cc$")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*_test_.*\\.cc$")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*benchmark.*\\.cc$")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/arc_custom/.*")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/arc_emsdp/.*")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/kernels/arc_mli/.*")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/kernels/cmsis_nn/.*")
# list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/kernels/ethos_u/.*")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/kernels/ethosu.cc$")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/bluepill/.*")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/ceva/.*")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/chre/.*")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/cortex_m_corstone_300/.*")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/cortex_m_generic/.*")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/hexagon/.*")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/riscv32_generic/.*")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/xtensa/.*")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/examples/.*")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/integration_tests/.*")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/python/.*")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/tools/.*")
list(FILTER TFLM_SOURCES EXCLUDE REGEX ".*/test_data_generation/.*")

set(SOURCES
    src/main.cpp
    src/utils/error_reporter_impl.cpp
    src/ai/yolo_pose.cpp
    src/ai/reid.cpp
    src/drivers/vsi_video.cpp
    src/drivers/video_drv.c
    src/utils/image_utils.cpp
    src/utils/draw_utils.cpp
    src/drivers/lcd_display.cpp
    src/ai/yolo_model_data.cc
    src/ai/reid_model_data.cc
    # src/platform/retarget.c
    external/CMSIS_5/Device/ARM/ARMCM55/Source/system_ARMCM55.c
    external/CMSIS_5/Device/ARM/ARMCM55/Source/startup_ARMCM55.c
    ${TFLM_INCLUDE_DIR}/tensorflow/lite/core/c/common.cc
    ${TFLM_INCLUDE_DIR}/tensorflow/lite/kernels/kernel_util.cc
    ${TFLM_INCLUDE_DIR}/tensorflow/compiler/mlir/lite/schema/schema_utils.cc
    ${TFLM_SOURCES}
)

# Ethos-U Driver
set(ETHOSU_DRIVER_PATH "${TFLM_INCLUDE_DIR}/tensorflow/lite/micro/tools/make/downloads/ethos_u_core_driver")
if(EXISTS "${ETHOSU_DRIVER_PATH}")
    message(STATUS "Found Ethos-U Driver at ${ETHOSU_DRIVER_PATH}")
    file(GLOB_RECURSE ETHOSU_DRIVER_SOURCES "${ETHOSU_DRIVER_PATH}/src/*.c")
    list(FILTER ETHOSU_DRIVER_SOURCES EXCLUDE REGEX ".*ethosu_device_u85.c$")
    list(APPEND SOURCES ${ETHOSU_DRIVER_SOURCES})
else()
    message(WARNING "Ethos-U Driver not found at ${ETHOSU_DRIVER_PATH}")
endif()

# ============================================================
# 可執行文件
# ============================================================
add_executable(fvp_yolo_reid_test ${SOURCES})

# 包含目錄
target_include_directories(fvp_yolo_reid_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ai
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${TFLM_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/flatbuffers/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/gemmlowp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/mock_ruy
    ${CMAKE_CURRENT_SOURCE_DIR}/external/CMSIS_5/CMSIS/Core/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/CMSIS_5/Device/ARM/ARMCM55/Include
    ${ETHOSU_DRIVER_PATH}/include
)

# 編譯選項
target_compile_options(fvp_yolo_reid_test PRIVATE
    -mcpu=cortex-m55
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv5-d16
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-int-to-pointer-cast
    $<$<COMPILE_LANGUAGE:C>:-Wno-pointer-to-int-cast>
    -O3
    -flto
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    -DTF_LITE_STATIC_MEMORY
    -DARMCM55
    -DETHOSU55
    -DETHOSU_ARCH=u55
)

# 模型配置
target_compile_definitions(fvp_yolo_reid_test PRIVATE
    YOLO_MODEL_FILE="${YOLO_MODEL}"
    REID_MODEL_FILE="${REID_MODEL}"
    VIDEO_INPUT_FILE="${VIDEO_INPUT}"
)

# Linker script 和記憶體配置
target_link_options(fvp_yolo_reid_test PRIVATE
    -mcpu=cortex-m55
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv5-d16
    -T ${CMAKE_CURRENT_SOURCE_DIR}/external/CMSIS_5/Device/ARM/ARMCM55/Source/GCC/gcc_arm.ld
    --specs=rdimon.specs
    -Wl,--gc-sections
    -flto
    -Wl,-Map=fvp_yolo_reid_test.map
)

# 鏈接庫
target_link_libraries(fvp_yolo_reid_test
    # 如果有 TFLM 庫
    # tensorflow-lite-micro
)

# ============================================================
# 安裝
# ============================================================
install(TARGETS fvp_yolo_reid_test
    RUNTIME DESTINATION bin
)

# 複製模型和視頻文件
install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/models/${YOLO_MODEL}
    ${CMAKE_CURRENT_SOURCE_DIR}/models/${REID_MODEL}
    DESTINATION models
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_videos/${VIDEO_INPUT}
    DESTINATION test_videos
)

message(STATUS "Configuration:")
message(STATUS "  YOLO Model: ${YOLO_MODEL}")
message(STATUS "  Re-ID Model: ${REID_MODEL}")
message(STATUS "  Video Input: ${VIDEO_INPUT}")