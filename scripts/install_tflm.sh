#!/bin/bash
# install_tflm.sh - 安裝 TensorFlow Lite Micro (修正版)

set -e

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
# Assume project root is one level up if script is in scripts/
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

INSTALL_DIR="${PROJECT_ROOT}/external/tensorflow"

echo "=========================================="
echo "Installing TensorFlow Lite Micro"
echo "=========================================="
echo ""
echo "Install directory: ${INSTALL_DIR}"
echo ""

# 1. 檢查是否已存在
if [ -d "${INSTALL_DIR}" ]; then
    echo "TensorFlow directory already exists."
    read -p "Remove and reinstall? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -rf "${INSTALL_DIR}"
    else
        echo "Using existing installation."
        exit 0
    fi
fi

# 2. 克隆 TensorFlow Lite Micro (正確的倉庫位址)
echo "Cloning TensorFlow Lite Micro..."
echo "This may take a few minutes..."
echo ""

# 官方 tflite-micro 倉庫
git clone --depth 1 \
    https://github.com/tensorflow/tflite-micro.git \
    "${INSTALL_DIR}"

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to clone repository"
    echo ""
    echo "Trying alternative method..."
    
    # 備選方案：從主 TensorFlow 倉庫克隆 (只取必要的部分)
    echo "Cloning from main TensorFlow repository..."
    git clone --depth 1 --filter=blob:none --sparse \
        https://github.com/tensorflow/tensorflow.git \
        "${INSTALL_DIR}_temp"
    
    cd "${INSTALL_DIR}_temp"
    git sparse-checkout set tensorflow/lite/micro
    git sparse-checkout add tensorflow/lite/schema
    git sparse-checkout add tensorflow/lite/core
    cd ..
    
    mv "${INSTALL_DIR}_temp" "${INSTALL_DIR}"
    
    if [ $? -ne 0 ]; then
        echo "ERROR: Both methods failed"
        exit 1
    fi
fi

echo "✓ TensorFlow Lite Micro cloned"

# 3. 驗證安裝
echo ""
echo "Verifying installation..."

# 檢查多個可能的位置
POSSIBLE_HEADERS=(
    "${INSTALL_DIR}/tensorflow/lite/micro/micro_interpreter.h"
    "${INSTALL_DIR}/tensorflow/lite/micro/micro_mutable_op_resolver.h"
)

FOUND_HEADERS=0
for HEADER in "${POSSIBLE_HEADERS[@]}"; do
    if [ -f "${HEADER}" ]; then
        echo "✓ Found: ${HEADER}"
        FOUND_HEADERS=$((FOUND_HEADERS + 1))
    fi
done

if [ $FOUND_HEADERS -eq 0 ]; then
    echo "ERROR: No TFLite Micro headers found"
    echo ""
    echo "Directory structure:"
    tree -L 3 "${INSTALL_DIR}" 2>/dev/null || find "${INSTALL_DIR}" -maxdepth 3 -type f -name "*.h" | head -20
    exit 1
fi

# 4. 顯示目錄結構
echo ""
echo "Directory structure:"
if command -v tree &> /dev/null; then
    tree -L 3 "${INSTALL_DIR}/tensorflow/lite" 2>/dev/null || true
else
    find "${INSTALL_DIR}/tensorflow/lite" -maxdepth 3 -type d 2>/dev/null | head -20
fi

# 5. 完成
echo ""
echo "=========================================="
echo "✓ Installation Complete"
echo "=========================================="
echo ""
echo "TensorFlow Lite Micro installed at:"
echo "  ${INSTALL_DIR}"
echo ""
echo "Include path for CMake:"
echo "  include_directories(\${CMAKE_SOURCE_DIR}/tensorflow)"
echo ""
echo "Key directories:"
ls -la "${INSTALL_DIR}/tensorflow/lite/" 2>/dev/null | grep "^d" || echo "  (use 'ls -la ${INSTALL_DIR}' to explore)"
echo ""