#ifndef __VEHICLE_MAIN_H
#define __VEHICLE_MAIN_H

/* impl by vehicle_main, call by ad detect */
void vehicle_ad_stat_change_notify(void);
void vehicle_gpio_stat_change_notify(void);
void vehicle_cif_error_notify(int last_line);
void vehicle_exit_notify(void);

#endif
