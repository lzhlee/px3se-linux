/*
 * Gadget Driver for CarPlay
 *
 * Copyright (C) 2017 zjinnova, Inc.
 * Author: guwenbiao <guwenbiao@zjinnova.com>
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/platform_device.h>

#include <linux/usb/ch9.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>

#include "gadget_chips.h"

#include "u_ether.c"

#include "f_fs.c"
#include "f_ncm.c"
#include "f_iap.c"

MODULE_AUTHOR("guwenbiao");
MODULE_DESCRIPTION("CarPlay Composite USB Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

static const char longname[] = "Gadget CarPlay";

/* Default vendor and product IDs, overridden by userspace */
#define VENDOR_ID		0x2207
#define PRODUCT_ID		0x2910

/* f_midi configuration */
#define MIDI_INPUT_PORTS    1
#define MIDI_OUTPUT_PORTS   1
#define MIDI_BUFFER_SIZE    256
#define MIDI_QUEUE_LENGTH   32

enum{
#ifdef CONFIG_CARPLAY_ANDROID
	ZJ_FALSE = 2,
	ZJ_TRUE = 3,
#else
	ZJ_FALSE = 0,
	ZJ_TRUE = 1,
#endif
};

struct carplay_usb_function {
	char *name;
	void *config;

	struct device *dev;
	char *dev_name;
	struct device_attribute **attributes;

	/* for carplay_dev.enabled_functions */
	struct list_head enabled_list;

	/* Optional: initialization during gadget bind */
	int (*init)(struct carplay_usb_function *, struct usb_composite_dev *);
	/* Optional: cleanup during gadget unbind */
	void (*cleanup)(struct carplay_usb_function *);
	/* Optional: called when the function is added the list of
	 *		enabled functions */
	void (*enable)(struct carplay_usb_function *);
	/* Optional: called when it is removed */
	void (*disable)(struct carplay_usb_function *);

	int (*bind_config)(struct carplay_usb_function *,
			   struct usb_configuration *);

	/* Optional: called when the configuration is removed */
	void (*unbind_config)(struct carplay_usb_function *,
			      struct usb_configuration *);
	/* Optional: handle ctrl requests before the device is configured */
	int (*ctrlrequest)(struct carplay_usb_function *,
			   struct usb_composite_dev *,
			   const struct usb_ctrlrequest *);
};

struct carplay_dev {
	struct carplay_usb_function **functions;
	struct list_head enabled_functions;
	struct usb_composite_dev *cdev;
	struct device *dev;

	void (*setup_complete)(struct usb_ep *ep,
			       struct usb_request *req);

	bool enabled;
	int disable_depth;
	struct mutex mutex;
	bool connected;
	bool sw_connected;
	struct work_struct work;
	char ffs_aliases[256];
};

static struct class *carplay_class;
static struct carplay_dev *_carplay_dev;
static int carplay_bind_config(struct usb_configuration *c);
static void carplay_unbind_config(struct usb_configuration *c);

/* string IDs are assigned dynamically */
#define STRING_MANUFACTURER_IDX		0
#define STRING_PRODUCT_IDX		1
#define STRING_SERIAL_IDX		2

static char manufacturer_string[256];
static char product_string[256];
static char serial_string[256];

/* String Table */
static struct usb_string strings_dev[] = {
	[STRING_MANUFACTURER_IDX].s = manufacturer_string,
	[STRING_PRODUCT_IDX].s = product_string,
	[STRING_SERIAL_IDX].s = serial_string,
	{  }			/* end of list */
};

static struct usb_gadget_strings stringtab_dev = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings_dev,
};

static struct usb_gadget_strings *dev_strings[] = {
	&stringtab_dev,
	NULL,
};

