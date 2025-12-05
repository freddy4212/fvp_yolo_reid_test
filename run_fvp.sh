#!/bin/bash
# run_fvp.sh - FVP 執行腳本

set -e  # 遇到錯誤立即退出
set -x  # Print commands

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
# Project root is the script directory
PROJECT_ROOT="$SCRIPT_DIR"

if [ ! -f "$PROJECT_ROOT/CMakeLists.txt" ]; then
    echo "ERROR: Cannot find CMakeLists.txt in $PROJECT_ROOT"
    echo "Please run this script from the project root."
    exit 1
fi

cd "$PROJECT_ROOT"

# ============================================================
# X11 顯示設定 (用於 FVP LCD 視窗)
# ============================================================
# 如果 DISPLAY 未設定，嘗試自動設定
if [ -z "$DISPLAY" ]; then
    # 在 Docker/Podman 容器中連接到 macOS XQuartz
    if [ -f /.dockerenv ] || [ -n "$container" ]; then
        export DISPLAY=host.docker.internal:0
        echo "Set DISPLAY=$DISPLAY (container mode)"
    else
        # 本機 Linux
        export DISPLAY=:0
        echo "Set DISPLAY=$DISPLAY (native mode)"
    fi
fi

# ============================================================
# 配置
# ============================================================
FVP_PATH="FVP_Corstone_SSE-300_Ethos-U55"
BUILD_DIR="build"

# 模型和視頻檔案
YOLO_MODEL="yolov8n_pose_256_vela_3_9_0x3BB000.tflite"
REID_MODEL="person_reid_int8_vela_64.tflite"
VIDEO_FILE="illit_dance_short.mp4"

# ============================================================
# 檢查環境
# ============================================================
echo "Checking environment..."

# Kill existing VSI server
pkill -f vsi_video_server.py || true

# 檢查 ARM 工具鏈
if ! command -v arm-none-eabi-gcc &> /dev/null; then
    echo "ERROR: arm-none-eabi-gcc not found"
    echo "Please install ARM GNU Toolchain from:"
    echo "  https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads"
    exit 1
fi

echo "✓ ARM toolchain found: $(arm-none-eabi-gcc --version | head -n1)"

# 檢查 FVP
if ! command -v $FVP_PATH &> /dev/null; then
    echo "WARNING: $FVP_PATH not found in PATH"
    echo "Trying to locate FVP..."
    
    # 嘗試常見位置
    FVP_LOCATIONS=(
        "/opt/arm/FVP_Corstone_SSE-300/models/Linux64_GCC-9.3/$FVP_PATH"
        "/usr/local/bin/$FVP_PATH"
        "$HOME/ARM/FVP_Corstone_SSE-300/models/Linux64_GCC-9.3/$FVP_PATH"
    )
    
    for loc in "${FVP_LOCATIONS[@]}"; do
        if [ -f "$loc" ]; then
            FVP_PATH="$loc"
            echo "✓ Found FVP at: $FVP_PATH"
            break
        fi
    done
    
    if ! [ -f "$FVP_PATH" ]; then
        echo "ERROR: FVP not found. Please install ARM FVP."
        exit 1
    fi
fi

# 檢查檔案
echo "Checking files..."
for file in "models/$YOLO_MODEL" "models/$REID_MODEL" "test_videos/$VIDEO_FILE"; do
    if [ ! -f "$file" ]; then
        echo "ERROR: File not found: $file"
        exit 1
    fi
    echo "✓ Found: $file"
done

# ============================================================
# 編譯專案
# ============================================================
echo ""
echo "Building project..."

# 建立 build 目錄
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# CMake 配置
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm-none-eabi.cmake \
    -DYOLO_MODEL="$YOLO_MODEL" \
    -DREID_MODEL="$REID_MODEL" \
    -DVIDEO_INPUT="$VIDEO_FILE" \
    -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo "ERROR: CMake configuration failed"
    exit 1
fi

# 編譯
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "ERROR: Build failed"
    exit 1
fi

echo "✓ Build successful"

# 複製 VSI 腳本到 build 目錄
cp ../python/arm_vsi0.py .
cp ../python/arm_vsi1.py .
cp ../python/vsi_video.py .
cp ../python/vsi_video_out.py .
cp ../python/vsi_video_server.py .
cp -r ../test_videos .
echo "✓ Copied VSI scripts and videos to build directory"

# ============================================================
# 執行 FVP
# ============================================================
echo ""
echo "Running on FVP..."
echo "Configuration:"
echo "  YOLO Model: $YOLO_MODEL"
echo "  Re-ID Model: $REID_MODEL"
echo "  Video: $VIDEO_FILE"
echo ""

# GUI 模式預設開啟 (需要 X11)
# 在 macOS 上需要:
#   1. 安裝 XQuartz: brew install --cask xquartz
#   2. 啟動 XQuartz 並在偏好設定 > 安全性中勾選 "允許來自網路客戶端的連接"
#   3. 執行: xhost +localhost
#   4. 在 Podman 容器啟動時加入: -e DISPLAY=host.docker.internal:0
#
# 設置 GUI_MODE=0 可禁用視覺化
GUI_MODE=${GUI_MODE:-1}
TELNET_MODE=${TELNET_MODE:-0}

if [ "$GUI_MODE" = "1" ]; then
    echo "GUI mode enabled - LCD visualization will be shown"
    echo "  (If no window appears, check X11/XQuartz setup)"
    VIS_OPTION="-C mps3_board.visualisation.disable-visualisation=0"
    VIS_TITLE="-C mps3_board.visualisation.window_title=YOLOv8-Pose_ReID"
else
    echo "GUI mode disabled (set GUI_MODE=1 to enable)"
    VIS_OPTION="-C mps3_board.visualisation.disable-visualisation=1"
    VIS_TITLE=""
fi

if [ "$TELNET_MODE" = "1" ]; then
    echo "Telnet terminals enabled on ports 5000, 5001, 5002"
    TELNET_OPTIONS="\
        -C mps3_board.telnetterminal0.start_telnet=1 \
        -C mps3_board.telnetterminal0.start_port=5000 \
        -C mps3_board.telnetterminal1.start_telnet=1 \
        -C mps3_board.telnetterminal1.start_port=5001 \
        -C mps3_board.telnetterminal2.start_telnet=1 \
        -C mps3_board.telnetterminal2.start_port=5002"
else
    echo "Telnet terminals disabled (set TELNET_MODE=1 to enable)"
    TELNET_OPTIONS="\
        -C mps3_board.telnetterminal0.start_telnet=0 \
        -C mps3_board.telnetterminal1.start_telnet=0 \
        -C mps3_board.telnetterminal2.start_telnet=0"
fi

echo ""

$FVP_PATH \
    -C cpu0.CFGDTCMSZ=15 \
    -C cpu0.CFGITCMSZ=15 \
    -C mps3_board.uart0.out_file="-" \
    -C mps3_board.uart0.shutdown_on_eot=1 \
    $VIS_OPTION \
    $VIS_TITLE \
    $TELNET_OPTIONS \
    -C mps3_board.DISABLE_GATING=1 \
    -C cpu0.semihosting-enable=1 \
    -C ethosu.num_macs=64 \
    --quantum=1000000 \
    -C mps3_board.smsc_91c111.enabled=0 \
    -C mps3_board.hostbridge.userNetworking=0 \
    -C mps3_board.v_path=. \
    --stat \
    -a fvp_yolo_reid_test

echo ""
echo "✓ FVP execution completed"
