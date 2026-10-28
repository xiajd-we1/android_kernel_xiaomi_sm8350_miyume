#!/bin/bash
# 编译后环境清理：释放内存缓存、清理中间产物与临时文件
# 用法: bash scripts/clean_after_build.sh
#
# 注意：绝不能删除 /tmp/*.log —— 调用方通常正把编译输出重定向到那里，
# unlink 之后日志即丢失（shell 继续写已删除的 inode）。

set -u
K=$(cd "$(dirname "$0")/.."; pwd)
cd "$K" || exit 1

echo
echo "==================== 编译后环境清理 ===================="

# 1) 找出最新成功产物目录
KEEP_DIR=""
for d in out_*; do
    [ -d "$d" ] || continue
    if [ -f "$d/arch/arm64/boot/Image" ]; then
        KEEP_DIR="$d"
    fi
done

# 2) 删除可再生的编译器中间文件（体积大、纯缓存）
echo "[1/4] 清理可再生的编译中间文件"
find out_* -type f \( -name '*.o.cmd' -o -name '*.su' -o -name '.*.tmp' \) -delete 2>/dev/null || true

# 3) 删除本工程自己的临时目录与明确可弃的临时文件（不碰 *.log）
echo "[2/4] 清理临时目录"
rm -rf .tmp_versions 2>/dev/null || true
find out_* -maxdepth 2 -type d -name '.tmp*' -exec rm -rf {} + 2>/dev/null || true
rm -rf /tmp/official_kernel.bin /tmp/susfs 2>/dev/null || true

# 4) 释放 page cache（不触碰用户进程内存）
echo "[3/4] 释放内核页缓存"
sync
if [ -w /proc/sys/vm/drop_caches ]; then
    ( echo 3 > /proc/sys/vm/drop_caches ) 2>/dev/null \
        && echo "      page cache dropped" \
        || echo "      (需要 sudo 才能 drop_caches，已跳过)"
else
    echo "      (无权限，已跳过)"
fi

# 5) 报告
echo "[4/4] 产物与磁盘状态"
if [ -n "$KEEP_DIR" ]; then
    ls -la "$KEEP_DIR/arch/arm64/boot/Image" 2>/dev/null
fi
command -v free >/dev/null 2>&1 && free -h | head -2
df -h "$K" | tail -1

echo "==================== 清理完成 ===================="
