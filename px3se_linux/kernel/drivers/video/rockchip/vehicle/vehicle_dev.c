#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/bitops.h>
#include <linux/moduleparam.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include "vehicle_main.h"
#include "vehicle_cfg.h"

int vechile_open(struct inode *inode, struct file *file)
{
	return 0;
}

int vechile_close(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t vechile_write(struct file *filp, const char __user *buf,
			     size_t size, loff_t *ppos) {
	int data = 0, ret = 0;

	if (strncmp(buf, "88", size < 2 ? size : 2) == 0) {
		vehicle_exit_notify();
		pr_err("android already up, request vehicle exit\n");
		return size;
	}

	ret = copy_from_user(&data, buf, sizeof(int));
	if (ret)
		return -1;

	return 1;
}

static ssize_t
vechile_read(struct file *file, char __user *buf, size_t size, loff_t *ppos) {
	return 1;
}

static const struct file_operations vechile_fops = {
	.owner      = THIS_MODULE,
	/*.compat_ioctl      = vechile_ioctl,*/
	.open       = vechile_open,
	.release    = vechile_close,
	.write  = vechile_write,
	.read = vechile_read,
};

static struct miscdevice vechile_dev = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "vehicle",
	.fops		= &vechile_fops,
};

static int __init vechile_module_init(void)
{
	int ret = 0;
	/*     unsigned char i2c_addr; */

	/* register misc device*/
	ret = misc_register(&vechile_dev);
	if (ret) {
		DBG("ERROR: could not register vechile dev\n");
		return ret;
	}

	return 0;
}

static void __exit vechile_module_exit(void)
{
	misc_deregister(&vechile_dev);
}

module_init(vechile_module_init);
module_exit(vechile_module_exit);

#ifdef MODULE
#include <linux/compile.h>
#endif
MODULE_LICENSE("GPL");