static struct usb_device_descriptor device_desc = {
	.bLength              = sizeof(device_desc),
	.bDescriptorType      = USB_DT_DEVICE,
	.bcdUSB               = __constant_cpu_to_le16(0x0200),
	.bDeviceClass         = USB_CLASS_PER_INTERFACE,
	.idVendor             = __constant_cpu_to_le16(VENDOR_ID),
	.idProduct            = __constant_cpu_to_le16(PRODUCT_ID),
	.bcdDevice            = __constant_cpu_to_le16(0xffff),
	.bNumConfigurations   = 1,
};

static struct usb_configuration carplay_config_driver = {
	.label		= "carplay",
	.unbind		= carplay_unbind_config,
	.bConfigurationValue = 1,
	.bmAttributes	= USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.MaxPower	= 1, /* 500ma */
};

static void carplay_work(struct work_struct *data)
{
	struct carplay_dev *dev = container_of(data, struct carplay_dev, work);
	struct usb_composite_dev *cdev = dev->cdev;
	char *disconnected[2] = { "USB_STATE=DISCONNECTED", NULL };
	char *connected[2]    = { "USB_STATE=CONNECTED", NULL };
	char *configured[2]   = { "USB_STATE=CONFIGURED", NULL };
	char **uevent_envp = NULL;
	unsigned long flags;

	spin_lock_irqsave(&cdev->lock, flags);
	if (cdev->config)
		uevent_envp = configured;
	else if (dev->connected != dev->sw_connected)
		uevent_envp = dev->connected ? connected : disconnected;
	dev->sw_connected = dev->connected;
	spin_unlock_irqrestore(&cdev->lock, flags);

	if (uevent_envp) {
		kobject_uevent_env(&dev->dev->kobj, KOBJ_CHANGE, uevent_envp);
		pr_info("%s: sent uevent %s\n", __func__, uevent_envp[0]);
	} else {
		pr_info("%s: did not send uevent (%d %d %p)\n", __func__,
			dev->connected, dev->sw_connected, cdev->config);
	}
}

static void carplay_enable(struct carplay_dev *dev)
{
	struct usb_composite_dev *cdev = dev->cdev;

	if (WARN_ON(!dev->disable_depth))
		return;

	if (--dev->disable_depth == 0) {
		usb_add_config(cdev, &carplay_config_driver,
			       carplay_bind_config);
		usb_gadget_connect(cdev->gadget);
	}
}

static void carplay_disable(struct carplay_dev *dev)
{
	struct usb_composite_dev *cdev = dev->cdev;

	if (dev->disable_depth++ == 0) {
		usb_gadget_disconnect(cdev->gadget);
		/* Cancel pending control requests */
		usb_ep_dequeue(cdev->gadget->ep0, cdev->req);
		usb_remove_config(cdev, &carplay_config_driver);
	}
}

/*-------------------------------------------------------------------------*/
/* Supported functions initialization */

struct functionfs_config {
	bool opened;
	bool enabled;
	struct ffs_data *data;
};

static int ffs_function_init(struct carplay_usb_function *f,
			     struct usb_composite_dev *cdev)
{
	f->config = kzalloc(sizeof(struct functionfs_config), GFP_KERNEL);
	if (!f->config)
		return -ENOMEM;

	return functionfs_init();
}

static void ffs_function_cleanup(struct carplay_usb_function *f)
{
	functionfs_cleanup();
	kfree(f->config);
}

static void ffs_function_enable(struct carplay_usb_function *f)
{
	struct carplay_dev *dev = _carplay_dev;
	struct functionfs_config *config = f->config;

	config->enabled = true;

	/* Disable the gadget until the function is ready */
	if (!config->opened)
		carplay_disable(dev);
}

static void ffs_function_disable(struct carplay_usb_function *f)
{
	struct carplay_dev *dev = _carplay_dev;
	struct functionfs_config *config = f->config;

	config->enabled = false;

	/* Balance the disable that was called in closed_callback */
	if (!config->opened)
		carplay_enable(dev);
}

static int ffs_function_bind_config(struct carplay_usb_function *f,
				    struct usb_configuration *c)
{
	struct functionfs_config *config = f->config;

