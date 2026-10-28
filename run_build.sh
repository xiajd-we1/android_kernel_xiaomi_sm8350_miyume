#!/bin/bash
K="/home/kria/kernelsu集成"
cd "$K" || exit 1
LOG="$K/build_venus.log"
rm -f "$LOG"

rm -f out_venus/kernel/config_data out_venus/kernel/config_data.gz
rm -f out_venus/kernel/configs.o
rm -f out_venus/kernel/kheaders_data.tar.xz out_venus/kernel/kheaders.o
rm -f out_venus/include/generated/compile.h
rm -f out_venus/include/config/auto.conf.cmd

export PATH="/usr/lib/llvm-18/bin:$PATH"
export KBUILD_BUILD_USER="builder"
export KBUILD_BUILD_HOST="pangu-build-component-vendor-16147-mz6v3-3zhx1-cd737"
export KBUILD_BUILD_TIMESTAMP="Mon Jan 30 19:37:13 CST 2023 f2fs-hash:5d53205670"
export KBUILD_BUILD_VERSION="1"
export KBUILD_BUILD_COMPILER="Android (6877366 based on r383902b1) clang version 11.0.2 (https://android.googlesource.com/toolchain/llvm-project b397f81060ce6d701042b782172ed13bee898b79)"
export KBUILD_BUILD_LLD="LLD 11.0.2 (https://android.googlesource.com/toolchain/llvm-project b397f81060ce6d701042b782172ed13bee898b79)"
export LOCALVERSION=""
export KSU_SCM_OVERRIDE="369674717ad3"

echo "START $(date)" > "$LOG"
make -j4 ARCH=arm64 LLVM=1 LLVM_IAS=1 O=out_venus venus_defconfig >> "$LOG" 2>&1
echo "defconfig EXIT=$?" >> "$LOG"
make -j4 ARCH=arm64 LLVM=1 LLVM_IAS=1 O=out_venus modules Image >> "$LOG" 2>&1
echo "BUILD EXIT=$?" >> "$LOG"
echo "DONE $(date)" >> "$LOG"
