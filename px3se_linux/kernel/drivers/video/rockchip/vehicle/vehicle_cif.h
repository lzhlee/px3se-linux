#ifndef __VEHICLE_CIF_H
#define __VEHICLE_CIF_H

#include "vehicle_cfg.h"

struct rk_cif_clk {
	/************must modify start************/
	struct clk *pd_cif;
	struct clk *aclk_cif;
	struct clk *hclk_cif;
	struct clk *cif_clk_in;
	struct clk *cif_clk_out;
	struct clk *pclk_cif;
	/************must modify end************/

	/*  spinlock_t lock; */
	bool on;
};

struct rk_cif_irqinfo {
	unsigned int irq;
	unsigned long cifirq_idx;
	unsigned long cifirq_normal_idx;
	unsigned long cifirq_abnormal_idx;

	unsigned long dmairq_idx;
};

struct vehicle_cif {
	struct device *dev;
	struct rk_cif_clk clk;
	struct vehicle_cfg cif_cfg;
	char *base;  /*cif base addr*/
	unsigned long cru_base;
	unsigned long grf_base;
	struct delayed_work work;

	u32 frame_buf[MAX_BUF_NUM];
	u32 current_buf_index;
	u32 last_buf_index;
	u32 active[2];
	int irq;
	int drop_frames;
	struct rk_cif_irqinfo irqinfo;

	atomic_t stop_cif;
	wait_queue_head_t cif_stop_done;
	volatile  bool cif_stopped;
};

int vehicle_cif_init(struct vehicle_cif *cif);
int vehicle_cif_deinit(struct vehicle_cif *cif);

int vehicle_cif_reverse_open(struct vehicle_cfg *v_cfg);

int vehicle_cif_reverse_close(void);

/* CIF IRQ STAT*/
#define DMA_FRAME_END					(0x01 << 0)
#define LINE_END						(0x01 << 1)
#define LINE_ERR						(0x01 << 2)
#define PIX_ERR							(0x01 << 3)
#define IFIFO_OF						(0x01 << 4)
#define DFIFO_OF						(0x01 << 5)
#define BUS_ERR							(0x01 << 6)
#define PRE_INF_FRAME_END				(0x01 << 8)
#define PST_INF_FRAME_END				(0x01 << 9)

/* CIF Reg Offset*/
#define  CIF_CIF_CTRL               0x00
#define  CIF_CIF_INTEN              0x04
#define  CIF_CIF_INTSTAT            0x08
#define  CIF_CIF_FOR            0x0c
#define  CIF_CIF_LINE_NUM_ADDR          0x10
#define  CIF_CIF_FRM0_ADDR_Y        0x14
#define  CIF_CIF_FRM0_ADDR_UV           0x18
#define  CIF_CIF_FRM1_ADDR_Y        0x1c
#define  CIF_CIF_FRM1_ADDR_UV           0x20
#define  CIF_CIF_VIR_LINE_WIDTH         0x24
#define  CIF_CIF_SET_SIZE           0x28
#define  CIF_CIF_SCM_ADDR_Y         0x2c
#define  CIF_CIF_SCM_ADDR_U         0x30
#define  CIF_CIF_SCM_ADDR_V         0x34
#define  CIF_CIF_WB_UP_FILTER           0x38
#define  CIF_CIF_WB_LOW_FILTER          0x3c
#define  CIF_CIF_WBC_CNT            0x40
#define  CIF_CIF_CROP               0x44
#define  CIF_CIF_SCL_CTRL           0x48
#define	 CIF_CIF_SCL_DST            0x4c
#define	 CIF_CIF_SCL_FCT            0x50
#define	 CIF_CIF_SCL_VALID_NUM          0x54
#define	 CIF_CIF_LINE_LOOP_CTR          0x58
#define	 CIF_CIF_FRAME_STATUS           0x60
#define	 CIF_CIF_CUR_DST            0x64
#define	 CIF_CIF_LAST_LINE          0x68
#define	 CIF_CIF_LAST_PIX           0x6c

/*The key register bit descrition*/
/* CIF_CTRL Reg , ignore SCM, WBC, ISP, */
#define  DISABLE_CAPTURE          (0x00<<0)
#define  ENABLE_CAPTURE           (0x01<<0)
#define  MODE_ONEFRAME        (0x00<<1)
#define  MODE_PINGPONG        (0x01<<1)
#define  MODE_LINELOOP        (0x02<<1)
#define  AXI_BURST_16         (0x0F << 12)