	return functionfs_bind_config(c->cdev, c, config->data);
}

static ssize_t
ffs_aliases_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
	struct carplay_dev *dev = _carplay_dev;
	int ret;

	mutex_lock(&dev->mutex);
	ret = sprintf(buf, "%s\n", dev->ffs_aliases);
	mutex_unlock(&dev->mutex);

	return ret;
}

static ssize_t
ffs_aliases_store(struct device *pdev, struct device_attribute *attr,
		  const char *buf, size_t size)
{
	struct carplay_dev *dev = _carplay_dev;
	char buff[256];

	mutex_lock(&dev->mutex);

	if (dev->enabled) {
		mutex_unlock(&dev->mutex);
		return -EBUSY;
	}

	strlcpy(buff, buf, sizeof(buff));
	strlcpy(dev->ffs_aliases, strim(buff), sizeof(dev->ffs_aliases));

	mutex_unlock(&dev->mutex);

	return size;
}

static DEVICE_ATTR(aliases, S_IRUGO | S_IWUSR, ffs_aliases_show,
					       ffs_aliases_store);
static struct device_attribute *ffs_function_attributes[] = {
	&dev_attr_aliases,
	NULL
};

static struct carplay_usb_function ffs_function = {
#ifdef CONFIG_CARPLAY_ANDROID
	.name		= "Zadb",
#else
	.name		= "adb",
#endif
	.init		= ffs_function_init,
	.enable		= ffs_function_enable,
	.disable	= ffs_function_disable,
	.cleanup	= ffs_function_cleanup,
	.bind_config	= ffs_function_bind_config,
	.attributes	= ffs_function_attributes,
};

static int functionfs_ready_callback(struct ffs_data *ffs)
{
	struct carplay_dev *dev = _carplay_dev;
	struct functionfs_config *config = ffs_function.config;
	int ret = 0;

	mutex_lock(&dev->mutex);

	ret = functionfs_bind(ffs, dev->cdev);
	if (ret)
		goto err;

	config->data = ffs;
	config->opened = true;

	if (config->enabled)
		carplay_enable(dev);

err:
	mutex_unlock(&dev->mutex);
	return ret;
}

static void functionfs_closed_callback(struct ffs_data *ffs)
{
	struct carplay_dev *dev = _carplay_dev;
	struct functionfs_config *config = ffs_function.config;

	mutex_lock(&dev->mutex);

	if (config->enabled)
		carplay_disable(dev);

	config->opened = false;
	config->data = NULL;

	if (!WARN_ON(!ffs->gadget))
		dev->cdev->next_string_id -= ffs->strings_count;

	functionfs_unbind(ffs);

	mutex_unlock(&dev->mutex);
}

static void *functionfs_acquire_dev_callback(const char *dev_name)
{
	return 0;
}

static void functionfs_release_dev_callback(struct ffs_data *ffs_data)
{
}

static int iap_function_init(struct carplay_usb_function *f,
			     struct usb_composite_dev *c)
{
	return iAP_setup();
}

static void iap_function_cleanup(struct carplay_usb_function *f)
{
	iAP_cleanup();
}


static int iap_function_bind_config(struct carplay_usb_function *f,
				    struct usb_configuration *c)
{
	int status;

	printk("iap_function_bind_config\n");
	status = iAP_bind_config(c);
	if (status)
		printk(KERN_DEBUG "could not bind iAP config\n");

	return 0;
}

static void iap_function_unbind_config(struct carplay_usb_function *f,
				       struct usb_configuration *c)
{
}


static struct carplay_usb_function iap_function = {
	.name		= "iap",
	.init		= iap_function_init,
	.cleanup	= iap_function_cleanup,
	.bind_config	= iap_function_bind_config,
	.unbind_config	= iap_function_unbind_config,
};

/* ncm */
struct ncm_function_config {
	u8      ethaddr[ETH_ALEN];
	struct eth_dev *dev;
};

