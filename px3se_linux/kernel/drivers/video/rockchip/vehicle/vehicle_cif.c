#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/rk_fb.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/rockchip/cru.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "vehicle_cif.h"
#include "vehicle_flinger.h"
#include "vehicle_main.h"

#define __raw_readl(p)	  (*(unsigned int *)(p))
#define __raw_writel(v, p)     (*(unsigned int *)(p) = (v))

#define write_cif_reg(base, addr, val)  __raw_writel(val, addr+(base))
#define read_cif_reg(base, addr) __raw_readl(addr+(base))
struct vehicle_cif *g_cif = NULL;

#define CRU_BASE (g_cif->cru_base)

#define write_cru_reg(addr, val)	__raw_writel(val, addr+CRU_BASE)
#define read_cru_reg(addr)	__raw_readl(addr+CRU_BASE)

static u32 CRU_PCLK_REG30;
static u32 CRU_CLK_OUT;
static u32 clk_cif_out_src_gate_en;
static u32 CRU_CLKSEL29_CON;
static u32 cif0_clk_sel;
static u32 ENANABLE_INVERT_PCLK_CIF0;
static u32 DISABLE_INVERT_PCLK_CIF0;
static u32 ENANABLE_INVERT_PCLK_CIF1;
static u32 DISABLE_INVERT_PCLK_CIF1;
static u32 CHIP_NAME;

extern void rk_camera_cif_iomux(int cif_index);
static int cif_io_mux(void)
{
    if(CHIP_NAME == 3288){
        __raw_writel(((1<<1)|(1<<(1+16))),RK_GRF_VIRT+0x0380);
    }else if(CHIP_NAME == 3368){
        //__raw_writel(((1<<1)|(1<<(1+16))),RK_GRF_VIRT+0x0900);
        __raw_writel(((1<<1)|(1<<(1+16))),CRU_BASE+0x0900);
    } else if (CHIP_NAME == 3228) {
        /* grf_cif_io_sel:select cifio_m1,done by pinctrl */
        /* __raw_writel(((1<<25)|(1<<9)),rk_cif_grf_base+0x0050); */
    }
	/*  pinctrl has been set by vehicle_main when parse dt */

	return 0;
}

static int cif_init_buffer(struct vehicle_cif *cif);

