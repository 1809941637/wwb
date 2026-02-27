#!/bin/bash

# VLA Orin 项目构建脚本
# 使用方法: ./build.sh [clean|cmake]

set -e

BUILD_DIR="build"
CMAKE_BUILD_TYPE="Release"
EXECUTABLE_NAME="vlaOrin"

# 清理构建目录
if [ "$1" == "clean" ]; then
    echo "清理构建目录..."
    rm -rf ${BUILD_DIR}
    exit 0
fi

# 强制重新配置CMake
if [ "$1" == "cmake" ]; then
    echo "强制重新配置CMake..."
    if [ -d "${BUILD_DIR}" ]; then
        rm -rf ${BUILD_DIR}
    fi
fi

# 创建构建目录
if [ ! -d "${BUILD_DIR}" ]; then
    mkdir -p ${BUILD_DIR}
fi

# 进入构建目录
cd ${BUILD_DIR}

# 检查是否需要运行CMake（检查CMakeCache.txt是否存在）
if [ ! -f "CMakeCache.txt" ]; then
    echo "首次配置CMake，生成Makefile..."
    cmake .. -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    echo "CMake配置完成！"
    echo ""
else
    echo "检测到已存在的CMake配置，跳过CMake步骤"
    echo "如需重新配置，请运行: ./build.sh cmake"
    echo ""
fi

# 编译
echo "开始编译..."
make -j$(nproc)

echo ""
echo "编译完成！可执行文件位于: ${BUILD_DIR}/${EXECUTABLE_NAME}"
echo ""