static int ncm_function_init(struct carplay_usb_function *f, struct usb_composite_dev *c)
{
	f->config = (void *)kzalloc(sizeof(struct ncm_function_config), GFP_KERNEL);
	if (!f->config)
		return -ENOMEM;

	return 0;
}

static void ncm_function_cleanup(struct carplay_usb_function *f)
{
	kfree(f->config);
	f->config = NULL;
}

static int ncm_function_bind_config(struct carplay_usb_function *f,
				    struct usb_configuration *c)
{
	struct ncm_function_config *ncm = f->config;
	int ret;

	if (!ncm) {
		pr_err("%s: ncm config is null\n", __func__);
		return -EINVAL;
	}

	pr_info("%s MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", __func__,
		ncm->ethaddr[0], ncm->ethaddr[1], ncm->ethaddr[2],
		ncm->ethaddr[3], ncm->ethaddr[4], ncm->ethaddr[5]);

	ncm->dev = gether_setup_name(c->cdev->gadget, ncm->ethaddr, "usb");
	if (IS_ERR(ncm->dev))
		return PTR_ERR(ncm->dev);

	ret = ncm_bind_config(c, ncm->ethaddr, ncm->dev);
	if (ret) {
		pr_err("%s: ncm bind config failed err:%d", __func__, ret);
		gether_cleanup(ncm->dev);
		return ret;
	}

	return 0;
}

static void ncm_function_unbind_config(struct carplay_usb_function *f,
				       struct usb_configuration *c)
{
	struct ncm_function_config *ncm = f->config;

	gether_cleanup(ncm->dev);
}

static ssize_t ncm_ethaddr_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct carplay_usb_function *f = dev_get_drvdata(dev);
	struct ncm_function_config *ncm = f->config;

	return snprintf(buf, PAGE_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x\n",
		ncm->ethaddr[0], ncm->ethaddr[1], ncm->ethaddr[2],
		ncm->ethaddr[3], ncm->ethaddr[4], ncm->ethaddr[5]);
}

static ssize_t ncm_ethaddr_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct carplay_usb_function *f = dev_get_drvdata(dev);
	struct ncm_function_config *ncm = f->config;

	if (sscanf(buf, "%02x:%02x:%02x:%02x:%02x:%02x\n",
		   (int *)&ncm->ethaddr[0], (int *)&ncm->ethaddr[1],
		   (int *)&ncm->ethaddr[2], (int *)&ncm->ethaddr[3],
		   (int *)&ncm->ethaddr[4], (int *)&ncm->ethaddr[5]) == 6)
		return size;
	return -EINVAL;
}

static DEVICE_ATTR(ncm_ethaddr, S_IRUGO | S_IWUSR, ncm_ethaddr_show,
					       ncm_ethaddr_store);
static struct device_attribute *ncm_function_attributes[] = {
	&dev_attr_ncm_ethaddr,
	NULL
};
static struct carplay_usb_function ncm_function = {
	.name		= "ncm",
	.init		= ncm_function_init,
	.cleanup	= ncm_function_cleanup,
	.bind_config	= ncm_function_bind_config,
	.unbind_config	= ncm_function_unbind_config,
	.attributes	= ncm_function_attributes,
};

static struct carplay_usb_function *supported_functions[] = {
	&ffs_function,
	&iap_function,
	&ncm_function,
	NULL
};

static int carplay_init_functions(struct carplay_usb_function **functions,
				  struct usb_composite_dev *cdev)
{
	struct carplay_dev *dev = _carplay_dev;
	struct carplay_usb_function *f;
	struct device_attribute **attrs;
	struct device_attribute *attr;
	int err;
	int index = 0;

