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

# ============================================================
# 构建标识与官方 boot 逐字节对齐
#   参考: boot_official.img 的 linux_banner
#   Linux version 5.4.147-qgki-g369674717ad3 (builder@pangu-build-component-vendor-16147-mz6v3-3zhx1-cd737)
#     (Android (6877366 based on r383902b1) clang version 11.0.2 (...), LLD 11.0.2 (...))
#     #1 SMP PREEMPT Mon Jan 30 19:37:13 CST 2023 f2fs-hash:5d53205670
#
# UTS_VERSION = "#<KBUILD_BUILD_VERSION> <SMP PREEMPT> <KBUILD_BUILD_TIMESTAMP>" (上限 64 字节)
# 官方尾部带 f2fs-hash 段，必须放进 TIMESTAMP 才能逐字节一致。
# " #1 SMP PREEMPT Mon Jan 30 19:37:13 CST 2023 f2fs-hash:5d53205670" 恰好 64 字节。
# ============================================================
export KBUILD_BUILD_USER="builder"
export KBUILD_BUILD_HOST="pangu-build-component-vendor-16147-mz6v3-3zhx1-cd737"
export KBUILD_BUILD_TIMESTAMP="Mon Jan 30 19:37:13 CST 2023 f2fs-hash:5d53205670"
export KBUILD_BUILD_VERSION="1"

# LINUX_COMPILER 与官方 clang 11.0.2 文本一致（实际编译仍用 clang 18）
export KBUILD_BUILD_COMPILER="Android (6877366 based on r383902b1) clang version 11.0.2 (https://android.googlesource.com/toolchain/llvm-project b397f81060ce6d701042b782172ed13bee898b79)"
export KBUILD_BUILD_LLD="LLD 11.0.2 (https://android.googlesource.com/toolchain/llvm-project b397f81060ce6d701042b782172ed13bee898b79)"

# setlocalversion 尾缀抑制；LOCALVERSION 取自 defconfig (-qgki-g369674717ad3，与官方一致)
export LOCALVERSION=""

# CONFIG_LOCALVERSION_AUTO=y 会让 scripts/setlocalversion 用本仓库的
# `git describe` 结果拼 release（我们的是 -g91309f6a，官方是 -g369674717ad3）。
# 这里把它固定成官方值，使 uname -a > /proc/version 与官方 boot 逐字节一致。
export KSU_SCM_OVERRIDE="369674717ad3"

# 配置内核
make -j$(nproc --all) ARCH=arm64 LLVM=1 LLVM_IAS=1 O=out_${device} ${device}_defconfig

# 编译内核
make -j$(nproc --all) ARCH=arm64 LLVM=1 LLVM_IAS=1 O=out_${device} modules Image

# 编译完成后清理中间产物与缓存，避免占用内存/磁盘拖慢主机
scripts/clean_after_build.sh 2>/dev/null || true
