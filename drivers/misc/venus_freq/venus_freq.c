/*
 * venus_freq.c — Venus Frequency QoS Control for Xiaomi Venus (SM8350)
 * Kernel 5.4.147
 *
 * 目的：
 *   mi_thermald 每秒通过 cpu_cooling -> cpufreq_set_cur_state ->
 *   freq_qos_update_request 刷新温控目标，会把用户态写入的
 *   scaling_max_freq/scaling_min_freq 覆盖掉，导致频率限制失效。
 *
 *   本模块为每个 cpufreq policy 注册一个独立的 FREQ_QOS_MAX / FREQ_QOS_MIN
 *   请求。freq_qos 聚合规则是：MAX 取所有请求里的最小值、MIN 取所有请求里
 *   的最大值。因此只要本模块请求的限制更严格，mi_thermald / msm_performance
 *   后续再写宽松值也覆盖不了它。
 *
 * sysfs 接口（/sys/kernel/venus_freq/policy{0,4,7}/）：
 *   max_freq        RW  目标最大频率 (kHz)
 *   min_freq        RW  目标最小频率 (kHz)
 *   enabled         RW  1=启用 QoS 请求 0=移除请求恢复默认
 *   cpuinfo_max_freq RO 硬件最大
 *   cpuinfo_min_freq RO 硬件最小
 *   current_max     RO 聚合后的实际上限 (policy->max)
 *   current_min     RO 聚合后的实际下限 (policy->min)
 *   cur_freq        RO 当前频率
 *
 * 用法：
 *   echo 1        > /sys/kernel/venus_freq/policy0/enabled
 *   echo 1209600  > /sys/kernel/venus_freq/policy0/max_freq
 *   echo 300000   > /sys/kernel/venus_freq/policy0/min_freq
 *   cat /sys/kernel/venus_freq/policy0/current_max
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/cpu.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/kernel.h> /* va_list, vscnprintf */

#define VENUS_FREQ_VERSION  "1.0.0"
#define VENUS_MAX_CLUSTERS  3

struct venus_cluster {
    int cluster_id;
    int first_cpu;
    struct cpufreq_policy *policy;
    struct freq_qos_request qos_max;
    struct freq_qos_request qos_min;
    u32 target_max;
    u32 target_min;
    bool enabled;
    struct kobject kobj;
    struct mutex lock;
};

static struct venus_cluster g_clusters[VENUS_MAX_CLUSTERS];
static struct kobject *venus_kobj;

/* ------------------------------------------------------------------ helpers */

/*
 * 5.4 没有 sysfs_emit，用 scnprintf 兼容。
 * kobj_attribute 的 show 里 buf 至少一页，安全。
 */
static ssize_t venus_emit(char *buf, const char *fmt, ...)
{
    va_list args;
    int len;

    va_start(args, fmt);
    len = vscnprintf(buf, PAGE_SIZE, fmt, args);
    va_end(args);

    return len;
}

/* ------------------------------------------------------------------ sysfs RW */

static ssize_t max_freq_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct venus_cluster *cl = container_of(kobj, struct venus_cluster, kobj);

    return venus_emit(buf, "%u\n", cl->target_max);
}

static ssize_t max_freq_store(struct kobject *kobj, struct kobj_attribute *attr,
                              const char *buf, size_t count)
{
    struct venus_cluster *cl = container_of(kobj, struct venus_cluster, kobj);
    unsigned long val;
    int ret;

    ret = kstrtoul(buf, 10, &val);
    if (ret)
        return ret;

    if (val == 0 || val > cl->policy->cpuinfo.max_freq)
        return -EINVAL;

    mutex_lock(&cl->lock);
    cl->target_max = (u32)val;
    if (cl->enabled && cl->qos_max.qos)
        freq_qos_update_request(&cl->qos_max, cl->target_max);
    mutex_unlock(&cl->lock);

    pr_info("venus_freq: cluster%d (cpu%d) max_freq -> %u\n",
            cl->cluster_id, cl->first_cpu, cl->target_max);

    return count;
}