	for (; (f = *functions++); index++) {
		f->dev_name = kasprintf(GFP_KERNEL, "f_%s", f->name);
		f->dev = device_create(carplay_class, dev->dev,
				MKDEV(0, index), f, f->dev_name);
		if (IS_ERR(f->dev)) {
			pr_err("%s: Failed to create dev %s", __func__,
			       f->dev_name);
			err = PTR_ERR(f->dev);
			goto err_create;
		}

		if (f->init) {
			err = f->init(f, cdev);
			if (err) {
				pr_err("%s: Failed to init %s", __func__,
				       f->name);
				goto err_out;
			}
		}

		attrs = f->attributes;
		if (attrs) {
			while ((attr = *attrs++) && !err)
				err = device_create_file(f->dev, attr);
		}
		if (err) {
			pr_err("%s: Failed to create function %s attributes",
			       __func__, f->name);
			goto err_out;
		}
	}
	return 0;

err_out:
	device_destroy(carplay_class, f->dev->devt);
err_create:
	kfree(f->dev_name);
	return err;
}

static void carplay_cleanup_functions(struct carplay_usb_function **functions)
{
	struct carplay_usb_function *f;

	while (*functions) {
		f = *functions++;

		if (f->dev) {
			device_destroy(carplay_class, f->dev->devt);
			kfree(f->dev_name);
		}

		if (f->cleanup)
			f->cleanup(f);
	}
}

static int
carplay_bind_enabled_functions(struct carplay_dev *dev,
			       struct usb_configuration *c)
{
	struct carplay_usb_function *f;
	int ret;

	list_for_each_entry(f, &dev->enabled_functions, enabled_list) {
		ret = f->bind_config(f, c);
		if (ret) {
			pr_err("%s: %s failed", __func__, f->name);
			return ret;
		}
	}
	return 0;
}

static void
carplay_unbind_enabled_functions(struct carplay_dev *dev,
				 struct usb_configuration *c)
{
	struct carplay_usb_function *f;

	list_for_each_entry(f, &dev->enabled_functions, enabled_list) {
		if (f->unbind_config)
			f->unbind_config(f, c);
	}
}

static int carplay_enable_function(struct carplay_dev *dev, char *name)
{
	struct carplay_usb_function **functions = dev->functions;
	struct carplay_usb_function *f;

	while ((f = *functions++)) {
		if (!strcmp(name, f->name)) {
			list_add_tail(&f->enabled_list,
				      &dev->enabled_functions);
			return 0;
		}
	}
	return -EINVAL;
}

/*-------------------------------------------------------------------------*/
/* /sys/class/carplay/carplay%d/ interface */

static ssize_t
functions_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
	struct carplay_dev *dev = dev_get_drvdata(pdev);
	struct carplay_usb_function *f;
	char *buff = buf;

	mutex_lock(&dev->mutex);

	list_for_each_entry(f, &dev->enabled_functions, enabled_list)
		buff += sprintf(buff, "%s,", f->name);

	mutex_unlock(&dev->mutex);

	if (buff != buf)
		*(buff-1) = '\n';
	return buff - buf;
}

static ssize_t
functions_store(struct device *pdev, struct device_attribute *attr,
		const char *buff, size_t size)
{
	struct carplay_dev *dev = dev_get_drvdata(pdev);
	char *name;
	char buf[256], *b;
	char aliases[256], *a;
	int err;
	int is_ffs;
	int ffs_enabled = 0;

	mutex_lock(&dev->mutex);

	if (dev->enabled) {
		mutex_unlock(&dev->mutex);
		return -EBUSY;
	}

	INIT_LIST_HEAD(&dev->enabled_functions);

	strlcpy(buf, buff, sizeof(buf));
	b = strim(buf);

	while (b) {
		name = strsep(&b, ",");
		if (!name)
			continue;

		is_ffs = 0;
		strlcpy(aliases, dev->ffs_aliases, sizeof(aliases));
		a = aliases;

		while (a) {
			char *alias = strsep(&a, ",");

			if (alias && !strcmp(name, alias)) {
				/*is_ffs = 1;*/
				break;
			}
		}

		if (is_ffs) {
			if (ffs_enabled)
				continue;
			err = carplay_enable_function(dev, "ffs");
			if (err)
				pr_err("android_usb: Cannot enable ffs (%d)",
				       err);
			else
				ffs_enabled = 1;
			continue;
		}

		err = carplay_enable_function(dev, name);
		if (err)
			pr_err("android_usb: Cannot enable '%s' (%d)",
			       name, err);
	}

	mutex_unlock(&dev->mutex);

	return size;
}

