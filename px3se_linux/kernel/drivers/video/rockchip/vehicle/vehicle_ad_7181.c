#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/sysctl.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <media/v4l2-chip-ident.h>
#include <linux/videodev2.h>
#include "vehicle_cfg.h"
#include "vehicle_main.h"
#include "vehicle_ad.h"
#include "vehicle_ad_7181.h"

enum {
	FORCE_PAL_WIDTH = 720,
	FORCE_PAL_HEIGHT = 576,
	FORCE_NTSC_WIDTH = 720,
	FORCE_NTSC_HEIGHT = 480,
	FORCE_CIF_OUTPUT_FORMAT = CIF_OUTPUT_FORMAT_422,
};

static v4l2_std_id std_old = V4L2_STD_NTSC;

#define SENSOR_REGISTER_LEN	1	/* sensor register address bytes*/
#define SENSOR_VALUE_LEN	1	/* sensor register value bytes*/

#define ADV7181_SENSOR_BUS_PARAM	(V4L2_MBUS_MASTER |     \
                    V4L2_MBUS_PCLK_SAMPLE_RISING |  \
                    V4L2_MBUS_HSYNC_ACTIVE_HIGH |   \
                    V4L2_MBUS_VSYNC_ACTIVE_HIGH |   \
                    V4L2_MBUS_DATA_ACTIVE_HIGH |    \
                    SOCAM_MCLK_24MHZ)
struct rk_sensor_reg {
	unsigned int reg;
	unsigned int val;
};

#define ADV7181_STATUS1_REG		0x10
#define ADV7181_STATUS1_IN_LOCK		0x01
#define ADV7181_STATUS1_AUTOD_MASK	0x70
#define ADV7181_STATUS1_AUTOD_NTSM_M_J	0x00
#define ADV7181_STATUS1_AUTOD_NTSC_4_43 0x10
#define ADV7181_STATUS1_AUTOD_PAL_M	0x20
#define ADV7181_STATUS1_AUTOD_PAL_60	0x30
#define ADV7181_STATUS1_AUTOD_PAL_B_G	0x40
#define ADV7181_STATUS1_AUTOD_SECAM	0x50
#define ADV7181_STATUS1_AUTOD_PAL_COMB	0x60
#define ADV7181_STATUS1_AUTOD_SECAM_525	0x70

#define ADV7181_INPUT_CONTROL		0x00
#define ADV7181_INPUT_DEFAULT		0x00
#define ADV7181_INPUT_CVBS_AIN2		0x00
#define ADV7181_INPUT_CVBS_AIN3		0x01
#define ADV7181_INPUT_CVBS_AIN5		0x02
#define ADV7181_INPUT_CVBS_AIN6		0x03
#define ADV7181_INPUT_CVBS_AIN8		0x04
#define ADV7181_INPUT_CVBS_AIN10	0x05
#define ADV7181_INPUT_CVBS_AIN1		0x0B
#define ADV7181_INPUT_CVBS_AIN4		0x0D
#define ADV7181_INPUT_CVBS_AIN7		0x0F

#define SEQCMD_END  0xFF000000
#define SensorEnd   {SEQCMD_END, 0x00}

#define SENSOR_DG DBG
#define SENSOR_TR DBG

/* Preview resolution setting*/
static struct rk_sensor_reg sensor_preview_data[] = {
	/* autodetect cvbs in ntsc/pal/secam 8-bit 422 encode */
	{0x00, 0x0B}, /*cvbs in AIN1*/
	{0x04, 0x77},
	{0x17, 0x41},
	{0x1D, 0x47},
	{0x31, 0x02},
	{0x3A, 0x17},
	{0x3B, 0x81},
	{0x3D, 0xA2},
	{0x3E, 0x6A},
	{0x3F, 0xA0},
	{0x86, 0x0B},
	{0xF3, 0x01},
	{0xF9, 0x03},
	{0x0E, 0x80},
	{0x52, 0x46},
	{0x54, 0x80},
	{0x7F, 0xFF},
	{0x81, 0x30},
	{0x90, 0xC9},
	{0x91, 0x40},
	{0x92, 0x3C},
	{0x93, 0xCA},
	{0x94, 0xD5},
	{0xB1, 0xFF},
	{0xB6, 0x08},
	{0xC0, 0x9A},
	{0xCF, 0x50},
	{0xD0, 0x4E},
	{0xD1, 0xB9},
	{0xD6, 0xDD},
	{0xD7, 0xE2},
	{0xE5, 0x51},
	{0xF6, 0x3B},
	{0x0E, 0x00},
	{0x03, 0x0C},
	SensorEnd
};

