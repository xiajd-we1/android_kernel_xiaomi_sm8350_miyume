# BakaSU + SUSFS v2.3.0 移植文档（Xiaomi SM8350 / venus / 内核 5.4.147）

> 本文档记录将 **SUSFS v2.3.0** 移植到 **非 GKI 内核 5.4.147**（小米 11 / M2011K2C / venus）的完整过程。
> 基线：`xiajd-we1/android_kernel_xiaomi_sm8350_miyume` 分支 `bakasu`，提交 `726f838eb`。

---

## 一、最终状态（已验证）

| 项目 | 值 |
|---|---|
| 内核版本 | `5.4.147-qgki-g369674717ad3`（与官方一致，防检测） |
| SUSFS 版本 | `v2.3.0` |
| KernelSU | BakaSU `5b76b884`（4522 提交）+ klog 静音补丁 |
| KSU_VERSION | `35222` |
| 编译产物 | `Image` 46,508,544 bytes |
| **验证结果** | **开机 ✓　root ✓　BakaSU 管理器 ✓　`ksud susfs show version` = v2.3.0 ✓** |

### 当前配置（纯 SUSFS 模式，官方推荐）

```kconfig
CONFIG_KSU=y
# CONFIG_KSU_MANUAL_HOOK is not set     ← 必须关闭（官方声明与 SUSFS 冲突）
CONFIG_KSU_SUSFS=y
# CONFIG_KPROBES is not set             ← v2.0.0+ 不需要
# CONFIG_KSU_SUSFS_SUS_PATH is not set  ← 9 个子功能，按需逐个开启
# CONFIG_KSU_SUSFS_SUS_MOUNT is not set
# CONFIG_KSU_SUSFS_SUS_KSTAT is not set
# CONFIG_KSU_SUSFS_SPOOF_UNAME is not set
# CONFIG_KSU_SUSFS_ENABLE_LOG is not set
# CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS is not set
# CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG is not set
# CONFIG_KSU_SUSFS_OPEN_REDIRECT is not set
# CONFIG_KSU_SUSFS_SUS_MAP is not set
# CONFIG_PANIC_ON_OOPS is not set        ← 重要：开启会导致 Oops 直接 panic 循环重启
```

---

## 二、为什么必须用纯 SUSFS 模式

BakaSU 官方文档（`docs/`）明确说明：

> SUSFS patches may conflict with some patches like **custom manual hooks for sucompat**
> since SUSFS already includes its own sucompat patches in KernelSU and kernel code.

因此 **`CONFIG_KSU_MANUAL_HOOK` 与 `CONFIG_KSU_SUSFS` 不能同时开启**。

### 之前失败的原因（历史记录）

| 尝试 | 做法 | 结果 | 根因 |
|---|---|---|---|
| 1 | BakaSU + `MANUAL_HOOK=y` + 树内遗留 SUSFS v2.1.0 | 卡二屏 | v2.1.0 的 `ksu_handle_stat` 传 `const char __user **`，BakaSU 期望 `struct filename **` |
| 2 | BakaSU + `MANUAL_HOOK=y` + SUSFS v2.3.0 共存 | 卡二屏 | 官方声明冲突，KSU 走 manual 路径但 SUSFS 代码假设 inline hook |
| 3 | **BakaSU + `MANUAL_HOOK=n` + SUSFS v2.3.0（纯 SUSFS）** | **开机成功** | 官方推荐路线 |

---

## 三、官方移植步骤