static ssize_t min_freq_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct venus_cluster *cl = container_of(kobj, struct venus_cluster, kobj);

    return venus_emit(buf, "%u\n", cl->target_min);
}

static ssize_t min_freq_store(struct kobject *kobj, struct kobj_attribute *attr,
                              const char *buf, size_t count)
{
    struct venus_cluster *cl = container_of(kobj, struct venus_cluster, kobj);
    unsigned long val;
    int ret;

    ret = kstrtoul(buf, 10, &val);
    if (ret)
        return ret;

    if (val > cl->policy->cpuinfo.max_freq)
        return -EINVAL;

    mutex_lock(&cl->lock);
    cl->target_min = (u32)val;
    if (cl->enabled && cl->qos_min.qos)
        freq_qos_update_request(&cl->qos_min, cl->target_min);
    mutex_unlock(&cl->lock);

    pr_info("venus_freq: cluster%d (cpu%d) min_freq -> %u\n",
            cl->cluster_id, cl->first_cpu, cl->target_min);

    return count;
}

static ssize_t enabled_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct venus_cluster *cl = container_of(kobj, struct venus_cluster, kobj);

    return venus_emit(buf, "%d\n", cl->enabled ? 1 : 0);
}

static ssize_t enabled_store(struct kobject *kobj, struct kobj_attribute *attr,
                             const char *buf, size_t count)
{
    struct venus_cluster *cl = container_of(kobj, struct venus_cluster, kobj);
    unsigned long val;
    bool want;
    int ret;

    ret = kstrtoul(buf, 10, &val);
    if (ret)
        return ret;
    if (val > 1)
        return -EINVAL;

    want = (bool)val;

    mutex_lock(&cl->lock);

    if (want == cl->enabled) {
        mutex_unlock(&cl->lock);
        return count;
    }

    if (want) {
        /*
         * add_request 直接用目标值而不是硬件上限：用硬件上限作为初值会让
         * cpufreq core 聚合出一次 hw_max，随后 update 到更严格值时若顺序
         * 不当，policy->max 不会重新收敛，实测会出现 current_max 读回
         * 1804800 而 target 已是 1209600 的情况（聚合没刷新）。
         *
         * target_max 为 0 表示用户还没设过上限，此时用硬件上限占位，
         * 等价于"不限制"；target_min 为 0 表示不限制下限。
         */
        u32 req_max = cl->target_max ? cl->target_max : cl->policy->cpuinfo.max_freq;
        u32 req_min = cl->target_min;

        if (!cl->qos_max.qos) {
            ret = freq_qos_add_request(&cl->policy->constraints, &cl->qos_max,
                                       FREQ_QOS_MAX, (s32)req_max);
            if (ret) {
                pr_err("venus_freq: add qos_max failed: %d\n", ret);
                mutex_unlock(&cl->lock);
                return ret;
            }
            ret = freq_qos_add_request(&cl->policy->constraints, &cl->qos_min,
                                       FREQ_QOS_MIN, (s32)req_min);
            if (ret) {
                pr_err("venus_freq: add qos_min failed: %d\n", ret);
                freq_qos_remove_request(&cl->qos_max);
                mutex_unlock(&cl->lock);
                return ret;
            }
        } else {
            /* 已存在请求：直接更新到目标值 */
            freq_qos_update_request(&cl->qos_max, (s32)req_max);
            freq_qos_update_request(&cl->qos_min, (s32)req_min);
        }

        cl->enabled = true;

        pr_info("venus_freq: cluster%d enabled, max=%u min=%u\n",
                cl->cluster_id, req_max, req_min);
    } else {
        if (cl->qos_max.qos) {
            freq_qos_remove_request(&cl->qos_min);
            freq_qos_remove_request(&cl->qos_max);
        }
        cl->enabled = false;
        pr_info("venus_freq: cluster%d disabled\n", cl->cluster_id);
    }

    mutex_unlock(&cl->lock);

    return count;
}

/* ------------------------------------------------------------- sysfs RO */