static v4l2_std_id adv7181_std_to_v4l2(u8 status1)
{
	/* in case V4L2_IN_ST_NO_SIGNAL */
	if (!(status1 & ADV7181_STATUS1_IN_LOCK))
		return V4L2_STD_UNKNOWN;

	switch (status1 & ADV7181_STATUS1_AUTOD_MASK) {
	case ADV7181_STATUS1_AUTOD_NTSM_M_J:
		return V4L2_STD_NTSC;
	case ADV7181_STATUS1_AUTOD_NTSC_4_43:
		return V4L2_STD_NTSC_443;
	case ADV7181_STATUS1_AUTOD_PAL_M:
		return V4L2_STD_PAL_M;
	case ADV7181_STATUS1_AUTOD_PAL_60:
		return V4L2_STD_PAL_60;
	case ADV7181_STATUS1_AUTOD_PAL_B_G:
		return V4L2_STD_PAL;
	case ADV7181_STATUS1_AUTOD_SECAM:
		return V4L2_STD_SECAM;
	case ADV7181_STATUS1_AUTOD_PAL_COMB:
		return V4L2_STD_PAL_Nc | V4L2_STD_PAL_N;
	case ADV7181_STATUS1_AUTOD_SECAM_525:
		return V4L2_STD_SECAM;
	default:
		return V4L2_STD_UNKNOWN;
	}
}

static u32 adv7181_status_to_v4l2(u8 status1)
{
	if (!(status1 & ADV7181_STATUS1_IN_LOCK))
		return V4L2_IN_ST_NO_SIGNAL;

	return 0;
}

static int adv7181_vehicle_status(struct ad_dev *ad,
				  u32 *status,
				  v4l2_std_id *std)
{
	unsigned char status1 = 0;

	status1 = vehicle_generic_sensor_read(ad, ADV7181_STATUS1_REG);
	//DBG("ADV7181_STATUS1_REG: %x\n", status1);
	if (status1 < 0)
		return status1;

	if (status)
		*status = adv7181_status_to_v4l2(status1);

	if (std)
		*std = adv7181_std_to_v4l2(status1);

	return 0;
}

static void adv7181_reinit_parameter(struct ad_dev *ad, v4l2_std_id std)
{
	int i;

	ad->cfg.bus_param = ADV7181_SENSOR_BUS_PARAM;
	if (std == V4L2_STD_PAL) {
		ad->cfg.width = FORCE_PAL_WIDTH;
		ad->cfg.height = FORCE_PAL_HEIGHT;
		ad->cfg.start_x = 0;
		ad->cfg.start_y = 0;
		ad->cfg.input_format = CIF_INPUT_FORMAT_PAL;
		ad->cfg.output_format = FORCE_CIF_OUTPUT_FORMAT;
		ad->cfg.field_order = 0;
		ad->cfg.yuv_order = 0;
		ad->cfg.frame_rate = 25;
	} else {
		ad->cfg.width = FORCE_NTSC_WIDTH;
		ad->cfg.height = FORCE_NTSC_HEIGHT;
		ad->cfg.start_x = 0;
		ad->cfg.start_y = 0;
		ad->cfg.input_format = CIF_INPUT_FORMAT_NTSC;
		ad->cfg.output_format = FORCE_CIF_OUTPUT_FORMAT;
		ad->cfg.field_order = 0;
		ad->cfg.yuv_order = 0;
		ad->cfg.frame_rate = 30;
	}

	/*href:0,high;1,low*/
	if (ad->cfg.bus_param | V4L2_MBUS_HSYNC_ACTIVE_HIGH)
		ad->cfg.href = 0;
	else
		ad->cfg.href = 1;
	/*vsync:0,low;1,high*/
	if (ad->cfg.bus_param | V4L2_MBUS_VSYNC_ACTIVE_HIGH)
		ad->cfg.vsync = 1;
	else
		ad->cfg.vsync = 0;

	/* fix crop info from dts config */
	for (i = 0; i < 4; i++) {
		if ((ad->defrects[i].width == ad->cfg.width) &&
		    (ad->defrects[i].height == ad->cfg.height)) {
			ad->cfg.start_x = ad->defrects[i].crop_x;
			ad->cfg.start_y = ad->defrects[i].crop_y;
			ad->cfg.width = ad->defrects[i].crop_width;
			ad->cfg.height = ad->defrects[i].crop_height;
		}
	}

	DBG("size %dx%d, crop(%d,%d)\n",
	    ad->cfg.width, ad->cfg.height,
	    ad->cfg.start_x, ad->cfg.start_y);
}

static void adv7181_reg_init(struct ad_dev *ad, unsigned char cvstd)
{
	struct rk_sensor_reg *sensor;
	int i = 0;
	unsigned char val[2];

	sensor = sensor_preview_data;

	while ((sensor[i].reg != SEQCMD_END) && (sensor[i].reg != 0xFC000000)) {
		val[0] = sensor[i].val;
		vehicle_generic_sensor_write(ad, sensor[i].reg, val);
		i++;
	}
}

