# Xiaomi SM8350 (miyume/venus) 内核源码 — 双版本归档

本仓库包含两套**已验证可开机**的内核源码改动，分目录存放，便于对比与回滚。

## 目录结构

```
table3/          ← Stable3 版本（ReSukiSU 35061 + SUSFS v2.1.0，全功能）
  source/        ← 相对 Stable3 基线 (6f0290557) 的改动文件
  Image          ← 编译产物
  README.md      ← 详细说明

bakasu/          ← BakaSU 版本（BakaSU 35222 + SUSFS v2.3.0，官方纯 SUSFS 路线）
  source/        ← 相对 BakaSU manual 基线 (726f838eb) 的改动文件
  Image          ← 编译产物
  README.md      ← 详细说明
```

## 两版本对比

| 项目 | table3 | bakasu |
|---|---|---|
| 基线提交 | `6f0290557` (Stable3) | `726f838eb` (bakasu manual) |
| KernelSU | ReSukiSU `a9216b04` (35061) | BakaSU `5b76b884` (35222) |
| SUSFS 版本 | v2.1.0 | **v2.3.0** |
| SUSFS 模式 | Inline Hook | Inline Hook（官方纯 SUSFS） |
| 子功能 | 9 个全部开启 | 全部关闭（待逐个验证） |
| 开机验证 | ✅ | ✅ |
| root | ✅ | ✅ |
| 管理器 | ✅ | ✅ |

## 关键说明

### table3（稳定版）
- 使用 ReSukiSU 35061，SUSFS v2.1.0
- **9 个 SUSFS 子功能全部开启**
- 已验证：开机、root、管理器、SUSFS 通信全部正常
- 这是**功能最完整**的版本

### bakasu（新移植版）
- 使用 BakaSU 35222，按 BakaSU 官方文档移植 SUSFS v2.3.0
- 纯 SUSFS 模式（`CONFIG_KSU_MANUAL_HOOK` 关闭）
- 当前**所有 SUSFS 子功能关闭**（仅验证基础框架）
- 已完成 10 项 5.4 内核适配

## 如何回滚

```bash
# 回到 table3 基线
git checkout 6f0290557
git checkout table3/source/. -- .   # 或者用 table3/source 下的文件覆盖

# 回到 bakasu 基线
git checkout 726f838eb
```

## 编译命令

```bash
export PATH=/usr/lib/llvm-18/bin:$PATH
make -j$(nproc) ARCH=arm64 LLVM=1 LLVM_IAS=1 O=out_venus venus_defconfig
make -j$(nproc) ARCH=arm64 LLVM=1 LLVM_IAS=1 O=out_venus modules Image
```

产物：`out_venus/arch/arm64/boot/Image`
