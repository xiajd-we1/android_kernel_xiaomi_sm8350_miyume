# table3 — Stable3 (ReSukiSU 35061 + SUSFS v2.1.0)

## 基本信息

- **基线**：`6f0290557` (Stable3 tag)
- **目标提交**：`4dd5c72a3` (`xiajd-we1/stable3_35061` 分支)
- **KernelSU**：ReSukiSU `a9216b04` (v4.2.0-rc1, KSU_VERSION 35061, UAPI 2)
- **SUSFS**：v2.1.0，Inline Hook 模式
- **内核**：Linux 5.4.147，arm64
- **设备**：Xiaomi M2011K2C (venus / miyume)

## 验证状态

| 项目 | 状态 |
|---|---|
| 开机 | ✅ 已验证 |
| root | ✅ `su -c id` → `uid=0(root) context=u:r:ksu:s0` |
| 管理器 | ✅ 通信正常 |
| SUSFS 通信 | ✅ `ksud susfs show version` → v2.1.0 |
| Image sha256 | `8a1dfd7361f234ebfebf5ed9940803244403d807fc05d8ea1652b272f455f7ae` |

## SUSFS 子功能（全部开启）

```
CONFIG_KSU_SUSFS=y
CONFIG_KSU_SUSFS_SUS_PATH=y
CONFIG_KSU_SUSFS_SUS_MOUNT=y
CONFIG_KSU_SUSFS_SUS_KSTAT=y
CONFIG_KSU_SUSFS_SPOOF_UNAME=y
CONFIG_KSU_SUSFS_ENABLE_LOG=y
CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS=y
CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG=y
CONFIG_KSU_SUSFS_OPEN_REDIRECT=y
CONFIG_KSU_SUSFS_SUS_MAP=y
```

## 改动文件（相对 Stable3 基线 6f0290557）

共 10 个文件：

| 文件 | 改动内容 |
|---|---|
| `Makefile` | 版本号配置 |
| `arch/arm64/configs/venus_defconfig` | SUSFS 全功能开启 |
| `build_device.sh` | 编译脚本 |
| `fs/proc/task_mmu.c` | SUSFS SUS_MAP / OPEN_REDIRECT 钩子 |
| `fs/proc/version.c` | 版本伪装 |
| `fs/susfs.c` | SUSFS v2.1.0 核心（1493 行） |
| `kernel/configs.c` | /proc/config.gz 伪装 |
| `kernel/sys.c` | `ksu_handle_setresuid` 钩子 |
| `kernel/utsname_sysctl.c` | uname 伪装 |
| `scripts/mkcompile_h` | 编译版本串伪装 |

## 已知限制

- SUSFS v2.1.0 版本较旧
- BakaSU 管理器可能不兼容（ReSukiSU 与 BakaSU 的 UAPI 版本不同：2 vs 5）