int adv7181_ad_get_cfg(struct vehicle_cfg **cfg)
{
	u32 status;

	if (!g_addev)
		return -1;

	adv7181_vehicle_status(g_addev, &status, NULL);
	if (status) /* No signal */
		g_addev->cfg.ad_ready = false;
	else
		g_addev->cfg.ad_ready = true;

	*cfg = &g_addev->cfg;

	return 0;
}

void adv7181_ad_check_cif_error(struct ad_dev *ad, int last_line)
{
	DBG("%s, last_line %d\n", __func__, last_line);
}

int adv7181_check_id(struct ad_dev *ad)
{
	int ret = 0;
	int val;

	val = vehicle_generic_sensor_read(ad, 0x11);
	DBG("%s read 0x11 --> 0x%02x\n", ad->ad_name, val);
	if (val != 0x20) {
		DBG("%s wrong camera ID, expected 0x20, detected 0x%02x\n",
		    ad->ad_name, val);
		ret = -EINVAL;
	}

	return ret;
}

static int adv7181_check_std(struct ad_dev *ad, v4l2_std_id *std)
{
	u32 status;
	static bool is_first = true;

	adv7181_vehicle_status(ad, &status, std);

	if (status && is_first) { /* No signal */
		mdelay(30);
		adv7181_vehicle_status(ad, &status, std);
		//DBG("status 0x%x\n", status);
	}

	//if (status)
		//*std = V4L2_STD_NTSC;

	return 0;
}

static void power_on(struct ad_dev *ad)
{
	/* gpio_direction_output(ad->power, ad->pwr_active); */

	if (gpio_is_valid(ad->powerdown)) {
		gpio_request(ad->powerdown, "ad_powerdown");
		gpio_direction_output(ad->powerdown, !ad->pwdn_active);
		/* gpio_set_value(ad->powerdown, !ad->pwdn_active); */
	}

	if (gpio_is_valid(ad->power)) {
		gpio_request(ad->power, "ad_power");
		gpio_direction_output(ad->power, ad->pwr_active);
		/* gpio_set_value(ad->power, ad->pwr_active); */
	}
}

static void power_off(struct ad_dev *ad)
{
	if (gpio_is_valid(ad->powerdown))
		gpio_free(ad->powerdown);

	if (gpio_is_valid(ad->power))
		gpio_free(ad->power);
}

static void adv7181_check_state_work(struct work_struct *work)
{
	struct ad_dev *ad;
	v4l2_std_id std;

	ad = g_addev;

	if (ad->cif_error_last_line > 0)
		ad->cif_error_last_line = 0;

	adv7181_check_std(ad, &std);

	if (std != std_old) {
		std_old = std;
		adv7181_reinit_parameter(ad, std);
		vehicle_ad_stat_change_notify();
	}

	queue_delayed_work(ad->state_check_work.state_check_wq,
			   &ad->state_check_work.work, msecs_to_jiffies(500));
}

int adv7181_ad_deinit(void)
{
	struct ad_dev *ad;

	ad = g_addev;

	if (!ad)
		return -1;

	if (ad->state_check_work.state_check_wq) {
		cancel_delayed_work_sync(&ad->state_check_work.work);
		flush_delayed_work(&ad->state_check_work.work);
		flush_workqueue(ad->state_check_work.state_check_wq);
		destroy_workqueue(ad->state_check_work.state_check_wq);
	}
	if (ad->irq)
		free_irq(ad->irq, ad);
	power_off(ad);
	return 0;
}

int adv7181_ad_init(struct ad_dev *ad)
{
	int ret;
	v4l2_std_id std;

	if (!ad)
		return -1;

	g_addev = ad;

	/*  1. i2c init */
	ad->adapter = i2c_get_adapter(ad->i2c_chl);
	if (ad->adapter == NULL)
		return -1;

	if (!i2c_check_functionality(ad->adapter, I2C_FUNC_I2C))
		return -1;

	/*  2. ad power on sequence */
	power_on(ad);

	/* fix mode */
	adv7181_check_std(ad, &std);
	std_old = std;
	DBG("std: %s\n", (std == V4L2_STD_NTSC) ? "ntsc" : "pal");

	/*  3 .init default format params */
	adv7181_reg_init(ad, std);
	adv7181_reinit_parameter(ad, std);
	vehicle_ad_stat_change_notify();

	/*  4. ad register signal detect irq */
	if (0) {
		ad->irq = gpio_to_irq(ad->cvstd);
		ret = request_irq(ad->irq, NULL, IRQF_TRIGGER_FALLING,
				  "vehicle ad_adv7181", ad);
	}

	/*  5. create workqueue to detect signal change */
	INIT_DELAYED_WORK(&ad->state_check_work.work, adv7181_check_state_work);
	ad->state_check_work.state_check_wq =
		create_singlethread_workqueue("vehicle-ad-adv7181");

	queue_delayed_work(ad->state_check_work.state_check_wq,
			   &ad->state_check_work.work, msecs_to_jiffies(100));

	return 0;
}


