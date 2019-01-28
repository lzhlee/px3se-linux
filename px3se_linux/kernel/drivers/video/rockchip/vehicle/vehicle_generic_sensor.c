#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include "vehicle_ad.h"
#include "vehicle_ad_7181.h"
#include "vehicle_ad_tp2825.h"

struct ad_dev *g_addev = NULL;

struct vehicle_sensor_ops {
	const char *name;

	int (*sensor_init)(struct ad_dev *ad);
	int (*sensor_deinit)(void);
	int (*sensor_get_cfg)(struct vehicle_cfg **cfg);
	void (*sensor_check_cif_error)(struct ad_dev *ad, int last_line);
	int (*sensor_check_id_cb)(struct ad_dev *ad);
};
struct vehicle_sensor_ops *sensor_cb;

static struct vehicle_sensor_ops sensor_cb_series[] = {
	{
		.name = "adv7181",
		.sensor_init = adv7181_ad_init,
		.sensor_deinit = adv7181_ad_deinit,
		.sensor_get_cfg = adv7181_ad_get_cfg,
		.sensor_check_cif_error = adv7181_ad_check_cif_error,
		.sensor_check_id_cb = adv7181_check_id
	},
	{
		.name = "tp2825",
		.sensor_init = tp2825_ad_init,
		.sensor_deinit = tp2825_ad_deinit,
		.sensor_get_cfg = tp2825_ad_get_cfg,
		.sensor_check_cif_error = tp2825_ad_check_cif_error,
		.sensor_check_id_cb = tp2825_check_id
	}
};

int vehicle_generic_sensor_write(struct ad_dev *ad, char reg, char *pval)
{
	struct i2c_msg msg;
	int ret;

	char *tx_buf = kmalloc(2, GFP_KERNEL);

	if (!tx_buf)
		return -ENOMEM;

	memcpy(tx_buf, &reg, 1);
	memcpy(tx_buf+1, (char *)pval, 1);

	msg.addr = ad->i2c_add;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = (char *)tx_buf;
	msg.scl_rate = ad->i2c_rate;

	ret = i2c_transfer(ad->adapter, &msg, 1);
	kfree(tx_buf);

	return (ret == 1) ? 4 : ret;
}

int vehicle_generic_sensor_read(struct ad_dev *ad, char reg)
{
	struct i2c_msg msgs[2];
	int ret;
	char reg_buf[2];
	char pval;

	memcpy(reg_buf, &reg, 1);

	msgs[0].addr =	ad->i2c_add;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = reg_buf;
	msgs[0].scl_rate = ad->i2c_rate;

	msgs[1].addr = ad->i2c_add;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = &pval;
	msgs[1].scl_rate = ad->i2c_rate;

	ret = i2c_transfer(ad->adapter, msgs, 2);

	return pval;
}

