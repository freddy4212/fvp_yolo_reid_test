#!/bin/bash
# install_dependencies.sh - 安裝所有依賴

set -e

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
# Assume project root is one level up if script is in scripts/
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

# Create external directory if it doesn't exist
mkdir -p external
cd external

echo "=========================================="
echo "Installing TFLite Micro Dependencies"
echo "=========================================="
echo ""

TFLM_DIR="${PWD}/tensorflow"

# 1. 下載 FlatBuffers
echo "Step 1: Installing FlatBuffers..."

if [ ! -d "flatbuffers" ]; then
    git clone --depth 1 --branch v23.5.26 \
        https://github.com/google/flatbuffers.git
    
    # 編譯 FlatBuffers (如果需要)
    cd flatbuffers
    cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    cd ..
    
    echo "✓ FlatBuffers installed"
else
    echo "✓ FlatBuffers already exists"
fi

# 2. 下載 CMSIS (包含 arm_vsi.h)
echo ""
echo "Step 2: Installing CMSIS..."

if [ ! -d "CMSIS_5" ]; then
    git clone --depth 1 --branch 5.9.0 \
        https://github.com/ARM-software/CMSIS_5.git
    
    echo "✓ CMSIS installed"
else
    echo "✓ CMSIS already exists"
fi

# 3. 下載 Gemmlowp (fixedpoint)
echo ""
echo "Step 3: Installing Gemmlowp..."

if [ ! -d "gemmlowp" ]; then
    git clone --depth 1 https://github.com/google/gemmlowp.git
    echo "✓ Gemmlowp installed"
else
    echo "✓ Gemmlowp already exists"
fi

# 4. 下載 Ruy
echo ""
echo "Step 4: Installing Ruy..."

if [ ! -d "ruy" ]; then
    git clone --depth 1 https://github.com/google/ruy.git
    echo "✓ Ruy installed"
else
    echo "✓ Ruy already exists"
fi

# 5. 下載 KissFFT
echo ""
echo "Step 5: Installing KissFFT..."

if [ ! -d "kissfft" ]; then
    git clone --depth 1 https://github.com/mborgerding/kissfft.git
    echo "✓ KissFFT installed"
else
    echo "✓ Gemmlowp already exists"
fi

# 4. 下載 Ruy
echo ""
echo "Step 4: Installing Ruy..."

if [ ! -d "ruy" ]; then
    git clone --depth 1 https://github.com/google/ruy.git
    echo "✓ Ruy installed"
else
    echo "✓ Ruy already exists"
fi

# 5. 下載 KissFFT
echo ""
echo "Step 5: Installing KissFFT..."

if [ ! -d "kissfft" ]; then
    git clone --depth 1 https://github.com/mborgerding/kissfft.git
    echo "✓ KissFFT installed"
else
    echo "✓ KissFFT already exists"
fi

# 6. 驗證
echo ""
echo "Step 6: Verification..."

REQUIRED_FILES=(
    "flatbuffers/include/flatbuffers/flatbuffers.h"
    "CMSIS_5/CMSIS/Core/Include/arm_vsi.h"
    "gemmlowp/fixedpoint/fixedpoint.h"
    "ruy/ruy/profiler/instrumentation.h"
    "kissfft/kiss_fft.h"
    "tensorflow/tensorflow/lite/micro/micro_interpreter.h"
)

ALL_FOUND=1
for FILE in "${REQUIRED_FILES[@]}"; do
    if [ -f "$FILE" ]; then
        echo "✓ Found: $FILE"
    else
        echo "✗ Missing: $FILE"
        ALL_FOUND=0
    fi
done

if [ $ALL_FOUND -eq 1 ]; then
    echo ""
    echo "✓ All dependencies installed successfully"
else
    echo ""
    echo "⚠ Some dependencies are missing"
    exit 1
fi

echo ""
echo "=========================================="
echo "✓ Installation Complete"
echo "=========================================="