#!/bin/bash
# 用法: bash build_device.sh <设备代号>
# 例如: bash build_device.sh venus
# 不带参数默认 venus

device=${1:-venus}
workdir=$(cd "$(dirname "$0")"; pwd)
cd "$workdir"

echo "构建设备: $device"

# 工具链: Clang 18.1.8 & LLD 18.1.8
export PATH=/usr/lib/llvm-18/bin:$PATH

# 构建信息与官方 boot 镜像对齐 (uname -a / /proc/version 与原版一致)
export KBUILD_BUILD_USER="builder"
export KBUILD_BUILD_HOST="pangu-build-component-vendor-16147-mz6v3-3zhx1-cd737"
export KBUILD_BUILD_TIMESTAMP="Mon Jan 30 19:37:13 CST 2023"
export KBUILD_BUILD_VERSION="1"
# LINUX_COMPILER 字符串与官方 clang 11.0.2 保持一致 (真实编译仍用 clang 18)
export KBUILD_BUILD_COMPILER="Android (6877366 based on r383902b1) clang version 11.0.2 (https://android.googlesource.com/toolchain/llvm-project b397f81060ce6d701042b782172ed13bee898b79)"
export KBUILD_BUILD_LLD="LLD 11.0.2 (https://android.googlesource.com/toolchain/llvm-project b397f81060ce6d701042b782172ed13bee898b79)"
# 抑制 setlocalversion 对未打 annotated tag 的仓库追加尾缀 "+"
export LOCALVERSION=""

# 配置内核
make -j$(nproc --all) ARCH=arm64 LLVM=1 LLVM_IAS=1 O=out_${device} ${device}_defconfig

# 编译内核
make -j$(nproc --all) ARCH=arm64 LLVM=1 LLVM_IAS=1 O=out_${device} modules Image