static ssize_t cpuinfo_max_freq_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct venus_cluster *cl = container_of(kobj, struct venus_cluster, kobj);

    return venus_emit(buf, "%u\n", cl->policy->cpuinfo.max_freq);
}

static ssize_t cpuinfo_min_freq_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct venus_cluster *cl = container_of(kobj, struct venus_cluster, kobj);

    return venus_emit(buf, "%u\n", cl->policy->cpuinfo.min_freq);
}

static ssize_t current_max_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct venus_cluster *cl = container_of(kobj, struct venus_cluster, kobj);

    /*
     * policy->max 就是 cpufreq core 回填的 freq_qos 聚合结果，
     * 直接读它即可。freq_qos_read_value 在 5.4 没有 EXPORT_SYMBOL，
     * 模块里调用会在 MODPOST 阶段报 undefined。
     */
    return venus_emit(buf, "%u\n", cl->policy->max);
}

static ssize_t current_min_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct venus_cluster *cl = container_of(kobj, struct venus_cluster, kobj);

    /* 同上：policy->min 即聚合后的下限 */
    return venus_emit(buf, "%u\n", cl->policy->min);
}

static ssize_t cur_freq_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct venus_cluster *cl = container_of(kobj, struct venus_cluster, kobj);
    unsigned int freq = cpufreq_quick_get(cl->first_cpu);

    return venus_emit(buf, "%u\n", freq);
}

/* ------------------------------------------------------------- attr defs */

static struct kobj_attribute max_freq_attr    = __ATTR_RW(max_freq);
static struct kobj_attribute min_freq_attr    = __ATTR_RW(min_freq);
static struct kobj_attribute enabled_attr     = __ATTR_RW(enabled);

/*
 * RO 属性名与文件名一致：__ATTR_RO(x) 生成 x##_show + 文件名 "x"
 * 所以属性文件名会是 cpuinfo_max_freq / cpuinfo_min_freq /
 * current_max / current_min / cur_freq
 */
static struct kobj_attribute cpuinfo_max_attr = __ATTR_RO(cpuinfo_max_freq);
static struct kobj_attribute cpuinfo_min_attr = __ATTR_RO(cpuinfo_min_freq);
static struct kobj_attribute current_max_attr = __ATTR_RO(current_max);
static struct kobj_attribute current_min_attr = __ATTR_RO(current_min);
static struct kobj_attribute cur_freq_attr    = __ATTR_RO(cur_freq);

static struct attribute *cluster_attrs[] = {
    &max_freq_attr.attr,
    &min_freq_attr.attr,
    &enabled_attr.attr,
    &cpuinfo_max_attr.attr,
    &cpuinfo_min_attr.attr,
    &current_max_attr.attr,
    &current_min_attr.attr,
    &cur_freq_attr.attr,
    NULL,
};

static struct attribute_group cluster_attr_group = {
    .attrs = cluster_attrs,
};

/* ---------------------------------------------------------- kobject type */

static void venus_cluster_release(struct kobject *kobj)
{
    /* 由 module_exit 统一清理，此处无需动作 */
}

static struct kobj_type cluster_ktype = {
    .release = venus_cluster_release,
    .sysfs_ops = &kobj_sysfs_ops,
};

/* ------------------------------------------------------------ probe/remove */

