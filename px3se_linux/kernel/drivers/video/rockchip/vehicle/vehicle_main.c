#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/fb.h>
#include <linux/rk_fb.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/completion.h>
#include <linux/wakelock.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include "vehicle_flinger.h"
#include "vehicle_cfg.h"
#include "vehicle_ad.h"
#include "vehicle_main.h"
#include "vehicle_cif.h"
#include "vehicle_gpio.h"

enum {
	STATE_CLOSE = 0,
	STATE_OPEN,
};
struct vehicle {
	struct device	*dev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct wake_lock wake_lock;
	struct gpio_detect gpio_data;
	struct vehicle_cif cif;
	struct ad_dev ad;
	int mirror;
	wait_queue_head_t vehicle_wait;
	atomic_t vehicle_atomic;
	int state;
	bool vehicle_need_exit;
};

struct vehicle *g_vehicle = NULL;

static int vehicle_parse_dt(struct vehicle *vehicle_info)
{
	struct device	*dev = vehicle_info->dev;

	DBG("%s\n", __func__);

	/*  1. pinctrl */
	vehicle_info->pinctrl = pinctrl_get(dev);
	if (IS_ERR(vehicle_info->pinctrl)) {
		dev_err(dev, "pinctrl get failed\n");
		return PTR_ERR(vehicle_info->pinctrl);
	}
	vehicle_info->pins_default = pinctrl_lookup_state(vehicle_info->pinctrl,
			"vehicle");
	if (IS_ERR(vehicle_info->pins_default))
		dev_err(dev, "get default pinstate failed\n");
	else
		pinctrl_select_state(vehicle_info->pinctrl,
				     vehicle_info->pins_default);
	return 0;
}

void vehicle_ad_stat_change_notify(void)
{
	if (g_vehicle)
		atomic_set(&g_vehicle->vehicle_atomic, 1);
}

void vehicle_gpio_stat_change_notify(void)
{
	if (g_vehicle)
		atomic_set(&g_vehicle->vehicle_atomic, 1);
}

void vehicle_cif_error_notify(int last_line)
{
	if (g_vehicle)
		ad_check_cif_error(&g_vehicle->ad, last_line);
}

static void vehicle_open(struct vehicle_cfg *v_cfg)
{
	vehicle_flinger_reverse_open(v_cfg);
	vehicle_cif_reverse_open(v_cfg);
}
static void vehicle_close(void)
{
	vehicle_cif_reverse_close();
	vehicle_flinger_reverse_close();
}
static int vehicle_state_change(struct vehicle *v)
{
	struct vehicle_cfg *v_cfg;
	struct gpio_detect *gpiod = &v->gpio_data;
	bool gpio_reverse_on;
	struct rk_fb_car_par *par;

	par = rk_fb_get_car_par();
	if(v->vehicle_need_exit)
		par->logo_showing = 0;
	gpio_reverse_on = vehicle_gpio_reverse_check(gpiod);
	v_cfg = ad_get_vehicle_cfg();

	if (!v_cfg)
		return -1;

	DBG("%s, gpio = reverse %s, ad width = %d, state=%d, ad ready %d\n", __func__,
	    gpio_reverse_on ? "on" : "over", v_cfg->width, v->state, v_cfg->ad_ready);
	switch (v->state) {
	case STATE_CLOSE:
		/*  reverse on & video in */
		if (gpio_reverse_on && v_cfg->ad_ready) {
			vehicle_open(v_cfg);
			v->state = STATE_OPEN;
		}
		break;
	case STATE_OPEN:
		/*  reverse exit || video loss */
		if (!gpio_reverse_on || !v_cfg->ad_ready) {
			vehicle_close();
			v->state = STATE_CLOSE;
		}
		/*  reverse on & video format change */
		if (gpio_reverse_on && v_cfg->ad_ready) {
			vehicle_open(v_cfg);
		}
		break;
	}
	return 0;
}

