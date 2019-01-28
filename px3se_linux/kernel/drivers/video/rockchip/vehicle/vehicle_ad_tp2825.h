#ifndef __VEHICLE_AD_TP2825_H__
#define __VEHICLE_AD_TP2825_H__

extern struct ad_dev *g_addev;

int tp2825_ad_init(struct ad_dev *ad);
int tp2825_ad_deinit(void);
int tp2825_ad_get_cfg(struct vehicle_cfg **cfg);
void tp2825_ad_check_cif_error(struct ad_dev *ad, int last_line);
int tp2825_check_id(struct ad_dev *ad);

#endif