static ssize_t enable_show(struct device *pdev, struct device_attribute *attr,
			   char *buf)
{
	struct carplay_dev *dev = dev_get_drvdata(pdev);

	return sprintf(buf, "%d\n", dev->enabled);
}

static ssize_t enable_store(struct device *pdev, struct device_attribute *attr,
			    const char *buff, size_t size)
{
	struct carplay_dev *dev = dev_get_drvdata(pdev);
	struct usb_composite_dev *cdev = dev->cdev;
	struct carplay_usb_function *f;
	int enabled = 0;

	if (!cdev)
		return -ENODEV;

	mutex_lock(&dev->mutex);

	sscanf(buff, "%d", &enabled);
	if (enabled == ZJ_TRUE && !dev->enabled) {
		/*
		 * Update values in composite driver's copy of
		 * device descriptor.
		 */
		cdev->desc.idVendor = device_desc.idVendor;
		cdev->desc.idProduct = device_desc.idProduct;
		cdev->desc.bcdDevice = device_desc.bcdDevice;
		cdev->desc.bDeviceClass = device_desc.bDeviceClass;
		cdev->desc.bDeviceSubClass = device_desc.bDeviceSubClass;
		cdev->desc.bDeviceProtocol = device_desc.bDeviceProtocol;
		list_for_each_entry(f, &dev->enabled_functions, enabled_list) {
			if (f->enable)
				f->enable(f);
		}
		carplay_enable(dev);
		dev->enabled = true;
	} else if (enabled == ZJ_FALSE && dev->enabled) {
		carplay_disable(dev);
		list_for_each_entry(f, &dev->enabled_functions, enabled_list) {
		if (f->disable)
			f->disable(f);
		}
		dev->enabled = false;
	} else {
		pr_err("android_usb: already %s\n",
		       dev->enabled ? "enabled" : "disabled");
	}

	mutex_unlock(&dev->mutex);
	return size;
}

static ssize_t state_show(struct device *pdev, struct device_attribute *attr,
			  char *buf)
{
	struct carplay_dev *dev = dev_get_drvdata(pdev);
	struct usb_composite_dev *cdev = dev->cdev;
	char *state = "DISCONNECTED";
	unsigned long flags;

	if (!cdev)
		goto out;

	spin_lock_irqsave(&cdev->lock, flags);
	if (cdev->config)
		state = "CONFIGURED";
	else if (dev->connected)
		state = "CONNECTED";
	spin_unlock_irqrestore(&cdev->lock, flags);
out:
	return sprintf(buf, "%s\n", state);
}

#define DESCRIPTOR_ATTR(field, format_string)				\
static ssize_t								\
field ## _show(struct device *dev, struct device_attribute *attr,	\
		char *buf)						\
{									\
	return sprintf(buf, format_string, device_desc.field);		\
}									\
static ssize_t								\
field ## _store(struct device *dev, struct device_attribute *attr,	\
		const char *buf, size_t size)				\
{									\
	int value;							\
	if (sscanf(buf, format_string, &value) == 1) {			\
		device_desc.field = value;				\
		return size;						\
	}								\
	return -1;							\
}									\
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, field ## _show, field ## _store);

#define DESCRIPTOR_STRING_ATTR(field, buffer)				\
static ssize_t								\
field ## _show(struct device *dev, struct device_attribute *attr,	\
		char *buf)						\
{									\
	return sprintf(buf, "%s", buffer);				\
}									\
static ssize_t								\
field ## _store(struct device *dev, struct device_attribute *attr,	\
		const char *buf, size_t size)				\
{									\
	if (size >= sizeof(buffer))					\
		return -EINVAL;						\
	return strlcpy(buffer, buf, sizeof(buffer));			\
}									\
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, field ## _show, field ## _store);


