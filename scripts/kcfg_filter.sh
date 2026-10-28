#!/bin/sh
# 过滤 .config：去掉 KSU / SUSFS 相关行后输出到 stdout
sed -e '/^CONFIG_KSU/d' \
    -e '/^# CONFIG_KSU/d' \
    -e '/^CONFIG_SUSFS/d' \
    -e '/^# CONFIG_SUSFS/d' \
    -e '/^# KernelSU/d' \
    -e '/^# end of KernelSU/d' \
    -e '/^# SUSFS/d' \
    "$1"
