#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/types.h>

static struct kobject *local_kobj = NULL;
static struct timespec64 starttimer = {0, 0};
static struct timespec64 stoptimer = {0, 0};
static ktime_t start_time = 0;
static ktime_t mod_time = 0;
static ktime_t stamped_time = 0;

static ssize_t testsysfs_start_time_show(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	const struct rtc_time rtm = rtc_ktime_to_tm(start_time);

	return sysfs_emit(buf, "%ptRs UTC\n", &rtm);
}

static struct device_attribute testsysfs_start_time_dev_attr = {
	.attr = {
		.name = "start_time",
		.mode = S_IRUGO,
	},
	.show = testsysfs_start_time_show,
};

static ssize_t testsysfs_mod_time_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (mod_time) {
		const struct rtc_time rtm = rtc_ktime_to_tm(mod_time);

		return sysfs_emit(buf, "%ptRs UTC\n", &rtm);
	}

	return sysfs_emit(buf, "N/A\n");
}

static struct device_attribute testsysfs_mod_time_dev_attr = {
	.attr = {
		.name = "mod_time",
		.mode = S_IRUGO,
	},
	.show = testsysfs_mod_time_show,
};

static ssize_t testsysfs_stamped_time_show(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	if (stamped_time) {
		const struct rtc_time rtm = rtc_ktime_to_tm(stamped_time);

		return sysfs_emit(buf, "%ptRs UTC\n", &rtm);
	}

	return sysfs_emit(buf, "N/A\n");
}

static ssize_t testsysfs_stamped_time_store(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t count)
{
	mod_time = stamped_time = ktime_get_real();

	return 1;
}

static struct device_attribute testsysfs_stamped_time_dev_attr = {
        .attr = {
                .name = "stamped_time",
                .mode = S_IWUSR | S_IRUGO,
        },
        .show = testsysfs_stamped_time_show,
        .store = testsysfs_stamped_time_store,
};

static ssize_t testsysfs_starttimer_show(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	if (starttimer.tv_sec || starttimer.tv_nsec)
		return sysfs_emit(buf, "%020lld.%010ld\n", starttimer.tv_sec, starttimer.tv_nsec);

	return sysfs_emit(buf, "N/A\n");
}

static ssize_t testsysfs_starttimer_store(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t count)
{
	ktime_get_boottime_ts64(&starttimer);
	mod_time = ktime_get_real();

	return 1;
}

static struct device_attribute testsysfs_starttimer_dev_attr = {
        .attr = {
                .name = "starttimer",
                .mode = S_IWUSR | S_IRUGO,
        },
        .show = testsysfs_starttimer_show,
        .store = testsysfs_starttimer_store,
};

static ssize_t testsysfs_stoptimer_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	if (stoptimer.tv_sec || stoptimer.tv_nsec)
		return sysfs_emit(buf, "%020lld.%010ld\n", stoptimer.tv_sec, stoptimer.tv_nsec);

	return sysfs_emit(buf, "N/A\n");
}

static ssize_t testsysfs_stoptimer_store(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t count)
{
	ktime_get_boottime_ts64(&stoptimer);
	mod_time = ktime_get_real();

	return 1;
}

static struct device_attribute testsysfs_stoptimer_dev_attr = {
        .attr = {
                .name = "stoptimer",
                .mode = S_IWUSR | S_IRUGO,
        },
        .show = testsysfs_stoptimer_show,
        .store = testsysfs_stoptimer_store,
};

static ssize_t testsysfs_difftimer_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct timespec64 diff = timespec64_sub(stoptimer, starttimer);

	if (diff.tv_sec >= 0 && diff.tv_nsec >= 0)
		return sysfs_emit(buf, "%020lld.%010ld\n", diff.tv_sec, diff.tv_nsec);

	return sysfs_emit(buf, "N/A\n");
}

static struct device_attribute testsysfs_difftimer_dev_attr = {
        .attr = {
                .name = "difftimer",
                .mode = S_IRUGO,
        },
        .show = testsysfs_difftimer_show,
};

static const struct attribute *testsysfs_attrs[] = {
	&testsysfs_start_time_dev_attr.attr,
	&testsysfs_mod_time_dev_attr.attr,
	&testsysfs_stamped_time_dev_attr.attr,
	&testsysfs_starttimer_dev_attr.attr,
	&testsysfs_stoptimer_dev_attr.attr,
	&testsysfs_difftimer_dev_attr.attr,
	NULL,
};

static int __init testsysfs_init(void)
{
	int err = 0;

	start_time = ktime_get_real();

	local_kobj = kobject_create_and_add("testsysfs", kernel_kobj);
	if (!local_kobj) {
		pr_err("unable to create kobject\n");
		return -ENOMEM;
	}

	err = sysfs_create_files(local_kobj, testsysfs_attrs);
	if (err) {
		pr_err("failed to create sysfs entry (%d)\n", err);
		goto kobj_fail;
	}

	return 0;

kobj_fail:
	kobject_put(local_kobj);

	return err;
}
module_init(testsysfs_init);

static void __exit testsysfs_exit(void)
{
	sysfs_remove_files(local_kobj, testsysfs_attrs);
	kobject_put(local_kobj);
}
module_exit(testsysfs_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Wilken Gottwalt");
MODULE_DESCRIPTION("sysfs test driver");