static int venus_freq_probe_cluster(struct venus_cluster *cl,
                                    int cluster_id, int first_cpu)
{
    struct cpufreq_policy *policy;
    int ret;

    cl->cluster_id = cluster_id;
    cl->first_cpu = first_cpu;
    cl->target_max = 0;
    cl->target_min = 0;
    cl->enabled = false;
    mutex_init(&cl->lock);

    policy = cpufreq_cpu_get(first_cpu);
    if (!policy) {
        /*
         * Prime cluster (cpu7) is routinely offline on Venus - the scheduler
         * parks it to save power. A missing policy is therefore a normal
         * runtime state, not a hard failure: skip this cluster and let the
         * other two register. venusd can bring cpu7 online and reload the
         * module if it wants Prime control back.
         */
        pr_info("venus_freq: cluster%d (cpu%d) offline, skipped\n",
                cluster_id, first_cpu);
        return -ENODEV;
    }
    cl->policy = policy;

    ret = kobject_init_and_add(&cl->kobj, &cluster_ktype, venus_kobj,
                               "policy%d", first_cpu);
    if (ret) {
        pr_err("venus_freq: kobject add failed for policy%d: %d\n", first_cpu, ret);
        cpufreq_cpu_put(policy);
        cl->policy = NULL;
        return ret;
    }

    ret = sysfs_create_group(&cl->kobj, &cluster_attr_group);
    if (ret) {
        pr_err("venus_freq: sysfs group failed for policy%d: %d\n", first_cpu, ret);
        kobject_put(&cl->kobj);
        cpufreq_cpu_put(policy);
        cl->policy = NULL;
        return ret;
    }

    pr_info("venus_freq: cluster%d (cpu%d) hw min=%u max=%u\n",
            cluster_id, first_cpu,
            policy->cpuinfo.min_freq, policy->cpuinfo.max_freq);

    return 0;
}

static void venus_freq_remove_cluster(struct venus_cluster *cl)
{
    if (!cl->policy)
        return;

    mutex_lock(&cl->lock);
    if (cl->enabled && cl->qos_max.qos) {
        freq_qos_remove_request(&cl->qos_min);
        freq_qos_remove_request(&cl->qos_max);
        cl->enabled = false;
    }
    mutex_unlock(&cl->lock);

    sysfs_remove_group(&cl->kobj, &cluster_attr_group);
    kobject_put(&cl->kobj);
    cpufreq_cpu_put(cl->policy);
    cl->policy = NULL;
}

/* ---------------------------------------------------------- module init/exit */

static const int venus_first_cpus[VENUS_MAX_CLUSTERS] = {0, 4, 7};

static int __init venus_freq_init(void)
{
    int i, ret;
    int registered = 0;
    int failed_offline = 0;

    pr_info("venus_freq: init v%s\n", VENUS_FREQ_VERSION);

    venus_kobj = kobject_create_and_add("venus_freq", kernel_kobj);
    if (!venus_kobj) {
        pr_err("venus_freq: root kobject failed\n");
        return -ENOMEM;
    }

    registered = 0;
    for (i = 0; i < VENUS_MAX_CLUSTERS; i++) {
        ret = venus_freq_probe_cluster(&g_clusters[i], i, venus_first_cpus[i]);
        if (!ret) {
            registered++;
            continue;
        }
        /*
         * -ENODEV means the CPU is offline so there is no policy to attach to.
         * That is expected on Venus (cpu7 sleeps most of the time) and must not
         * take the whole module down: keep the clusters that did register.
         * Any other error is a real problem and still rolls everything back.
         */
        if (ret == -ENODEV) {
            failed_offline++;
            continue;
        }
        pr_err("venus_freq: cluster%d probe failed (%d), rolling back\n", i, ret);
        goto err_rollback;
    }

    if (registered == 0) {
        pr_err("venus_freq: no cluster could be registered, aborting\n");
        kobject_put(venus_kobj);
        venus_kobj = NULL;
        return -ENODEV;
    }

    pr_info("venus_freq: %d/%d clusters registered (%d offline)\n",
            registered, VENUS_MAX_CLUSTERS, failed_offline);
    return 0;

err_rollback:
    while (--i >= 0)
        venus_freq_remove_cluster(&g_clusters[i]);
    kobject_put(venus_kobj);
    venus_kobj = NULL;
    return ret;
}

static void __exit venus_freq_exit(void)
{
    int i;

    pr_info("venus_freq: exit\n");
    for (i = 0; i < VENUS_MAX_CLUSTERS; i++)
        venus_freq_remove_cluster(&g_clusters[i]);
    if (venus_kobj) {
        kobject_put(venus_kobj);
        venus_kobj = NULL;
    }
}

module_init(venus_freq_init);
module_exit(venus_freq_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Venus Sched Team");
MODULE_DESCRIPTION("Venus Frequency QoS Control for Xiaomi Venus (SM8350)");
MODULE_VERSION(VENUS_FREQ_VERSION);