static int vehicle_probe(struct platform_device *pdev)
{
	struct vehicle *vehicle_info;

	vehicle_info = devm_kzalloc(&pdev->dev,
				    sizeof(struct vehicle), GFP_KERNEL);
	if (!vehicle_info)
		return -ENOMEM;

	vehicle_info->dev = &pdev->dev;
	vehicle_info->gpio_data.dev = &pdev->dev;
	vehicle_info->cif.dev = &pdev->dev;
	vehicle_info->ad.dev = &pdev->dev;

	dev_set_name(vehicle_info->dev, "vehicle_main");
	if (!pdev->dev.of_node)
		return -EINVAL;

	vehicle_parse_dt(vehicle_info);

	if (IS_ERR_VALUE(vehicle_parse_sensor(&vehicle_info->ad))) {
		DBG("parse sensor failed!\n");
		pinctrl_put(vehicle_info->pinctrl);
		return -EINVAL;
	}

	wake_lock_init(&vehicle_info->wake_lock, WAKE_LOCK_SUSPEND, "vehicle");

	dev_info(vehicle_info->dev, "vehicle driver probe success\n");

	init_waitqueue_head(&vehicle_info->vehicle_wait);
	atomic_set(&vehicle_info->vehicle_atomic, 0);
	vehicle_info->state = STATE_CLOSE;
	vehicle_info->vehicle_need_exit = false;

	g_vehicle = vehicle_info;
	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id vehicle_of_match[] = {
	{ .compatible = "vehicle", },
	{},
};
#endif

static struct platform_driver vehicle_driver = {
	.driver     = {
		.name   = "vehicle",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(vehicle_of_match),
	},
	.probe      = vehicle_probe,
};

void vehicle_exit_notify(void)
{
	if (g_vehicle)
		g_vehicle->vehicle_need_exit = true;
}
static void vehicle_exit_complete_notify(struct vehicle *v)
{
	char *status = NULL;
	char *envp[2];

	if (!v)
		return;
	status = kasprintf(GFP_KERNEL, "vehicle_exit=done");
	envp[0] = status;
	envp[1] = NULL;
	wake_lock_timeout(&v->wake_lock, 5 * HZ);
	kobject_uevent_env(&v->dev->kobj, KOBJ_CHANGE, envp);

	DBG("%s: uevent:Vehicle exit done\r\n", __func__);
	kfree(status);
}

extern int rk_camera_init(void);
extern int gpio_det_init(void);

int rk_vehicle_system_main(void *arg)
{
	int ret = -1;
	struct vehicle *v = g_vehicle;

	if (!g_vehicle) {
		DBG("vehicle probe failed, g_vehicle is NULL.\n");
		goto VEHICLE_EXIT;
	}

	/*  1.ad */
	ret = vehicle_ad_init(&v->ad);
	if (IS_ERR_VALUE(ret)) {
		DBG("rk_vehicle_system_main: ad init failed\r\n");
		goto VEHICLE_AD_DEINIT;
	}

	/*  2. flinger */
	ret = vehicle_flinger_init(v->dev);
	if (IS_ERR_VALUE(ret)) {
		DBG("rk_vehicle_system_main: flinger init failed\r\n");
		goto VEHICLE_FLINGER_DEINIT;
	}

	/*  3. cif init */
	ret = vehicle_cif_init(&v->cif);
	if (IS_ERR_VALUE(ret)) {
		DBG("rk_vehicle_system_main: cif init failed\r\n");
		goto VEHICLE_CIF_DEINIT;
	}

	/*  4. gpio init and check state */
	vehicle_gpio_init(&v->gpio_data);
	if (IS_ERR_VALUE(ret)) {
		DBG("rk_vehicle_system_main: gpio init failed\r\n");
		goto VEHICLE_GPIO_DEINIT;
	}

	DBG("yuyz %s init success, start loop\n", __func__);

	while (STATE_OPEN == v->state || !v->vehicle_need_exit) {
		wait_event_timeout(v->vehicle_wait,
				   atomic_read(&v->vehicle_atomic),
				   msecs_to_jiffies(100));
		if (atomic_read(&v->vehicle_atomic)) {
			atomic_set(&v->vehicle_atomic, 0);
			vehicle_state_change(v);
		}
	}

VEHICLE_GPIO_DEINIT:
	vehicle_gpio_deinit(&v->gpio_data);

VEHICLE_CIF_DEINIT:
	vehicle_cif_deinit(&v->cif);

VEHICLE_FLINGER_DEINIT:
	vehicle_flinger_deinit();

VEHICLE_AD_DEINIT:
	vehicle_ad_deinit();

	/*Init normal drivers*/
VEHICLE_EXIT:
	if (v->pinctrl)
		pinctrl_put(v->pinctrl);
	rk_camera_init();
	gpio_det_init();

	msleep(100);
	vehicle_exit_complete_notify(v);
	return 0;
}

static int __init vehicle_system_start(void)
{
	DBG("vehicle_system_start:start vehicle system \r\n");
	platform_driver_register(&vehicle_driver);
	kthread_run(rk_vehicle_system_main, NULL, "vehicle main");
	return 0;
}

/* rootfs_initcall(vehicle_system_start); */
/* rootfs_initcall(vehicle_system_start); */
subsys_initcall_sync(vehicle_system_start);
/* fs_initcall_sync(vehicle_system_start); */

