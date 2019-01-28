#ifndef __VEHICLE_AD_7181_H__
#define __VEHICLE_AD_7181_H__

extern struct ad_dev *g_addev;

int adv7181_ad_init(struct ad_dev *ad);
int adv7181_ad_deinit(void);
int adv7181_ad_get_cfg(struct vehicle_cfg **cfg);
void adv7181_ad_check_cif_error(struct ad_dev *ad, int last_line);
int adv7181_check_id(struct ad_dev *ad);

#endif