static void cif_dump_regs(struct vehicle_cif *cif)
{
	int val = read_cif_reg(cif->base, CIF_CIF_CTRL);

	if (0)
		return;

	DBG("CIF_CIF_CTRL = 0x%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_INTEN);
	DBG("CIF_CIF_INTEN = 0x%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_INTSTAT);
	DBG("CIF_CIF_INTSTAT = 0x%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_FOR);
	DBG("CIF_CIF_FOR = 0x%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_CROP);
	DBG("CIF_CIF_CROP = 0x%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_SET_SIZE);
	DBG("CIF_CIF_SET_SIZE = 0x%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_SCL_CTRL);
	DBG("CIF_CIF_SCL_CTRL = 0x%x\r\n", val);

	val = read_cru_reg(CRU_PCLK_REG30);
	DBG("CRU_PCLK_REG30 = 0X%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_LAST_LINE);
	DBG("CIF_CIF_LAST_LINE = 0X%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_LAST_PIX);
	DBG("CIF_CIF_LAST_PIX = 0X%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_VIR_LINE_WIDTH);
	DBG("CIF_CIF_VIR_LINE_WIDTH = 0X%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_LINE_NUM_ADDR);
	DBG("CIF_CIF_LINE_NUM_ADDR = 0X%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_FRM0_ADDR_Y);
	DBG("CIF_CIF_FRM0_ADDR_Y = 0X%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_FRM0_ADDR_UV);
	DBG("CIF_CIF_FRM0_ADDR_UV = 0X%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_FRM1_ADDR_Y);
	DBG("CIF_CIF_FRM1_ADDR_Y = 0X%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_FRM1_ADDR_UV);
	DBG("CIF_CIF_FRM1_ADDR_UV = 0X%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_FRAME_STATUS);
	DBG("CIF_CIF_FRAME_STATUS = 0X%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_SCL_VALID_NUM);
	DBG("CIF_CIF_SCL_VALID_NUM = 0X%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_CUR_DST);
	DBG("CIF_CIF_CUR_DST = 0X%x\r\n", val);

	val = read_cif_reg(cif->base, CIF_CIF_LINE_NUM_ADDR);
	DBG("CIF_CIF_LINE_NUM_ADDR = 0X%x\r\n", val);
}
static void cif_reset(struct vehicle_cif  *cif, int only_rst);
static int cif_format_setup(struct vehicle_cif *cif)
{
	struct vehicle_cfg *cfg = &cif->cif_cfg;
	u32 format;
	u32 crop;

	format = (cfg->vsync | (cfg->href<<1) | (cfg->input_format<<2) |
		  (cfg->yuv_order<<5) | (cfg->output_format<<16) |
		  (cfg->field_order<<9));
	crop = (cfg->start_x | (cfg->start_y<<16));

   /*must do it*/
   cif_reset(cif, 1);

	write_cif_reg(cif->base, CIF_CIF_CTRL,
		      AXI_BURST_16|MODE_PINGPONG|DISABLE_CAPTURE);

	write_cif_reg(cif->base, CIF_CIF_FOR, format);
	write_cif_reg(cif->base, CIF_CIF_INTSTAT, 0xFFFFFFFF);
	write_cif_reg(cif->base, CIF_CIF_CROP, crop);
	write_cif_reg(cif->base, CIF_CIF_SET_SIZE,
		      cfg->width | (cfg->height << 16));
	write_cif_reg(cif->base, CIF_CIF_VIR_LINE_WIDTH, cfg->width);
	write_cif_reg(cif->base, CIF_CIF_FRAME_STATUS,  0x00000003);

	/*MUST bypass scale */
	write_cif_reg(cif->base, CIF_CIF_SCL_CTRL, 0x10);

	return 0;
}
static int cif_s_stream(struct vehicle_cif *cif, int enable)
{
	int cif_ctrl_val;

	DBG("%s enable=%d\n", __func__, enable);
	cif_ctrl_val = read_cif_reg(cif->base, CIF_CIF_CTRL);
	if (enable) {
		cif->irqinfo.cifirq_idx = 0;
		cif->irqinfo.cifirq_normal_idx = 0;
		cif->irqinfo.cifirq_abnormal_idx = 0;
		cif->irqinfo.dmairq_idx = 0;
		write_cif_reg(cif->base, CIF_CIF_INTEN, 0x01|0x200);
		cif_ctrl_val |= ENABLE_CAPTURE;
		write_cif_reg(cif->base, CIF_CIF_CTRL, cif_ctrl_val);
	} else {
		cif_ctrl_val &= ~ENABLE_CAPTURE;
		write_cif_reg(cif->base, CIF_CIF_CTRL, cif_ctrl_val);
		write_cif_reg(cif->base, CIF_CIF_INTEN, 0);
	}
	DBG("%s enable=%d succeed\n", __func__, enable);

	return 0;
}
static void cif_cru_set_soft_reset(u32 idx, int on , u32 RK_CRU_SOFTRST_CON)
{
	/* dsb(sy); */
    u32 val = 0;
    void __iomem *reg;

    //reg = (void*)(CRU_BASE + RK_CRU_SOFTRST_CON);

    printk("%s: CHIP_NAME == %d\n", __func__, CHIP_NAME);
    if (CHIP_NAME == 3368 || CHIP_NAME == 3228)
        reg = (void*)(CRU_BASE + RK_CRU_SOFTRST_CON);
    else
        reg = (void*)(RK_CRU_VIRT + RK_CRU_SOFTRST_CON);

    if(CHIP_NAME == 3126){
        val = on ? 0x10001U << 14 : 0x10000U << 14;
    }else if(CHIP_NAME == 3288){
        val = on ? 0x10001U << 8 : 0x10000U << 8;
    }else if(CHIP_NAME == 3368){
        val = on ? 0x10001U << 8 : 0x10000U << 8;
    } else if (CHIP_NAME == 3228) {
        val = on ? 0x00380038 : 0x00380000U;
    }
    writel_relaxed(val, reg);
    dsb(sy);	
}

static void cif_reset(struct vehicle_cif  *cif, int only_rst)
{
	int ctrl_reg, inten_reg, crop_reg, set_size_reg, for_reg;
	int vir_line_width_reg, scl_reg;
	int y0_reg, uv0_reg, y1_reg, uv1_reg;

	u32 RK_CRU_SOFTRST_CON = 0;

    if(CHIP_NAME == 3126) {
        RK_CRU_SOFTRST_CON = RK312X_CRU_SOFTRSTS_CON(6);
    } else if (CHIP_NAME == 3228) {
        RK_CRU_SOFTRST_CON = 0x0324;
    }else if(CHIP_NAME == 3288) {
        RK_CRU_SOFTRST_CON = RK3288_CRU_SOFTRSTS_CON(6);
    }else if(CHIP_NAME == 3368) {
        RK_CRU_SOFTRST_CON = RK3368_CRU_SOFTRSTS_CON(6);
    }

	if (only_rst == 1) {
		cif_cru_set_soft_reset(0, 1, RK_CRU_SOFTRST_CON);
		udelay(5);
		cif_cru_set_soft_reset(0, 0, RK_CRU_SOFTRST_CON);
	} else {
		ctrl_reg = read_cif_reg(cif->base, CIF_CIF_CTRL);
		if (ctrl_reg & ENABLE_CAPTURE)
			write_cif_reg(cif->base, CIF_CIF_CTRL,
				      ctrl_reg & ~ENABLE_CAPTURE);

		crop_reg = read_cif_reg(cif->base, CIF_CIF_CROP);
		set_size_reg = read_cif_reg(cif->base, CIF_CIF_SET_SIZE);
		inten_reg = read_cif_reg(cif->base, CIF_CIF_INTEN);
		for_reg = read_cif_reg(cif->base, CIF_CIF_FOR);
		vir_line_width_reg = read_cif_reg(cif->base,
						  CIF_CIF_VIR_LINE_WIDTH);
		scl_reg = read_cif_reg(cif->base, CIF_CIF_SCL_CTRL);
		y0_reg = read_cif_reg(cif->base, CIF_CIF_FRM0_ADDR_Y);
		uv0_reg = read_cif_reg(cif->base, CIF_CIF_FRM0_ADDR_UV);
		y1_reg = read_cif_reg(cif->base, CIF_CIF_FRM1_ADDR_Y);
		uv1_reg = read_cif_reg(cif->base, CIF_CIF_FRM1_ADDR_UV);

		cif_cru_set_soft_reset(0, 1, RK_CRU_SOFTRST_CON);
		udelay(5);
		cif_cru_set_soft_reset(0, 0, RK_CRU_SOFTRST_CON);

		write_cif_reg(cif->base, CIF_CIF_CTRL,
			      ctrl_reg & ~ENABLE_CAPTURE);
		write_cif_reg(cif->base, CIF_CIF_INTEN, inten_reg);
		write_cif_reg(cif->base, CIF_CIF_CROP, crop_reg);
		write_cif_reg(cif->base, CIF_CIF_SET_SIZE, set_size_reg);
		write_cif_reg(cif->base, CIF_CIF_FOR, for_reg);
		write_cif_reg(cif->base, CIF_CIF_VIR_LINE_WIDTH,
			      vir_line_width_reg);
		write_cif_reg(cif->base, CIF_CIF_SCL_CTRL, scl_reg);
		write_cif_reg(cif->base, CIF_CIF_FRM0_ADDR_Y, y0_reg);
		write_cif_reg(cif->base, CIF_CIF_FRM0_ADDR_UV, uv0_reg);
		write_cif_reg(cif->base, CIF_CIF_FRM1_ADDR_Y, y1_reg);
		write_cif_reg(cif->base, CIF_CIF_FRM1_ADDR_UV, uv1_reg);

		write_cif_reg(cif->base, CIF_CIF_FRAME_STATUS,  0x00000003);
	}
	dsb(sy);  
}

static void cif_reset_delay(struct vehicle_cif *cif)
{
	DBG("%s\n", __func__);
	mdelay(1);
	cif_reset(cif, 0);
	mdelay(1);
  
    if (atomic_read(&cif->stop_cif) == false)
	    cif_s_stream(cif, 1);
	DBG("%s succeed\n", __func__);
}

void cif_capture_en(char *reg, int enable)
{
	int val = 0;

	val = read_cif_reg(reg, CIF_CIF_CTRL);
	if (enable == 1)
		write_cif_reg(reg, CIF_CIF_CTRL, val|0x01);
	else
		write_cif_reg(reg, CIF_CIF_CTRL, val&(~0x01));
}

#define UV_OFFSET (cif->cif_cfg.width * cif->cif_cfg.height)
static int cif_init_buffer(struct vehicle_cif  *cif)
{
	u8 i;
	unsigned long y_addr, uv_addr;

	if (cif->cif_cfg.buf_num < 2)
		return -1;

	if (cif->cif_cfg.buf_num > MAX_BUF_NUM)
		cif->cif_cfg.buf_num = MAX_BUF_NUM;

	for (i = 0 ; i < cif->cif_cfg.buf_num; i++) {
		cif->frame_buf[i] = cif->cif_cfg.buf_phy_addr[i];
		if (cif->frame_buf[i] == 0)
			return -1;
	}

	cif->last_buf_index = 0;
	cif->current_buf_index = 1;

	/*y_addr = cif->frame_buf[0];*/
	y_addr = vehicle_flinger_request_cif_buffer(0);
	uv_addr = y_addr + UV_OFFSET;
	write_cif_reg(cif->base, CIF_CIF_FRM0_ADDR_Y, y_addr);
	write_cif_reg(cif->base, CIF_CIF_FRM0_ADDR_UV, uv_addr);
	cif->active[0] = y_addr;

	/*y_addr = cif->frame_buf[1];*/
	y_addr = vehicle_flinger_request_cif_buffer(1);
	uv_addr = y_addr + UV_OFFSET;
	write_cif_reg(cif->base, CIF_CIF_FRM1_ADDR_Y, y_addr);
	write_cif_reg(cif->base, CIF_CIF_FRM1_ADDR_UV, uv_addr);
	cif->active[1] = y_addr;

	return 0;
}

int cif_next_buffer(struct vehicle_cif *cif, u32 frame_ready)
{
	unsigned long y_addr, uv_addr;

	if ((frame_ready > 1) || (cif->cif_cfg.buf_num < 2) ||
	    (cif->cif_cfg.buf_num > MAX_BUF_NUM))
		return 0;

	cif->last_buf_index = cif->current_buf_index;
	cif->current_buf_index = (cif->current_buf_index + 1) %
				 cif->cif_cfg.buf_num;

	/*y_addr = cif->frame_buf[cif->current_buf_index];*/
	y_addr = vehicle_flinger_request_cif_buffer(frame_ready);
	if (y_addr == 0) {
		DBG("%s, warnning request buffer failed\n", __func__);
		return -1;
	}
	uv_addr = y_addr + UV_OFFSET;
	if (frame_ready == 0) {
		write_cif_reg(cif->base, CIF_CIF_FRM0_ADDR_Y, y_addr);
		write_cif_reg(cif->base, CIF_CIF_FRM0_ADDR_UV, uv_addr);
		cif->active[0] = y_addr;
	} else {
		write_cif_reg(cif->base, CIF_CIF_FRM1_ADDR_Y, y_addr);
		write_cif_reg(cif->base, CIF_CIF_FRM1_ADDR_UV, uv_addr);
		cif->active[1] = y_addr;
	}

	return 0;
}

int cif_irq_error_process(struct vehicle_cif *cif, unsigned int reg_intstat)
{
    if (CHIP_NAME == 3368) {
      	if ((reg_intstat & LINE_ERR) || (reg_intstat & PIX_ERR) ||
      	    (reg_intstat & IFIFO_OF) || (reg_intstat & DFIFO_OF)) {
      		DBG("irq ERROR %x\r\n", reg_intstat);
      		return -2;
      	}
    } else if (CHIP_NAME == 3126) {
      if ((reg_intstat & 0x1) == 0 && ((reg_intstat & LINE_ERR) || (reg_intstat & PIX_ERR) ||
          (reg_intstat & IFIFO_OF) || (reg_intstat & DFIFO_OF))) {
        DBG("irq ERROR %x\r\n", reg_intstat);
        return -2;
      }
   }
	return 0;
}

static void cif_reset_work_func(struct work_struct *work)
{
	struct vehicle_cif *cif = container_of(work, struct vehicle_cif,
			work.work);
	cif_reset_delay(cif);
}

static void cif_cifirq(struct vehicle_cif  *cif)
{
	unsigned int reg_lastpix, reg_lastline;

	/*  1. clear irq status */
	write_cif_reg(cif->base, CIF_CIF_INTSTAT, 0x2fe);
	reg_lastpix = read_cif_reg(cif->base, CIF_CIF_LAST_PIX);
	reg_lastline = read_cif_reg(cif->base, CIF_CIF_LAST_LINE);

	cif->irqinfo.cifirq_idx++;
	if (reg_lastline != cif->cif_cfg.height) {
		cif->irqinfo.cifirq_abnormal_idx = cif->irqinfo.cifirq_idx;
		DBG("Cif irq error:abnormal_idx %ld, %dx%d != %dx%d\n",
		    cif->irqinfo.cifirq_abnormal_idx, reg_lastpix,
		    reg_lastline, cif->cif_cfg.width, cif->cif_cfg.height);
	} else {
		cif->irqinfo.cifirq_normal_idx = cif->irqinfo.cifirq_idx;
	}

	if (cif->irqinfo.cifirq_abnormal_idx > 0) {
		if ((cif->irqinfo.cifirq_idx -
		    cif->irqinfo.cifirq_abnormal_idx) == 1) {
			vehicle_cif_error_notify(reg_lastline);
			DBG("Receive cif error twice, cif reset\n");

			write_cif_reg(cif->base, CIF_CIF_INTSTAT, 0x3f);
			cif_capture_en(cif->base, 0);
			write_cif_reg(cif->base, CIF_CIF_INTEN, 0);
			queue_delayed_work(system_wq, &cif->work,
					 msecs_to_jiffies(1));
		}
	}
}

static inline void cif_dmairq(struct vehicle_cif  *cif)
{
	unsigned int reg_frame_status, reg_cur_dst, reg_cifctrl;
	int frame_ready;
	unsigned long addr;

	reg_cifctrl = read_cif_reg(cif->base, CIF_CIF_CTRL);
	reg_frame_status = read_cif_reg(cif->base, CIF_CIF_FRAME_STATUS);
	reg_cur_dst = read_cif_reg(cif->base, CIF_CIF_CUR_DST);

	write_cif_reg(cif->base, CIF_CIF_INTSTAT, 0x01);
	
	//DBG("reg_frame_status = 0x%x\n", reg_frame_status);
	if (reg_frame_status & (CIF_F0_READY | CIF_F1_READY)) {
		/*  1. clear irq status */
		write_cif_reg(cif->base, CIF_CIF_INTSTAT, 0x01);

		cif->irqinfo.dmairq_idx++;
		if (cif->irqinfo.cifirq_abnormal_idx ==
		    cif->irqinfo.dmairq_idx) {
			write_cif_reg(cif->base, CIF_CIF_FRAME_STATUS,  0x03);
			goto end;
		}

		/*  2. error check */
		if ((reg_frame_status & CIF_F0_READY) &&
		    (reg_frame_status & CIF_F1_READY)) {
			DBG("err f0 && f1 ready\n");
			cif_capture_en(cif->base, 0);
			write_cif_reg(cif->base, CIF_CIF_INTEN, 0);
			queue_delayed_work(system_wq, &cif->work,
					 msecs_to_jiffies(1));
			return;
		}
		if (reg_frame_status & CIF_F0_READY)
			frame_ready = 0;
		else
			frame_ready = 1;

		addr = cif->active[frame_ready];
		if (cif_next_buffer(cif, frame_ready) < 0)
			DBG("cif_nex_buffer error, do not commit %lx\n", addr);
		else
			vehicle_flinger_commit_cif_buffer(addr);
	}
end:
	if ((reg_cifctrl & ENABLE_CAPTURE) == 0)
		write_cif_reg(cif->base, CIF_CIF_CTRL,
			      (reg_cifctrl | ENABLE_CAPTURE));
}

static irqreturn_t rk_camera_irq(int irq, void *data)
{
	struct vehicle_cif *cif = (struct vehicle_cif *)data;
	int reg_intstat;

	reg_intstat = read_cif_reg(cif->base, CIF_CIF_INTSTAT);

		/* error process */
	if (cif_irq_error_process(cif, reg_intstat) < 0) {
		DBG("irq error, to do... reset, intstat=%x\n", reg_intstat);
		write_cif_reg(cif->base,CIF_CIF_INTSTAT,0xffffffff);
		cif_capture_en(cif->base, 0);
		write_cif_reg(cif->base, CIF_CIF_INTEN, 0);

		queue_delayed_work(system_wq, &cif->work,
				 msecs_to_jiffies(1));
		goto IRQ_EXIT;
	}
	/*DBG("IRQ = 0x%x\n", reg_intstat);*/
	if (reg_intstat & 0x0200)
		cif_cifirq(cif);

	if (reg_intstat & 0x01) {
		if(atomic_read(&cif->stop_cif) == true) {
			DBG("%s(%d): cif has stopped by app, disable irq and reset cif\n",__FUNCTION__,__LINE__);
			write_cif_reg(cif->base,CIF_CIF_INTSTAT,0xFFFFFFFF);  /* clear vip interrupte single  */
			write_cif_reg(cif->base, CIF_CIF_INTEN, 0x0);

			cif_reset(cif, true);

			cif->cif_stopped = true;
			wake_up(&cif->cif_stop_done);
		} else {
			cif_dmairq(cif);
		}
	}

    reg_intstat = read_cif_reg(cif->base, CIF_CIF_INTSTAT);

    /*DBG("%s reg_intstat 0x%x succeed\n", __func__, reg_intstat);*/

IRQ_EXIT:
	return IRQ_HANDLED;
}

static int rk_cif_mclk_ctrl(struct rk_cif_clk *clk, int on, int clk_rate)
{
	int err = 0;

	if (!clk->aclk_cif || !clk->hclk_cif ||
	    !clk->cif_clk_in || !clk->cif_clk_out) {
		DBG("failed to get cif clock source\n");
		err = -ENOENT;
		return -1;
	}

	if (on && !clk->on) {
        if (CHIP_NAME != 3228)
            clk_prepare_enable(clk->pd_cif);
        if (CHIP_NAME == 3368)
            clk_prepare_enable(clk->pclk_cif);
		clk_prepare_enable(clk->aclk_cif);
		clk_prepare_enable(clk->hclk_cif);
        if (CHIP_NAME != 3228)
            clk_prepare_enable(clk->cif_clk_in);
		clk_prepare_enable(clk->cif_clk_out);
		clk_set_rate(clk->cif_clk_out, clk_rate);
		clk->on = true;
	} else if (!on && clk->on) {
		clk_set_rate(clk->cif_clk_out, 36000000);
		clk_disable_unprepare(clk->aclk_cif);
		clk_disable_unprepare(clk->hclk_cif);
        if (CHIP_NAME != 3228)
            clk_disable_unprepare(clk->cif_clk_in);
        if (CHIP_NAME == 3126) {
            write_cru_reg(CRU_CLKSEL29_CON, 0x007c0000);
            write_cru_reg(CRU_CLK_OUT, 0x00800080);
        }
		clk_disable_unprepare(clk->cif_clk_out);
        if (CHIP_NAME != 3228)
            clk_disable_unprepare(clk->pd_cif);
        if (CHIP_NAME == 3368)
            clk_disable_unprepare(clk->pclk_cif);

		clk->on = false;
	}

	return err;
}

int vehicle_cif_reverse_open(struct vehicle_cfg *v_cfg)
{
	struct vehicle_cif *cif = g_cif;

	if (!cif)
		return -1;

	DBG("%s\n", __func__);

	memcpy(&cif->cif_cfg, v_cfg, sizeof(struct vehicle_cfg));

	/*  1. format setup */
	cif_format_setup(cif);

	/*  2. cif init buffer */
	if (cif_init_buffer(cif) < 0)
		return -1;

	cif_dump_regs(cif);

	/*  3. start stream */
	atomic_set(&cif->stop_cif,false);
	cif->cif_stopped = false;
	cif_s_stream(cif, 1);

	DBG("%s succeed\n", __func__);

	return 0;
}

int vehicle_cif_reverse_close(void)
{
	if (!g_cif)
		return -1;
	DBG("%s \n", __func__);

	cancel_delayed_work_sync(&(g_cif->work));
	flush_delayed_work(&(g_cif->work));

	atomic_set(&g_cif->stop_cif,true);
	init_waitqueue_head(&g_cif->cif_stop_done);
	if (wait_event_timeout(g_cif->cif_stop_done, g_cif->cif_stopped, msecs_to_jiffies(50)) == 0) {
		DBG("%s:%d, wait cif stop timeout!",__func__,__LINE__);
		write_cif_reg(g_cif->base, CIF_CIF_INTEN, 0x0);
		cif_reset(g_cif, true);
		g_cif->cif_stopped = true;
	}
/*cif_s_stream(g_cif, 0);*/

	DBG("%s succeed\n", __func__);

	return 0;
}

static void rk_camera_diffchips(const char *rockchip_name)
{
    if(strstr(rockchip_name, "3128")||
	   strstr(rockchip_name, "3126")||
	   strstr(rockchip_name, "px3se"))
    {
        CRU_PCLK_REG30 = 0xbc;
        ENANABLE_INVERT_PCLK_CIF0 = ((0x1<<23)|(0x1<<7));
        DISABLE_INVERT_PCLK_CIF0  = ((0x1<<23)|(0x0<<7));
        ENANABLE_INVERT_PCLK_CIF1 = ENANABLE_INVERT_PCLK_CIF0;
        DISABLE_INVERT_PCLK_CIF1  = DISABLE_INVERT_PCLK_CIF0;

        CRU_CLK_OUT = 0xdc;
        clk_cif_out_src_gate_en = ((0x1<<23)|(0x1<<7));
        CRU_CLKSEL29_CON = 0xb8;
        cif0_clk_sel = ((0x1<<23)|(0x0<<7));

        CHIP_NAME = 3126;
    }
    else if(strstr(rockchip_name,"3288"))
    {
        CRU_PCLK_REG30 = 0xd4;
        ENANABLE_INVERT_PCLK_CIF0 = ((0x1<<20)|(0x1<<4));
        DISABLE_INVERT_PCLK_CIF0  = ((0x1<<20)|(0x0<<4));
        ENANABLE_INVERT_PCLK_CIF1 = ENANABLE_INVERT_PCLK_CIF0;
        DISABLE_INVERT_PCLK_CIF1  = DISABLE_INVERT_PCLK_CIF0;

        CRU_CLK_OUT = 0x16c;
        CHIP_NAME = 3288;
    } else if (strstr(rockchip_name, "3368") ||
           strstr(rockchip_name, "px5"))
    {
        CRU_PCLK_REG30 = 0x154;
        ENANABLE_INVERT_PCLK_CIF0 = ((0x1<<29)|(0x1<<13));
        DISABLE_INVERT_PCLK_CIF0  = ((0x1<<29)|(0x0<<13));
        ENANABLE_INVERT_PCLK_CIF1 = ENANABLE_INVERT_PCLK_CIF0;
        DISABLE_INVERT_PCLK_CIF1  = DISABLE_INVERT_PCLK_CIF0;

        //CRU_CLK_OUT = 0x16c;
        CHIP_NAME = 3368;
    } else if (strstr(rockchip_name, "3228h")) {
        CRU_PCLK_REG30 = 0x0410;
        ENANABLE_INVERT_PCLK_CIF0 = ((0x1<<31)|(0x1<<15));
        DISABLE_INVERT_PCLK_CIF0  = ((0x1<<31)|(0x0<<15));
        ENANABLE_INVERT_PCLK_CIF1 = ENANABLE_INVERT_PCLK_CIF0;
        DISABLE_INVERT_PCLK_CIF1  = DISABLE_INVERT_PCLK_CIF0;
        CHIP_NAME = 3228;
    }
}

static int cif_parse_dt(struct vehicle_cif *cif)
{
	struct device *dev = cif->dev;
	struct device_node *node;
	struct device_node *cif_node;
	const char *compatible = NULL;
	int err;

    err = of_property_read_string(dev->of_node->parent,"compatible",&compatible);
	if(err < 0) {
		DBG("Get compatible failed\n");
		return -1;
	}

	rk_camera_diffchips(compatible);

	DBG("%s:compatible %s, CHIP NAME %d\n", __func__, compatible, CHIP_NAME);

	if (of_property_read_u32(dev->of_node, "cif,drop-frames",
				 &cif->drop_frames)) {
		DBG("%s:Get cif, drop-frames failed!\n", __func__);
	}

	cif_node = of_parse_phandle(dev->of_node, "rockchip,cif", 0);
	cif->base = (char *)of_iomap(cif_node, 0);

	node = of_parse_phandle(dev->of_node, "rockchip,cru", 0);
	if (node)
		cif->cru_base = (unsigned long)of_iomap(node, 0);
	else
		cif->cru_base = (unsigned long)RK_CRU_VIRT;

	node = of_parse_phandle(dev->of_node, "rockchip,grf", 0);
	if (node)
		cif->grf_base = (unsigned long)of_iomap(node, 0);

	cif->irq = irq_of_parse_and_map(cif_node, 0);
	if (cif->irq < 0) {
		DBG("%s: request irq failed\n", __func__);
		return -1;
	}

	DBG("%s, drop_frames = %d\n", __func__, cif->drop_frames);
	return 0;
}
int	vehicle_cif_init(struct vehicle_cif *cif)
{
	int ret;
	struct device *dev = cif->dev;
	struct rk_cif_clk *clk = &cif->clk;

	if (!cif)
		return -1;

	g_cif = cif;

	if (cif_parse_dt(cif) < -1) {
		DBG("%s: cif_parse_dt failed\n", __func__);
		return -1;
	}

	/*  init addr */
	/* cif->base = ioremap(BASE_CIF, REG_LEN); */

	/*  1. iomux */
	cif_io_mux();

	/*  2. cif clk setup */
	if (1) {//IS_CIF0()) {
		if (CHIP_NAME != 3228)
			clk->pd_cif = devm_clk_get(dev, "pd_cif0");
		if (CHIP_NAME == 3368)
			clk->pclk_cif = devm_clk_get(dev, "pclk_cif");
		clk->aclk_cif = devm_clk_get(dev, "aclk_cif0");
		clk->hclk_cif = devm_clk_get(dev, "hclk_cif0");
		if (CHIP_NAME != 3228)
			clk->cif_clk_in = devm_clk_get(dev, "cif0_in");
		clk->cif_clk_out = devm_clk_get(dev, "cif0_out");
	} else {
        if (CHIP_NAME != 3368 && CHIP_NAME != 3228)
            clk->pd_cif = devm_clk_get(dev, "pd_cif0");/*cif0  only */

        clk->aclk_cif = devm_clk_get(dev, "aclk_cif0");
        clk->hclk_cif = devm_clk_get(dev, "hclk_cif0");
        if (CHIP_NAME != 3228)
            clk->cif_clk_in = devm_clk_get(dev, "cif0_in");
        clk->cif_clk_out = devm_clk_get(dev, "cif0_out");
    }

	/*  2. set cif clk */
	rk_cif_mclk_ctrl(clk, 1, 24000000);

	INIT_DELAYED_WORK(&cif->work, cif_reset_work_func);
	atomic_set(&cif->stop_cif, true);
	cif->cif_stopped = true;

	/*  3. request irq */
	ret = request_irq(cif->irq, rk_camera_irq, IRQF_SHARED, "veh_cif", cif);
	if (ret < 0) {
		DBG("yuyz test: request cif irq\n");
		return -1;
	}

	return 0;
}

int vehicle_cif_deinit(struct vehicle_cif *cif)
{
	struct rk_cif_clk *clk = &cif->clk;
	struct device *dev = cif->dev;

	cif_s_stream(cif, 0);

	rk_cif_mclk_ctrl(clk, 0, 0);

	if (CHIP_NAME != 3228)
		devm_clk_put(dev, clk->pd_cif);
	if (CHIP_NAME == 3368)
		devm_clk_put(dev, clk->pclk_cif);
	if (CHIP_NAME != 3228)
		devm_clk_put(dev, clk->cif_clk_in);
	devm_clk_put(dev, clk->aclk_cif);
	devm_clk_put(dev, clk->hclk_cif);
	devm_clk_put(dev, clk->cif_clk_out);

	free_irq(cif->irq, cif);
	return 0;
}