来源：[`simonpunk/susfs4ksu`](https://gitlab.com/simonpunk/susfs4ksu) 分支 **`gki-android12-5.10`**

BakaSU 官方文档要求：

> SUSFS is no longer supported for NonGKI kernels, if you want to use SUSFS on NonGKI kernel,
> please **manually backport the patches from `gki-android12-5.10` branch**.

### Step 1：复制内核侧文件

```bash
SUSFS_REPO=/path/to/susfs4ksu
BRANCH=origin/gki-android12-5.10

git show $BRANCH:kernel_patches/fs/susfs.c               > fs/susfs.c
git show $BRANCH:kernel_patches/include/linux/susfs.h    > include/linux/susfs.h
git show $BRANCH:kernel_patches/include/linux/susfs_def.h > include/linux/susfs_def.h
```

文件规格：
- `fs/susfs.c` — 1538 行
- `include/linux/susfs.h` — 243 行（含 `#define SUSFS_VERSION "v2.3.0"`）
- `include/linux/susfs_def.h` — 162 行

### Step 2：应用内核补丁

```bash
git show $BRANCH:kernel_patches/50_add_susfs_in_gki-android12-5.10.patch > /tmp/50_add.patch
patch -p1 --forward < /tmp/50_add.patch
```

补丁涉及 **24 个文件**：

```
drivers/input/input.c          fs/Makefile           fs/exec.c
fs/namei.c                     fs/namespace.c        fs/notify/fdinfo.c
fs/open.c                      fs/proc/base.c        fs/proc/bootconfig.c
fs/proc/fd.c                   fs/proc/task_mmu.c    fs/proc_namespace.c
fs/read_write.c                fs/readdir.c          fs/stat.c
fs/statfs.c                    fs/super.c            kernel/kallsyms.c
kernel/reboot.c                kernel/sys.c          mm/memory.c
security/selinux/avc.c         security/selinux/hooks.c
security/selinux/selinuxfs.c
```

### Step 3：KernelSU 侧

**BakaSU 已内置 SUSFS 支持，不需要 `10_enable_susfs_for_ksu.patch`。**

验证依据：
- BakaSU `KernelSU/kernel/Kconfig` 有 10 个 `CONFIG_KSU_SUSFS*` 选项
- `KernelSU/kernel/supercall/dispatch.c` 有 21 处 `CONFIG_KSU_SUSFS` 分支
- `KernelSU/kernel/selinux/selinux.c` 提供 9 个 `susfs_*` 符号
- `KernelSU/kernel/tools/susfs_compat.mk` 和 `inline_hook_check.mk` 是专门的 SUSFS 集成检查

**KernelSU 只需替换为 `5b76b884` + klog 静音补丁：**

```bash
cd KernelSU
git checkout 5b76b884              # 4522 提交 → KSU_VERSION 35222
patch -p1 < ../klog_h_silence.patch
```

`klog_h_silence.patch` 把 `klog.h` 里的 `pr_*` 全部改为 `no_printk()`，
使 `"KernelSU"` / `"BakaSU"` 字符串不出现在 dmesg 中（防检测）。

---

## 四、5.4 适配清单（10 项）

官方补丁基于 5.10，直接应用到 5.4 会有编译错误。以下每一项都是**实测编译报错后**才做的适配，不是猜测。

### 1. `struct kstat.mnt_id`（5.10 新增字段）

**文件**：`include/linux/stat.h`

```c
struct kstat {
	...
	u64		blocks;
	/* 5.10 backport: mount ID, used by SUSFS SUS_KSTAT spoofing */
	unsigned int	mnt_id;
};
```

**原因**：`fs/susfs.c:602` 使用 `stat->mnt_id = entry->spoofed_mnt_id;`，5.4 无此字段。

### 2. `fsnotify_ops.handle_inode_event`（5.9+ 字段）

**文件**：`fs/susfs.c`

```c
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
static int susfs_handle_sdcard_inode_event(struct fsnotify_mark *mark, u32 mask,
											struct inode *inode, struct inode *dir,
											const struct qstr *file_name, u32 cookie)
#else
static int susfs_handle_sdcard_inode_event(struct fsnotify_group *group,
										   struct inode *inode, u32 mask,
										   const void *data, int data_type,
										   const struct qstr *file_name,
										   u32 cookie, struct fsnotify_iter_info *iter_info)
#endif
{ ... }

static const struct fsnotify_ops fsnotify_ops = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
	.handle_inode_event = susfs_handle_sdcard_inode_event,
#else
	.handle_event = susfs_handle_sdcard_inode_event,
#endif
};
```

**原因**：5.4 的 `struct fsnotify_ops` 只有 `handle_event`，签名不同。

### 3. `selinux_state.status_lock`（5.10 字段）

**文件**：`security/selinux/selinuxfs.c`

```c
// 5.10 原版
mutex_lock(&selinux_state.status_lock);
data = fake_status;
mutex_unlock(&selinux_state.status_lock);

// 5.4 适配
data = READ_ONCE(fake_status);
```

**原因**：5.4 的 `struct selinux_state` 无 `status_lock` 成员。

### 4. `VMA_PAD_START` 宏（5.10 宏）

**文件**：`fs/proc/task_mmu.c`

```c
// 5.10
end = VMA_PAD_START(vma);
// 5.4
end = vma->vm_end;
```

### 5. `filename_lookup` 签名差异

**文件**：`fs/open.c`、`fs/stat.c`

```c
// 5.10
int filename_lookup(int dfd, struct filename *name, unsigned flags,
                    struct path *path, struct nameidata *nd);
// 5.4
int filename_lookup(int dfd, struct filename *name, unsigned flags,
                    struct path *path, struct path *root);
```

5.4 未在头文件导出，需手动 `extern` 声明。

### 6. `ksu_setup_preinit_file`（BakaSU 无此功能）

**文件**：`fs/exec.c`

BakaSU `5b76b884` 没有 `kernel/preinit/` 目录，SUSFS 补丁假设的 preinit 功能不存在。
**处理：删除全部 preinit 相关代码**（调用块、变量声明、extern 声明、后处理）。

### 7. `my_setprocattr` / `my_write_context` / `my_write_access`（static 函数）

**文件**：`security/selinux/hooks.c`、`security/selinux/selinuxfs.c`

**原因**：这些函数在 `KernelSU/kernel/feature/selinux_hide.c` 中是 `static`，
BakaSU 的设计是**用 `ksu_patch_text()` 在运行时替换函数指针**，不通过符号链接。

**处理：删除内核侧对这些符号的所有引用**：
- `hooks.c`：删除 `extern` 声明、`handle_selinux_setprocattr` 函数、LSM hook 注册改回 `selinux_setprocattr`
- `selinuxfs.c`：删除 `sel_open_handle_status` 的 SUSFS 块、`fake_status` 初始化块、
  `handle_sel_write_context` / `handle_sel_write_access` 包装函数、`write_op[]` 数组简化

### 8. `susfs_init()` 未调用

**文件**：`KernelSU/kernel/core/init.c`

BakaSU 的 `ksu_hook_init()` 是 `#elif` 链：

```c
#if defined(CONFIG_KSU_TRACEPOINT_HOOK)
    ...
#elif defined(CONFIG_KSU_MANUAL_HOOK)
    ksu_lsm_hook_built_in_init();
#elif defined(CONFIG_KSU_SUSFS)
    ksu_lsm_hook_built_in_init();
    susfs_init();          ← 只有这条分支才调用
#endif
```

纯 SUSFS 模式下会正确走到第三条分支，**不需要修改**。
（共存模式才需要补，但共存已被官方否决。）

### 9. `drivers/input/input.c` 缺少钩子

**文件**：`drivers/input/input.c`

BakaSU 编译时会检查 `ksu_handle_input_handle_event`，缺失则报错：
```
You lost ksu_handle_input_handle_event hook in your kernel
```

**补丁内容**（在 `input_handle_event` 函数前加声明，函数内加调用）：

```c
#ifdef CONFIG_KSU_SUSFS
extern struct static_key_true ksu_is_input_hook_enabled;
extern __attribute__((cold)) int ksu_handle_input_handle_event(
	unsigned int *type, unsigned int *code, int *value);
#endif

static void input_handle_event(struct input_dev *dev, unsigned int type,
							   unsigned int code, int value)
{
	...
#ifdef CONFIG_KSU_SUSFS
	if (static_branch_unlikely(&ksu_is_input_hook_enabled))
		ksu_handle_input_handle_event(&type, &code, &value);
#endif
	...
}
```

### 10. 孤儿 `#endif`（模糊匹配遗留）

**文件**：`fs/namei.c`、`fs/namespace.c`、`fs/stat.c`、`fs/notify/fdinfo.c`、`fs/proc/fd.c`

**原因**：用脚本剥离 v2.1.0 的 SUSFS 块时，`#ifdef ... #else ... #endif` 结构被错误处理，
留下无匹配 `#if` 的 `#endif`。

**处理**：逐个定位并删除。验证方法：

```bash
for f in <所有修改的文件>; do
  o=$(grep -cE '^\s*#\s*(ifdef|ifndef|if)\b' $f)
  e=$(grep -cE '^\s*#\s*endif' $f)
  [ "$o" != "$e" ] && echo "$f: if=$o endif=$e ⚠"
done
```

---

## 五、编译

```bash
cd /home/kria/kernel

export PATH=/usr/lib/llvm-18/bin:$PATH
export KBUILD_BUILD_USER="builder"
export KBUILD_BUILD_HOST="pangu-build-component-vendor-16147-mz6v3-3zhx1-cd737"
export KBUILD_BUILD_TIMESTAMP="Mon Jan 30 19:37:13 CST 2023"
export KBUILD_BUILD_VERSION="1"
export KBUILD_BUILD_COMPILER="Android (6877366 based on r383902b1) clang version 11.0.2 (https://android.googlesource.com/toolchain/llvm-project b397f81060ce6d701042b782172ed13bee898b79)"
export KBUILD_BUILD_LLD="LLD 11.0.2 (https://android.googlesource.com/toolchain/llvm-project b397f81060ce6d701042b782172ed13bee898b79)"
export LOCALVERSION=""

make -j$(nproc) ARCH=arm64 LLVM=1 LLVM_IAS=1 O=out_venus venus_defconfig
make -j$(nproc) ARCH=arm64 LLVM=1 LLVM_IAS=1 O=out_venus modules Image
```

**编译时间**：约 45-60 分钟（QCOM 内核，kallsyms 单线程生成耗时最长）

**关键验证点**（编译日志里应出现）：
```
-- BakaSU version code: 35222
-- BakaSU: using SuSFS Inline hook
-- SUSFS_VERSION: v2.3.0
-- BakaSU/susfs_inline: ksu_handle_setresuid found
-- BakaSU/susfs_inline: ksu_handle_execveat found
-- BakaSU/susfs_inline: ksu_handle_faccessat found
-- BakaSU/susfs_inline: ksu_handle_sys_read found
-- BakaSU/susfs_inline: ksu_handle_stat found
-- BakaSU/susfs_inline: ksu_handle_sys_reboot found
-- BakaSU/susfs_inline: ksu_handle_input_handle_event found
```

---

## 六、设备验证

```bash
# 1. 内核版本
adb shell "cat /proc/version"
# 应显示: Linux version 5.4.147-qgki-g369674717ad3

# 2. root
adb shell "su -c id"
# 应显示: uid=0(root) ... context=u:r:ksu:s0

# 3. KSU 管理器
adb shell "su -c 'ksud feature list'"
# 应显示: su_compat ENABLED, kernel_umount ENABLED, selinux_hide ENABLED

# 4. SUSFS 通信
adb shell "su -c 'ksud susfs show version'"
# 应显示: v2.3.0

# 5. SUSFS 子功能状态
adb shell "su -c 'ksud susfs show enabled_features'"
# 未开启子功能时为空

# 6. 测试子功能（以 SUS_PATH 为例）
adb shell "su -c 'mkdir -p /data/local/tmp/test_hide'"
adb shell "su -c 'ksud susfs add_sus_path /data/local/tmp/test_hide'"
# 若子功能未编译: Error: Unsupported SuSFS command: 0x55550
# 若已编译: 无输出（成功）
```

---

## 七、逐步开启子功能

**原则：一次只开一个，编译刷机验证，出问题立刻回滚。**

| 顺序 | 功能 | defconfig 项 | 影响文件 | 风险 |
|---|---|---|---|---|
| 1 | 隐藏路径 | `CONFIG_KSU_SUSFS_SUS_PATH=y` | `fs/namei.c`(14)、`fs/readdir.c`(22)、`fs/proc/base.c` | 低 |
| 2 | 隐藏挂载 | `CONFIG_KSU_SUSFS_SUS_MOUNT=y` | `fs/namespace.c`、`fs/proc/fd.c`、`fs/proc_namespace.c` | 中 |
| 3 | 伪造文件属性 | `CONFIG_KSU_SUSFS_SUS_KSTAT=y` | `fs/stat.c`、`fs/statfs.c`、`fs/proc/task_mmu.c` | 中 |
| 4 | 内核信息伪装 | `CONFIG_KSU_SUSFS_SPOOF_UNAME=y`<br>`CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG=y` | `kernel/utsname_sysctl.c`、`fs/proc/bootconfig.c` | 低 |
| 5 | 日志与隐藏 | `CONFIG_KSU_SUSFS_ENABLE_LOG=y`<br>`CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS=y` | `kernel/kallsyms.c` | 低 |
| 6 | 高级功能 | `CONFIG_KSU_SUSFS_OPEN_REDIRECT=y`<br>`CONFIG_KSU_SUSFS_SUS_MAP=y` | `fs/namei.c`、`mm/memory.c` | 高 |

**注意**：开启子功能时，如果对应的 `.rej` 曾被删除，需要重新从官方补丁中应用该子功能的 hunk。

---

## 八、回滚

```bash
cd /home/kria/kernel

# 回到纯 SUSFS（无子功能）
git checkout susfs-v230-bootable

# 回到 manual 基线（无 SUSFS）
git checkout 726f838eb

# 完全重建（从 GitHub 拉取）
rm -rf /home/kria/kernel
git clone -b bakasu https://github.com/xiajd-we1/android_kernel_xiaomi_sm8350_miyume.git /home/kria/kernel
```

---

## 九、已知问题

1. **`CONFIG_PANIC_ON_OOPS=y` 会导致循环重启**
   OEM 内核每次开机都会触发 `md_testing_mode_probe` 的 WARNING，
   开启 `PANIC_ON_OOPS` 后这个 WARNING 被当成致命错误 → 立即 panic。
   **必须保持 `is not set`。**

2. **9 个子功能全部关闭时 `ksud susfs add_sus_path` 会报错**
   `Error: Unsupported SuSFS command: 0x55550` 是正常的（命令号未编译进去），
   开启对应 `CONFIG_KSU_SUSFS_XXX` 后即可用。

3. **`susfs_add_sus_path` 等符号在 `nm` 中缺失是正常的**
   子功能关闭时对应代码被 `#ifdef` 排除，不影响核心功能。

4. **编译时间长**
   QCOM 内核的 kallsyms 符号表生成是单线程的，6 万多个符号需 10 分钟以上。
   这不是卡死。

---

## 十、文件清单

### 新增文件
```
fs/susfs.c                      1538 行（SUSFS v2.3.0 核心）
include/linux/susfs.h            243 行（版本定义 + 接口）
include/linux/susfs_def.h        162 行（宏与内联函数）
```

### 修改文件（34 个）
```
KernelSU                                   子模块指针（5b76b884）
arch/arm64/configs/{venus,haydn,odin,star}_defconfig   SUSFS 开关
drivers/input/input.c                      input 钩子
fs/Makefile                                susfs.o
fs/exec.c                                  execve 钩子
fs/namei.c                                 路径隐藏钩子（14 处）
fs/namespace.c                             挂载隐藏钩子
fs/notify/fdinfo.c                         fdinfo 钩子
fs/open.c                                  faccessat 钩子
fs/proc/base.c                             proc 钩子
fs/proc/cmdline.c                          cmdline 伪装
fs/proc/fd.c                               proc fd 钩子
fs/proc/task_mmu.c                         maps 钩子
fs/proc/version.c                          版本伪装
fs/proc_namespace.c                        命名空间钩子
fs/read_write.c                            read 钩子
fs/readdir.c                               目录隐藏钩子（22 处）
fs/stat.c                                  stat 钩子
fs/statfs.c                                statfs 钩子
fs/super.c                                 super 钩子
fs/susfs.c                                 SUSFS 核心（替换为 v2.3.0）
include/linux/stat.h                       kstat.mnt_id
include/linux/susfs.h                      SUSFS 头文件（v2.3.0）
include/linux/susfs_def.h                  SUSFS 定义（v2.3.0）
kernel/kallsyms.c                          符号隐藏
kernel/reboot.c                            reboot 钩子
kernel/sys.c                               setresuid 钩子
kernel/utsname_sysctl.c                    uname 伪装
mm/memory.c                                mmap 钩子
security/selinux/avc.c                     AVC 日志伪装
security/selinux/hooks.c                   setprocattr
security/selinux/selinuxfs.c               selinuxfs
```

---

## 十一、参考资料

- [BakaSU 官方仓库](https://github.com/Baka-SU/BakaSU)
- [BakaSU 官方文档](https://bakasu.org/guide/build)
- [SUSFS 官方仓库](https://gitlab.com/simonpunk/susfs4ksu)（分支 `gki-android12-5.10`）
- [SUSFS KernelSU 集成指南](https://kernelsu.org/guide/how-to-integrate-for-non-gki.html)

---

**最后更新**：2026-10-22
**验证状态**：开机 ✓　root ✓　管理器 ✓　SUSFS v2.3.0 通信 ✓
