# bakasu — BakaSU 35222 + SUSFS v2.3.0（官方纯 SUSFS 路线）

## 基本信息

- **基线**：`726f838eb` (`xiajd-we1/bakasu` 分支，manual hook 版)
- **当前提交**：`c187ac43e` (`susfs-v230-bootable` 分支)
- **KernelSU**：BakaSU `5b76b884` (v4.2.0-rc3, KSU_VERSION 35222, UAPI 5)
- **SUSFS**：**v2.3.0**（来自 `susfs4ksu` 的 `gki-android12-5.10` 分支）
- **内核**：Linux 5.4.147，arm64
- **设备**：Xiaomi M2011K2C (venus / miyume)

## 验证状态

| 项目 | 状态 |
|---|---|
| 开机 | ✅ 已验证 |
| root | ✅ `su -c id` → `uid=0(root) context=u:r:ksu:s0` |
| 管理器 | ✅ `ksud 4.2.0-rc3` uapi:5 |
| SUSFS 通信 | ✅ `ksud susfs show version` → v2.3.0 |
| Image sha256 | `5029eb241bef1c6f9ddb3b4c3fdb0736e3f90b4028e28a7824cb8eb3cb713ea6` |

## 当前配置（纯 SUSFS 模式）

```
CONFIG_KSU=y
# CONFIG_KSU_MANUAL_HOOK is not set
CONFIG_KSU_SUSFS=y
# CONFIG_KSU_SUSFS_SUS_PATH is not set    ← 待开启
# CONFIG_KSU_SUSFS_SUS_MOUNT is not set
# CONFIG_KSU_SUSFS_SUS_KSTAT is not set
# CONFIG_KSU_SUSFS_SPOOF_UNAME is not set
# CONFIG_KSU_SUSFS_ENABLE_LOG is not set
# CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS is not set
# CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG is not set
# CONFIG_KSU_SUSFS_OPEN_REDIRECT is not set
# CONFIG_KSU_SUSFS_SUS_MAP is not set
# CONFIG_KPROBES is not set
# CONFIG_PANIC_ON_OOPS is not set
```

## 官方移植流程（已执行）

1. 剔除 SUSFS v2.1.0（296 个 `#ifdef` 块 / 1096 行 / 22 文件 + 3 个文件 + fs/Makefile + 4 个 defconfig）
2. 重建 KernelSU = BakaSU `5b76b884` + `klog_h_silence.patch`
   - `KSU_VERSION = 30000 + 4522 + 700 = 35222`
3. 部署 SUSFS v2.3.0 三文件：
   - `fs/susfs.c` (1538 行)
   - `include/linux/susfs.h` (243 行，`#define SUSFS_VERSION "v2.3.0"`)
   - `include/linux/susfs_def.h` (162 行)
4. 应用官方 `50_add_susfs_in_gki-android12-5.10.patch`（2737 行 / 24 文件）
5. 5.4 内核适配（10 项，见下）
6. defconfig 设纯 SUSFS 模式

## 10 项 5.4 适配（均为实测编译/运行错误驱动）

| # | 问题 | 修复 |
|---|---|---|
| 1 | `struct kstat` 无 `mnt_id` 字段（5.10 新增） | `include/linux/stat.h` 补 `unsigned int mnt_id;` |
| 2 | `fsnotify_ops.handle_inode_event` 是 5.9+ 字段 | 条件编译：<5.9 用 `handle_event` |
| 3 | `selinux_state.status_lock` 是 5.10 字段 | 改 `READ_ONCE(fake_status)` |
| 4 | `VMA_PAD_START` 是 5.10 宏 | 改 `vma->vm_end` |
| 5 | `filename_lookup` 5.4 签名不同（第 5 参是 `struct path *root` 而非 `struct nameidata *nd`） | 修正 extern 声明 |
| 6 | `ksu_setup_preinit_file` BakaSU 无此功能 | 删除 preinit 代码 |
| 7 | `my_setprocattr` 未导出 | 删除内核侧引用（BakaSU 用 `ksu_patch_text` 运行时替换） |
| 8 | `my_write_context` / `my_write_access` 未导出 | 同上 |
| 9 | `fake_status` 外部访问失败 | 删除引用 |
| 10 | 多个孤儿 `#endif` | 逐个删除 |

## 改动文件（相对 726f838eb，共 32 个）

**SUSFS 核心（3 个）**：
- `fs/susfs.c`
- `include/linux/susfs.h`
- `include/linux/susfs_def.h`

**SUSFS 钩子文件（21 个）**：
- `fs/Makefile`、`fs/exec.c`、`fs/namei.c`、`fs/namespace.c`、`fs/notify/fdinfo.c`
- `fs/open.c`、`fs/proc/base.c`、`fs/proc/cmdline.c`、`fs/proc/fd.c`
- `fs/proc/task_mmu.c`、`fs/proc/version.c`、`fs/proc_namespace.c`
- `fs/read_write.c`、`fs/readdir.c`、`fs/stat.c`、`fs/statfs.c`、`fs/super.c`
- `kernel/reboot.c`、`kernel/sys.c`、`kernel/utsname_sysctl.c`
- `drivers/input/input.c`、`security/selinux/avc.c`、`security/selinux/hooks.c`、`security/selinux/selinuxfs.c`

**配置与文档（5 个）**：
- `arch/arm64/configs/venus_defconfig`（主配置）
- `arch/arm64/configs/haydn_defconfig`、`odin_defconfig`、`star_defconfig`
- `SUSFS_PORTING.md`（详细移植文档 486 行）

## 关键坑（已踩过）

1. **`CONFIG_PANIC_ON_OOPS=y` 会导致卡二屏**
   - OEM 内核每次开机会触发无害 WARNING（`dev_pm_attach_wake_irq`）
   - 开了 PANIC_ON_OOPS 后，这个 WARNING 被当致命错误 → 立即 panic → 循环重启
   - **必须保持 `not set`**

2. **BakaSU 官方明确不支持 MANUAL_HOOK + SUSFS 共存**
   - 官方文档：SUSFS 与 manual hook 补丁冲突
   - 必须用纯 SUSFS 模式

3. **SUSFS 与 5.10 内核结构强绑定**
   - 必须逐项适配 5.4

## 待办：逐个开启 SUSFS 子功能

建议顺序（每开一个编译验证一次）：

| 顺序 | 功能 | 风险 |
|---|---|---|
| 1 | `SUS_PATH` | 低（只影响 namei/readdir 查找） |
| 2 | `SUS_KSTAT` | 中（改 stat 结果） |
| 3 | `SPOOF_UNAME` | 低（只改 uname） |
| 4 | `SPOOF_CMDLINE_OR_BOOTCONFIG` | 低 |
| 5 | `SUS_MOUNT` | **高**（改 mount 结构） |
| 6 | `SUS_MAP` | 中（改 /proc/pid/maps） |
| 7 | `OPEN_REDIRECT` | 中 |
| 8 | `ENABLE_LOG` | 低（只影响日志） |
| 9 | `HIDE_KSU_SUSFS_SYMBOLS` | 低（只影响 kallsyms） |