DESCRIPTOR_ATTR(idVendor, "%04x\n")
DESCRIPTOR_ATTR(idProduct, "%04x\n")
DESCRIPTOR_ATTR(bcdDevice, "%04x\n")
DESCRIPTOR_ATTR(bDeviceClass, "%d\n")
DESCRIPTOR_ATTR(bDeviceSubClass, "%d\n")
DESCRIPTOR_ATTR(bDeviceProtocol, "%d\n")
DESCRIPTOR_STRING_ATTR(iManufacturer, manufacturer_string)
DESCRIPTOR_STRING_ATTR(iProduct, product_string)
DESCRIPTOR_STRING_ATTR(iSerial, serial_string)

static DEVICE_ATTR(functions, S_IRUGO | S_IWUSR, functions_show,
						 functions_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, enable_show, enable_store);
static DEVICE_ATTR(state, S_IRUGO, state_show, NULL);

static struct device_attribute *carplay_usb_attributes[] = {
	&dev_attr_idVendor,
	&dev_attr_idProduct,
	&dev_attr_bcdDevice,
	&dev_attr_bDeviceClass,
	&dev_attr_bDeviceSubClass,
	&dev_attr_bDeviceProtocol,
	&dev_attr_iManufacturer,
	&dev_attr_iProduct,
	&dev_attr_iSerial,
	&dev_attr_functions,
	&dev_attr_enable,
	&dev_attr_state,
	NULL
};

/*-------------------------------------------------------------------------*/
/* Composite driver */

static int carplay_bind_config(struct usb_configuration *c)
{
	struct carplay_dev *dev = _carplay_dev;
	int ret = 0;

	ret = carplay_bind_enabled_functions(dev, c);
	if (ret)
		return ret;

	return 0;
}

static void carplay_unbind_config(struct usb_configuration *c)
{
	struct carplay_dev *dev = _carplay_dev;

	carplay_unbind_enabled_functions(dev, c);
}

static int carplay_bind(struct usb_composite_dev *cdev)
{
	struct carplay_dev *dev = _carplay_dev;
	struct usb_gadget	*gadget = cdev->gadget;
	int			id, ret;

	/* Save the default handler */
	dev->setup_complete = cdev->req->complete;

	/*
	 * Start disconnected. Userspace will connect the gadget once
	 * it is done configuring the functions.
	 */
	usb_gadget_disconnect(gadget);

	ret = carplay_init_functions(dev->functions, cdev);
	if (ret)
		return ret;

	/* Allocate string descriptor numbers ... note that string
	 * contents can be overridden by the composite_dev glue.
	 */
	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_MANUFACTURER_IDX].id = id;
	device_desc.iManufacturer = id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_PRODUCT_IDX].id = id;
	device_desc.iProduct = id;

	/* Default strings - should be updated by userspace */
	strncpy(manufacturer_string, "Carplay", sizeof(manufacturer_string)-1);
	strncpy(product_string, "Carplay", sizeof(product_string) - 1);
	strncpy(serial_string, "0123456789ABCDEF", sizeof(serial_string) - 1);

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_SERIAL_IDX].id = id;
	device_desc.iSerialNumber = id;
	device_desc.bcdDevice = cpu_to_le16(get_default_bcdDevice());

	usb_gadget_set_selfpowered(gadget);
	dev->cdev = cdev;

	return 0;
}

static int carplay_usb_unbind(struct usb_composite_dev *cdev)
{
	struct carplay_dev *dev = _carplay_dev;

	cancel_work_sync(&dev->work);
	carplay_cleanup_functions(dev->functions);
	return 0;
}

