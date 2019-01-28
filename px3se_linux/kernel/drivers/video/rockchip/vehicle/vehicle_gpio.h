#ifndef __VEHICLE_GPIO_H
#define __VEHICLE_GPIO_H

#include "vehicle_cfg.h"

struct gpio_detect {
	int gpio;
	int atv_val;
	int val;
	int irq;
	int mirror;
	unsigned int debounce_ms;
	struct delayed_work work;
	struct device *dev;
};
/*
true : reverse on
false : reverse over
*/
bool vehicle_gpio_reverse_check(struct gpio_detect *gpiod);

int vehicle_gpio_init(struct gpio_detect *gpiod);

int vehicle_gpio_deinit(struct gpio_detect *gpiod);

#endif