int vehicle_parse_sensor(struct ad_dev *ad)
{
	struct device *dev = ad->dev;
	struct device_node *node = NULL;
	struct device_node *cp = NULL;
	enum of_gpio_flags flags;
	const char *status = NULL;
	int i;
	int ret = 0;

	if (of_property_read_u32(dev->of_node, "ad,fix-format",
				 &ad->fix_format))
		DBG("get fix-format failed!\n");

	if (of_property_read_u32(dev->of_node, "vehicle,rotate-mirror",
				 &ad->cfg.rotate_mirror))
		DBG("get rotate-mirror failed!\n");

	node = of_parse_phandle(dev->of_node, "rockchip,cif-sensor", 0);
	if (!node) {
		DBG("get cif-sensor dts failed\n");
		return -1;
	}

	for_each_child_of_node(node, cp) {
		of_property_read_string(cp, "status", &status);
		DBG("status: %s\n", status);
		if (status && strcmp(status, "okay"))
			continue;
		DBG("status: %s\n", status);

		if (of_property_read_u32(cp, "i2c_rata", &ad->i2c_rate))
			DBG("Get %s i2c_rata failed!\n", cp->name);
		if (of_property_read_u32(cp, "i2c_chl", &ad->i2c_chl))
			DBG("Get %s i2c_chl failed!\n", cp->name);
		if (of_property_read_u32(cp, "ad_chl", &ad->ad_chl))
			DBG("Get %s ad_chl failed!\n", cp->name);

		if (ad->ad_chl > 4 || ad->ad_chl < 0) {
			DBG("error, ad_chl %d !\n", ad->ad_chl);
			ad->ad_chl = 0;
		}
		if (of_property_read_u32(cp, "mclk_rate", &ad->mclk_rate))
			DBG("Get %s mclk_rate failed!\n", cp->name);

		if (of_property_read_u32(cp, "pwr_active", &ad->pwr_active))
			DBG("Get %s pwdn_active failed!\n", cp->name);

		if (of_property_read_u32(cp, "pwdn_active", &ad->pwdn_active))
			DBG("Get %s pwdn_active failed!\n", cp->name);

		ad->power = of_get_named_gpio_flags(cp, "rockchip,power",
						    0, &flags);
		ad->powerdown = of_get_named_gpio_flags(cp,
							"rockchip,powerdown",
							0, &flags);

		if (of_property_read_u32(cp, "i2c_add", &ad->i2c_add))
			DBG("Get %s i2c_add failed!\n", cp->name);

		ad->i2c_add = (ad->i2c_add >> 1);

		if (of_property_read_u32(cp, "resolution", &ad->resolution))
			DBG("Get %s resolution failed!\n", cp->name);

		of_property_read_u32_array(
				cp,
				"rockchip,camera-module-defrect0",
				(unsigned int *)&ad->defrects[0],
				6);
		of_property_read_u32_array(
				cp,
				"rockchip,camera-module-defrect1",
				(unsigned int *)&ad->defrects[1],
				6);
		of_property_read_u32_array(
				cp,
				"rockchip,camera-module-defrect2",
				(unsigned int *)&ad->defrects[2],
				6);
		of_property_read_u32_array(
				cp,
				"rockchip,camera-module-defrect3",
				(unsigned int *)&ad->defrects[3],
				6);

		of_property_read_string(
				cp,
				"rockchip,camera-module-interface0",
				&ad->defrects[0].interface);
		of_property_read_string(
				cp,
				"rockchip,camera-module-interface1",
				&ad->defrects[1].interface);
		of_property_read_string(
				cp,
				"rockchip,camera-module-interface2",
				&ad->defrects[2].interface);
		of_property_read_string(
				cp,
				"rockchip,camera-module-interface3",
				&ad->defrects[3].interface);

		ad->ad_name = cp->name;
		for (i = 0; i < ARRAY_SIZE(sensor_cb_series); i++) {
			if (!strcmp(ad->ad_name, sensor_cb_series[i].name))
				sensor_cb = sensor_cb_series + i;
		}

		DBG("%s: ad_chl=%d,,ad_addr=%x,fix_for=%d\n", ad->ad_name,
		    ad->ad_chl, ad->i2c_add, ad->fix_format);
		DBG("gpio power:%d, active:%d\n", ad->power, ad->pwr_active);
		DBG("gpio powerdown:%d, active:%d\n",
		    ad->powerdown, ad->pwdn_active);
		break;
	}
	if (!ad->ad_name)
		ret = -EINVAL;

	return ret;
}

int vehicle_ad_init(struct ad_dev *ad)
{
	int ret = 0;

	if (sensor_cb->sensor_init) {
		ret = sensor_cb->sensor_init(ad);
		if (IS_ERR_VALUE(ret)) {
			DBG("%s sensor_init failed!\n", ad->ad_name);
			goto end;
		}
	} else {
		DBG("%s sensor_init is NULL!\n", ad->ad_name);
		ret = -1;
		goto end;
	}

	if (sensor_cb->sensor_check_id_cb) {
		ret = sensor_cb->sensor_check_id_cb(ad);
		if (IS_ERR_VALUE(ret))
			DBG("%s check id failed!\n", ad->ad_name);
	}

end:
	return ret;
}

int vehicle_ad_deinit(void)
{
	if (sensor_cb->sensor_deinit)
		return sensor_cb->sensor_deinit();
	else
		return -1;
}

struct vehicle_cfg *ad_get_vehicle_cfg(void)
{
	struct vehicle_cfg *cfg;

	if (sensor_cb->sensor_get_cfg)
		sensor_cb->sensor_get_cfg(&cfg);

	return cfg;
}

void ad_check_cif_error(struct ad_dev *ad, int last_line)
{
	if (sensor_cb->sensor_get_cfg)
		sensor_cb->sensor_check_cif_error(ad, last_line);
}