/* HACK: Carplay needs to override setup for accessory to work */
static int (*composite_setup_func)(struct usb_gadget *gadget, const struct usb_ctrlrequest *c);

static int
carplay_setup(struct usb_gadget *gadget, const struct usb_ctrlrequest *c)
{
	struct carplay_dev		*dev = _carplay_dev;
	struct usb_composite_dev	*cdev = get_gadget_data(gadget);
	struct usb_request		*req = cdev->req;
	struct carplay_usb_function	*f;
	int value = -EOPNOTSUPP;
	unsigned long flags;

	req->zero = 0;
	req->length = 0;
	req->complete = dev->setup_complete;
	gadget->ep0->driver_data = cdev;

	list_for_each_entry(f, &dev->enabled_functions, enabled_list) {
		if (f->ctrlrequest) {
			value = f->ctrlrequest(f, cdev, c);
			if (value >= 0)
				break;
		}
	}

	if (value < 0)
		value = composite_setup_func(gadget, c);

	spin_lock_irqsave(&cdev->lock, flags);
	if (!dev->connected) {
		dev->connected = 1;
		schedule_work(&dev->work);
	} else if (c->bRequest == USB_REQ_SET_CONFIGURATION &&
						cdev->config) {
		schedule_work(&dev->work);
	}
	spin_unlock_irqrestore(&cdev->lock, flags);

	return value;
}

static void carplay_disconnect(struct usb_composite_dev *cdev)
{
	struct carplay_dev *dev = _carplay_dev;

	dev->connected = 0;
	schedule_work(&dev->work);
}

static struct usb_composite_driver carplay_usb_driver = {
	.name		= "carplay_usb",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.bind		= carplay_bind,
	.unbind		= carplay_usb_unbind,
	.disconnect	= carplay_disconnect,
	.max_speed	= USB_SPEED_HIGH,
};

static int carplay_create_device(struct carplay_dev *dev)
{
	struct device_attribute **attrs = carplay_usb_attributes;
	struct device_attribute *attr;
	int err;

	dev->dev = device_create(carplay_class, NULL,
					MKDEV(0, 0), NULL, "android0");
	if (IS_ERR(dev->dev))
		return PTR_ERR(dev->dev);

	dev_set_drvdata(dev->dev, dev);

	while ((attr = *attrs++)) {
		err = device_create_file(dev->dev, attr);
		if (err) {
			device_destroy(carplay_class, dev->dev->devt);
			return err;
		}
	}
	return 0;
}


static int __init init(void)
{
	struct carplay_dev *dev;
	int err;

	carplay_class = class_create(THIS_MODULE, "android_usb");
	if (IS_ERR(carplay_class))
		return PTR_ERR(carplay_class);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		err = -ENOMEM;
		goto err_dev;
	}

	dev->disable_depth = 1;
	dev->functions = supported_functions;
	INIT_LIST_HEAD(&dev->enabled_functions);
	INIT_WORK(&dev->work, carplay_work);
	mutex_init(&dev->mutex);

	err = carplay_create_device(dev);
	if (err) {
		pr_err("%s: failed to create carplay device %d", __func__, err);
		goto err_create;
	}

	_carplay_dev = dev;

	err = usb_composite_probe(&carplay_usb_driver);
	if (err) {
		pr_err("%s: failed to probe driver %d", __func__, err);
		goto err_probe;
	}

	/* HACK: exchange composite's setup with ours */
	composite_setup_func = carplay_usb_driver.gadget_driver.setup;
	carplay_usb_driver.gadget_driver.setup = carplay_setup;

	return 0;

err_probe:
	device_destroy(carplay_class, dev->dev->devt);
err_create:
	kfree(dev);
err_dev:
	class_destroy(carplay_class);
	return err;
}
late_initcall(init);

static void __exit cleanup(void)
{
	usb_composite_unregister(&carplay_usb_driver);
	class_destroy(carplay_class);
	kfree(_carplay_dev);
	_carplay_dev = NULL;
}
module_exit(cleanup);