/*CIF_CIF_INTEN*/
#define  FRAME_END_EN			(0x01<<1)
#define  BUS_ERR_EN				(0x01<<6)
#define  SCL_ERR_EN				(0x01<<7)

/* CIF_CIF_FRAME_STATUS */
#define CIF_F0_READY (0x01<<0)
#define CIF_F1_READY (0x01<<1)

/*CIF_CIF_FOR*/
#define  VSY_HIGH_ACTIVE           (0x01<<0)
#define  VSY_LOW_ACTIVE            (0x00<<0)
#define  HSY_LOW_ACTIVE				(0x01<<1)
#define  HSY_HIGH_ACTIVE			(0x00<<1)
#define  INPUT_MODE_YUV				(0x00<<2)
#define  INPUT_MODE_PAL				(0x02<<2)
#define  INPUT_MODE_NTSC			(0x03<<2)
#define  INPUT_MODE_RAW				(0x04<<2)
#define  INPUT_MODE_JPEG			(0x05<<2)
#define  INPUT_MODE_MIPI			(0x06<<2)
#define  YUV_INPUT_ORDER_UYVY(ori)   (ori & (~(0x03<<5)))
#define  YUV_INPUT_ORDER_YVYU(ori)   ((ori & (~(0x01<<6)))|(0x01<<5))
#define  YUV_INPUT_ORDER_VYUY(ori)   ((ori & (~(0x01<<5))) | (0x1<<6))
#define  YUV_INPUT_ORDER_YUYV(ori)		   (ori|(0x03<<5))
#define  YUV_INPUT_422		           (0x00<<7)
#define  YUV_INPUT_420		           (0x01<<7)
#define  INPUT_420_ORDER_EVEN		       (0x00<<8)
#define  INPUT_420_ORDER_ODD		       (0x01<<8)
#define  CCIR_INPUT_ORDER_ODD		       (0x00<<9)
#define  CCIR_INPUT_ORDER_EVEN         (0x01<<9)
#define  RAW_DATA_WIDTH_8          (0x00<<11)
#define  RAW_DATA_WIDTH_10         (0x01<<11)
#define  RAW_DATA_WIDTH_12         (0x02<<11)
#define  YUV_OUTPUT_422            (0x00<<16)
#define  YUV_OUTPUT_420            (0x01<<16)
#define  OUTPUT_420_ORDER_EVEN         (0x00<<17)
#define  OUTPUT_420_ORDER_ODD          (0x01<<17)
#define  RAWD_DATA_LITTLE_ENDIAN       (0x00<<18)
#define  RAWD_DATA_BIG_ENDIAN          (0x01<<18)
#define  UV_STORAGE_ORDER_UVUV         (0x00<<19)
#define  UV_STORAGE_ORDER_VUVU         (0x01<<19)

/*CIF_CIF_SCL_CTRL*/
#define ENABLE_SCL_DOWN            (0x01<<0)
#define DISABLE_SCL_DOWN           (0x00<<0)
#define ENABLE_SCL_UP              (0x01<<1)
#define DISABLE_SCL_UP             (0x00<<1)
#define ENABLE_YUV_16BIT_BYPASS        (0x01<<4)
#define DISABLE_YUV_16BIT_BYPASS       (0x00<<4)
#define ENABLE_RAW_16BIT_BYPASS        (0x01<<5)
#define DISABLE_RAW_16BIT_BYPASS       (0x00<<5)
#define ENABLE_32BIT_BYPASS        (0x01<<6)
#define DISABLE_32BIT_BYPASS           (0x00<<6)

enum rk_camera_signal_polarity {
	RK_CAMERA_DEVICE_SIGNAL_HIGH_LEVEL = 1,
	RK_CAMERA_DEVICE_SIGNAL_LOW_LEVEL = 0,
};
enum rk_camera_device_type {
	RK_CAMERA_DEVICE_BT601_8	= 0x10000011,
	RK_CAMERA_DEVICE_BT601_10	= 0x10000012,
	RK_CAMERA_DEVICE_BT601_12	= 0x10000014,
	RK_CAMERA_DEVICE_BT601_16	= 0x10000018,

	RK_CAMERA_DEVICE_BT656_8	= 0x10000021,
	RK_CAMERA_DEVICE_BT656_10	= 0x10000022,
	RK_CAMERA_DEVICE_BT656_12	= 0x10000024,
	RK_CAMERA_DEVICE_BT656_16	= 0x10000028,

	RK_CAMERA_DEVICE_CVBS_NTSC	= 0x20000001,
	RK_CAMERA_DEVICE_CVBS_PAL	= 0x20000002
};
#endif
