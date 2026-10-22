// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#include <linux/jump_label.h>
#include <linux/rwsem.h>

#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
extern struct static_key_false susfs_is_uname_spoof_buffer_set;
extern void susfs_spoof_uname(struct new_utsname *tmp);
#endif

static int version_proc_show(struct seq_file *m, void *v)
{
	struct new_utsname ut;

	/*
	 * Keep /proc/version consistent with what uname(2) returns, so a
	 * SUSFS spoof (or the absence of one) looks identical from every
	 * source: uname(2), /proc/version and /proc/sys/kernel/*.
	 */
	down_read(&uts_sem);
	memcpy(&ut, utsname(), sizeof(ut));
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
	if (static_branch_likely(&susfs_is_uname_spoof_buffer_set))
		susfs_spoof_uname(&ut);
#endif
	up_read(&uts_sem);

	seq_printf(m, linux_proc_banner, ut.sysname, ut.release, ut.version);
	return 0;
}

static int __init proc_version_init(void)
{
	proc_create_single("version", 0, NULL, version_proc_show);
	return 0;
}
fs_initcall(proc_version_init);
