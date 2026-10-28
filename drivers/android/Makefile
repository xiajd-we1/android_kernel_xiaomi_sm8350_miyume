# SPDX-License-Identifier: GPL-2.0-only
ccflags-y += -I$(src)			# needed for trace events

obj-$(CONFIG_ANDROID_BINDERFS)		+= binderfs.o
obj-$(CONFIG_ANDROID_BINDER_IPC)	+= binder.o binder_alloc.o
obj-$(CONFIG_ANDROID_BINDER_IPC_SELFTEST) += binder_alloc_selftest.o
obj-$(CONFIG_ANDROID_VENDOR_HOOKS) += vendor_hooks.o

# hwui_mon / jank_detect_demo are deliberately NOT built.
#
# 1) hwui_mon registers a uprobe on /system/lib64/libhwui.so. The kernel
#    installs the out-of-line (XOL) slot via _install_special_mapping() with
#    VM_EXEC and vm_file == NULL, so every process that touches the probed
#    page grows an anonymous executable VMA named "[uprobes]". SUSFS cannot
#    hide it: SUSFS_IS_INODE_SUS_MAP() keys on vma->vm_file, and its own
#    Kconfig states anon memory is out of scope. Detectors flag that VMA.
#
# 2) The uprobe also breaks the probed code page, COW-ing libhwui.so into
#    private dirty pages, which surfaces as "anonymous executable pages in
#    /system/lib64/libhwui.so" via smaps.
#
# 3) jank_detect_demo prints the probed process name on every frame through
#    pr_info(), bypassing the klog.h no_printk shim (it does not include
#    klog.h), leaking "in weishu.kernelsu with frametime" into dmesg.
#
# Both files are LibreXZR frame-timing demos, unrelated to KernelSU.
