/* Copyright (c) 2012-2018, 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/qpnp/pin.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/qpnp/pwm.h>
#include <linux/err.h>
#include <linux/string.h>

//ASUS BSP Display +++
#include <linux/msm_mdp.h>
#include <linux/syscalls.h>
#include <linux/proc_fs.h>
#include "mdss_panel.h"
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/file.h>
#include <linux/uaccess.h>
//ASUS BSP Display ---

#include "mdss_dsi.h"
#include "mdss_dba_utils.h"
#include "mdss_debug.h"
#include "mdss_livedisplay.h"

#ifdef CONFIG_POWERSUSPEND
#include <linux/powersuspend.h>
#endif

#define DT_CMD_HDR 6
#define DEFAULT_MDP_TRANSFER_TIME 14000
#define ASUS_ZE620KL_PROJECT 1
#define VSYNC_DELAY msecs_to_jiffies(17)

//ASUS_BSP: display basic variables and function prototypes
//          ZC600KL is not OEM project, skip those functions

#define ASUS_LCD_PROC_REGISTER_RW      "driver/panel_reg_rw"
#define ASUS_LCD_PROC_CABC             "driver/cabc"
#define ASUS_LCD_PROC_DUMP_CALI_INFO   "driver/panel_info"
#define ASUS_LCD_PROC_UNIQUE_ID        "lcd_unique_id"
#define ASUS_LCD_PROC_PANEL_ID         "lcd_id"
#define ASUS_LCD_REG_RW_MIN_LEN         2
#define ASUS_LCD_REG_RW_MAX_LEN         4
#define WLED_MAX_LEVEL_ENABLE           4095
#define WLED_MIN_LEVEL_DISABLE          0
#define LCD_BL_THRESHOLD_TM             44
#define LCD_BL_THRESHOLD_AUO            44
#define LCD_BL_THRESHOLD_BOE            11

#define ASUS_LCD_CALI_BL_TITAN_DEFAULT 650
#define ASUS_LCD_CALI_PATH     "/factory/lcd_calibration.ini"
#define ASUS_LCD_CALI_BL_PATH  "/data/data/bl_adjust"
#define ASUS_LCD_CALI_DO       "on"
#define ASUS_LCD_CALI_SHIFT_MASK 23

int8_t g_lcd_uniqueId_crc = -1;
int g_asus_lcdID_verify = 0;
int g_calibrated_bl = -1;
int g_actual_bl = -1;
int g_adjust_bl = 0;

static DEFINE_SPINLOCK(asus_lcd_bklt_spinlock);
static bool g_timer = false;
static bool g_bl_wled_enable = false;
static int g_previous_bl = 0x0;
static int g_last_bl = 0x0;
static int g_bl_threshold;
static int g_wled_dimming_div;
static int g_mdss_first_boot = 1;
extern int asus_lcd_display_commit_cnt;
extern u32 g_update_bl;

char lcd_unique_id[64] = {0};
extern struct mdss_panel_data *g_mdss_pdata;
extern int g_asus_lcdID;
extern int g_ftm_mode;
extern bool asus_lcd_tcon_cmd_fence;
extern bool g_asus_lcd_power_off;

void asus_tcon_cmd_set(char *cmd, short len);
void asus_tcon_cmd_get(char cmd, int rlen);
void asus_tcon_cmd_set_unlocked(char *cmd, short len);
static struct mutex asus_lcd_tcon_cmd_mutex;
static struct mutex asus_lcd_bl_cmd_mutex;

char asus_lcd_dimming_cmd[2] = {0x53, 0x2C};
char asus_lcd_cabc_mode[2]   = {0x55, Still_MODE};
char asus_lcd_invert_mode[2]   = {0x21, 0x21};

static struct panel_list supp_panels[] = {
	{"AUO", ARA_LCD_AUO},
	{"AUO2", ARA_LCD_AUO_2},
};
char g_reg_buffer[256];

static void asus_lcd_delay_work_func(struct work_struct *);
static DECLARE_DELAYED_WORK(asus_lcd_delay_work, asus_lcd_delay_work_func);
static struct workqueue_struct *asus_lcd_delay_workqueue;
static u32 asus_lcd_read_chip_unique_id(void);

// -----------
// Ara project
// -----------
#ifdef ASUS_ZE620KL_PROJECT
// Asus Ara AUO LCD HW ID, default value is 1st MP panel
void asus_lcd_read_hw_id(void);
bool asus_lcd_is_shipping(void);
bool asus_lcd_is_second_source(void);

u32 asus_lcd_hw_id_0xDA = 0x61;
u32 asus_lcd_hw_id_0xDC = 0xA2;

// for Ara second source panel wled fs current
#define ARA_AUO_PANEL2_WLED_FS_CURR_UA 22500
extern int qpnp_wled_fs_curr_ua_set(unsigned int curr_ua);

// for Touch suspend/resume
extern int fts_ts_resume(void);

// -------------
// Titan project
// -------------
#else
#ifdef ASUS_ZE554KL_PROJECT
// for Touch suspend/resume
extern int asus_rmi4_suspend(void);
extern int asus_rmi4_resume(void);
#endif
#endif

//ASUS_BSP ---

static mm_segment_t asus_lcd_cali_oldfs;

static void asus_lcd_cali_init_fs(void)
{
	asus_lcd_cali_oldfs = get_fs();
	set_fs(KERNEL_DS);
}

static void asus_lcd_cali_deinit_fs(void)
{
	set_fs(asus_lcd_cali_oldfs);
}

typedef void (*asus_lcd_cali_func_type)( const char *msg, int index);

struct asus_lcd_cali_command{
	char *id;
	int len;
	const asus_lcd_cali_func_type func;
};

int asus_lcd_cali_cal_bl_float(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4)
{
	unsigned int raw_data, mantissa;
	int mantissa_val=0;
	int exp, i, float_bl = -1;

	raw_data = (byte1<<24) | (byte2<<16) | (byte3<<8) | byte4;

	exp = ((raw_data & 0x7F800000) >> ASUS_LCD_CALI_SHIFT_MASK);
	exp = exp - 127;

	mantissa = raw_data & 0x7FFFFF;
	for(i = 0; i <= ASUS_LCD_CALI_SHIFT_MASK; i++) {	/*bit 23 = 1/2, bit 22 = 1/4*/
		if ((mantissa >> (ASUS_LCD_CALI_SHIFT_MASK - i)) & 0x01)
			mantissa_val += 100000/(0x01 << i);
	}

	float_bl = (0x01 << exp) + ((0x01 << exp) * mantissa_val / 100000);

	return float_bl;
}

bool asus_lcd_cali_adjust_backlight(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4, uint8_t byte5)
{
	unsigned int readstr;
	unsigned int asus_lcdID;
	int readlen = 0;
	struct inode *inode;
	struct file *fp = NULL;
	unsigned char *buf = NULL;

	sscanf(lcd_unique_id, "%x", &readstr);
	asus_lcdID = ((readstr>>24) ^ 0x55 ^ (readstr>>16) ^ (readstr>>8) ^ (readstr)) & 0xff;

	printk("[Display] Panel Unique ID from Phone: 0x%x\n", asus_lcdID);
	printk("[Display] Panel Unique ID from File : 0x%x\n", byte1);

	g_actual_bl = asus_lcd_cali_cal_bl_float( byte2, byte3, byte4, byte5);
	printk("[Display] Actual Panel Backlight value = %d \n", g_actual_bl);

	if (asus_lcdID == byte1) {
		asus_lcd_cali_init_fs();
		fp = filp_open(ASUS_LCD_CALI_BL_PATH, O_RDONLY, 0);
		if (!IS_ERR_OR_NULL(fp)) {
			inode = fp->f_path.dentry->d_inode;
			buf = kmalloc(inode->i_size, GFP_KERNEL);

			if (fp->f_op != NULL && fp->f_op->read != NULL) {
				readlen = fp->f_op->read(fp, buf, inode->i_size, &(fp->f_pos));
				buf[readlen] = '\0';
			}
			sscanf(buf, "%d", &g_adjust_bl);
			printk("[Display] Dynamic calibration Backlight value = %d\n", g_adjust_bl);

			g_asus_lcdID_verify = 1;
			asus_lcd_cali_deinit_fs();
			filp_close(fp, NULL);
			kfree(buf);
		} else {
			g_adjust_bl = ASUS_LCD_CALI_BL_TITAN_DEFAULT;
			pr_err("[Display] Dynamic calibration Backlight disable\n");
			return false;
		}
	} else
		return false;

	return true;
}

bool asus_lcd_cali_file_read(void)
{
	int ret;
	int readlen =0;
	off_t fsize;
	struct file *fp = NULL;
	struct inode *inode;
	uint8_t *buf = NULL;

	asus_lcd_cali_init_fs();

	fp = filp_open(ASUS_LCD_CALI_PATH, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		pr_err("[Display] Read (%s) failed\n", ASUS_LCD_CALI_PATH);
		return false;
	}

	inode = fp->f_path.dentry->d_inode;
	fsize = inode->i_size;
	buf = kmalloc(fsize, GFP_KERNEL);

	if (fp->f_op != NULL) {
		readlen = vfs_read(fp, buf, fsize, &(fp->f_pos));
		if (readlen < 0) {
			DEV_ERR("[Display] Read (%s) error\n", ASUS_LCD_CALI_PATH);
			asus_lcd_cali_deinit_fs();
			filp_close(fp, NULL);
			kfree(buf);
			return false;
		}
	} else
		pr_err("[Display] Read (%s) f_op=NULL or op->read=NULL\n", ASUS_LCD_CALI_PATH);

	asus_lcd_cali_deinit_fs();
	filp_close(fp, NULL);

	//g_calibrated_bl = (buf[60]<<8) + buf[61];

	g_calibrated_bl = asus_lcd_cali_cal_bl_float( buf[102], buf[101], buf[100], buf[99]);
	printk("[Display] Panel Calibrated Backlight: %d\n", g_calibrated_bl);
	g_lcd_uniqueId_crc = buf[5];

	ret = asus_lcd_cali_adjust_backlight(buf[5], buf[55], buf[54], buf[53], buf[52]);
	if(!ret)
		g_asus_lcdID_verify = 0;

	kfree(buf);
	return true;
}

void asus_lcd_cali_bl(const char *msg, int index)
{
	int ret;
	ret = asus_lcd_cali_file_read();
	if(!ret)
		pr_err("[Display] Dynamic Calibration Backlight disable\n");
}

struct asus_lcd_cali_command asus_lcd_cali_cmd_tb[] = {
	{ASUS_LCD_CALI_DO, sizeof(ASUS_LCD_CALI_DO), asus_lcd_cali_bl},		/*do calibration*/
};

static void asus_lcd_cali_write(char *msg)
{
	int i = 0;
	for(; i<ARRAY_SIZE(asus_lcd_cali_cmd_tb);i++){
		if(strncmp(msg, asus_lcd_cali_cmd_tb[i].id, asus_lcd_cali_cmd_tb[i].len-1)==0){
			asus_lcd_cali_cmd_tb[i].func(msg, asus_lcd_cali_cmd_tb[i].len-1);
			return;
		}
	}
	pr_err("### error Command no found in  %s ###\n", __FUNCTION__);
	return;
}

static ssize_t asus_lcd_info_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
	char messages[256];

	memset(messages, 0, sizeof(messages));

	if (len > 256)
		len = 256;
	if (copy_from_user(messages, buff, len))
		return -EFAULT;

	pr_info("[Dislay] ### %s ###\n", __func__);

	asus_lcd_cali_write(messages);

	return len;
}

static ssize_t asus_lcd_info_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	int len = 0;
	ssize_t ret = 0;
	char *buff;

	buff = kmalloc(512, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	len += sprintf(buff, "===================\nPanel Vendor: %s\nUnique ID: %s\nUnique ID after CRC: %x\nSW Calibration Enable: %s\nCalibrated Value: %d\nActual Backlight: %d\nAdjust Backlight: %d\nCABC Mode: %d\n===================\n",
		supp_panels[g_asus_lcdID].name, lcd_unique_id, g_lcd_uniqueId_crc, g_asus_lcdID_verify ? "Y" : "N", g_calibrated_bl, g_actual_bl, g_adjust_bl, asus_lcd_cabc_mode[1]);
	ret = simple_read_from_buffer(buf, count, ppos, buff, len);
	kfree(buff);

	return ret;
}

static struct file_operations asus_lcd_info_proc_ops = {
	.write = asus_lcd_info_write,
	.read  = asus_lcd_info_read,
};
//ASUS_BSP: ---

DEFINE_LED_TRIGGER(bl_led_trigger);

void mdss_dsi_panel_pwm_cfg(struct mdss_dsi_ctrl_pdata *ctrl)
{
	if (ctrl->pwm_pmi)
		return;

	ctrl->pwm_bl = pwm_request(ctrl->pwm_lpg_chan, "lcd-bklt");
	if (ctrl->pwm_bl == NULL || IS_ERR(ctrl->pwm_bl)) {
		pr_err("%s: Error: lpg_chan=%d pwm request failed",
				__func__, ctrl->pwm_lpg_chan);
	}
	ctrl->pwm_enabled = 0;
}

bool mdss_dsi_panel_pwm_enable(struct mdss_dsi_ctrl_pdata *ctrl)
{
	bool status = true;
	if (!ctrl->pwm_enabled)
		goto end;

	if (pwm_enable(ctrl->pwm_bl)) {
		pr_err("%s: pwm_enable() failed\n", __func__);
		status = false;
	}

	ctrl->pwm_enabled = 1;

end:
	return status;
}

static void mdss_dsi_panel_bklt_pwm(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	int ret;
	u32 duty;
	u32 period_ns;

	if (ctrl->pwm_bl == NULL) {
		pr_err("%s: no PWM\n", __func__);
		return;
	}

	if (level == 0) {
		if (ctrl->pwm_enabled) {
			ret = pwm_config_us(ctrl->pwm_bl, level,
					ctrl->pwm_period);
			if (ret)
				pr_err("%s: pwm_config_us() failed err=%d.\n",
						__func__, ret);
			pwm_disable(ctrl->pwm_bl);
		}
		ctrl->pwm_enabled = 0;
		return;
	}

	duty = level * ctrl->pwm_period;
	duty /= ctrl->bklt_max;

	pr_debug("%s: bklt_ctrl=%d pwm_period=%d pwm_gpio=%d pwm_lpg_chan=%d\n",
			__func__, ctrl->bklt_ctrl, ctrl->pwm_period,
				ctrl->pwm_pmic_gpio, ctrl->pwm_lpg_chan);

	pr_debug("%s: ndx=%d level=%d duty=%d\n", __func__,
					ctrl->ndx, level, duty);

	if (ctrl->pwm_period >= USEC_PER_SEC) {
		ret = pwm_config_us(ctrl->pwm_bl, duty, ctrl->pwm_period);
		if (ret) {
			pr_err("%s: pwm_config_us() failed err=%d.\n",
					__func__, ret);
			return;
		}
	} else {
		period_ns = ctrl->pwm_period * NSEC_PER_USEC;
		ret = pwm_config(ctrl->pwm_bl,
				level * period_ns / ctrl->bklt_max,
				period_ns);
		if (ret) {
			pr_err("%s: pwm_config() failed err=%d.\n",
					__func__, ret);
			return;
		}
	}

	if (!ctrl->pwm_enabled) {
		ret = pwm_enable(ctrl->pwm_bl);
		if (ret)
			pr_err("%s: pwm_enable() failed err=%d\n", __func__,
				ret);
		ctrl->pwm_enabled = 1;
	}
}

static char dcs_cmd[2] = {0x54, 0x00}; /* DTYPE_DCS_READ */
static struct dsi_cmd_desc dcs_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(dcs_cmd)},
	dcs_cmd
};

int mdss_dsi_panel_cmd_read(struct mdss_dsi_ctrl_pdata *ctrl, char cmd0,
		char cmd1, void (*fxn)(int), char *rbuf, int len)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_info *pinfo;

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			return -EINVAL;
	}

	dcs_cmd[0] = cmd0;
	dcs_cmd[1] = cmd1;
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &dcs_read_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = len;
	cmdreq.rbuf = rbuf;
	cmdreq.cb = fxn; /* call back */
	/*
	 * blocked here, until call back called
	 */

	return mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

static void mdss_dsi_panel_apply_settings(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_panel_cmds *pcmds)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_info *pinfo;

	pinfo = &(ctrl->panel_data.panel_info);
	if ((pinfo->dcs_cmd_by_left) && (ctrl->ndx != DSI_CTRL_LEFT))
		return;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = pcmds->cmds;
	cmdreq.cmds_cnt = pcmds->cmd_cnt;
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}


void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_panel_cmds *pcmds, u32 flags)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_info *pinfo;

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			return;
	}

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = pcmds->cmds;
	cmdreq.cmds_cnt = pcmds->cmd_cnt;
	cmdreq.flags = flags;

	/*Panel ON/Off commands should be sent in DSI Low Power Mode*/
	if (pcmds->link_state == DSI_LP_MODE)
		cmdreq.flags  |= CMD_REQ_LP_MODE;
	else if (pcmds->link_state == DSI_HS_MODE)
		cmdreq.flags |= CMD_REQ_HS_MODE;

	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

static char led_pwm1[2] = {0x51, 0x0};	/* DTYPE_DCS_WRITE1 */
static struct dsi_cmd_desc backlight_cmd = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm1)},
	led_pwm1
};

#ifndef ASUS_ZC600KL_PROJECT
/* backlight DCS command 2 parameter */
static char led_pwm2[3] = {0x51, 0x00, 0x00};
static struct dsi_cmd_desc tbyte_backlight_cmd = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(led_pwm2)},
	led_pwm2
};
#endif

static void mdss_dsi_panel_bklt_dcs(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_info *pinfo;
	int tmp_level = 0;
#if !defined(ASUS_FACTORY_BUILD) && !defined(ASUS_ZC600KL_PROJECT)
	static bool g_bl_first_bootup = true;
	int rc;
#endif

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			return;
	}

#if !defined(ASUS_FACTORY_BUILD) && !defined(ASUS_ZC600KL_PROJECT)
	if (g_bl_first_bootup) {
		rc = asus_lcd_cali_file_read();
		if(!rc) {
			pr_err("%s: Backlight calibration read failed\n", __func__);
		}
		g_bl_first_bootup = false;
	}
#endif

	tmp_level = level;

#if !defined(ASUS_FACTORY_BUILD) && !defined(ASUS_ZC600KL_PROJECT)
	if ((g_adjust_bl < g_calibrated_bl) && (g_adjust_bl > 0))
		level = level * g_adjust_bl / g_calibrated_bl;
#endif

	if (level < pinfo->bl_min)
		level = pinfo->bl_min;

#ifndef ASUS_ZC600KL_PROJECT
	pr_debug("%s:[Display] Set %s brightness (Actual:%d) (Adjust:%d)\n",
		__func__, supp_panels[g_asus_lcdID].name, tmp_level, level);

	if (g_asus_lcdID == ARA_LCD_AUO || g_asus_lcdID == ARA_LCD_AUO_2) {
		led_pwm2[1] = (unsigned char) (level >> 2) & 0xFF; //10bit
		led_pwm2[2] = (unsigned char) (level) & 0x03;
		pr_debug("%s:[DISPLAY] AUO level(0x%x%x) (%d) \n",
			__func__, led_pwm2[1], led_pwm2[2], level);
		memset(&cmdreq, 0, sizeof(cmdreq));
		cmdreq.cmds = &tbyte_backlight_cmd;
		cmdreq.cmds_cnt = 1;
		cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
	} else {
		led_pwm1[1] = (unsigned char)level;
		pr_debug("%s:[DISPLAY] TM level(%x)\n", __func__, led_pwm1[1]);
		memset(&cmdreq, 0, sizeof(cmdreq));
		cmdreq.cmds = &backlight_cmd;
		cmdreq.cmds_cnt = 1;
		cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
	}
#else
	led_pwm1[1] = (unsigned char)level;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &backlight_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL | CMD_REQ_DCS;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
#endif

	if (ctrl->bklt_dcs_op_mode == DSI_HS_MODE)
		cmdreq.flags |= CMD_REQ_HS_MODE;
	else
		cmdreq.flags |= CMD_REQ_LP_MODE;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

static int mdss_dsi_request_gpios(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc = 0;

	if (gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		rc = gpio_request(ctrl_pdata->disp_en_gpio,
						"disp_enable");
		if (rc) {
			pr_err("request disp_en gpio failed, rc=%d\n",
				       rc);
			goto disp_en_gpio_err;
		}
	}
	rc = gpio_request(ctrl_pdata->rst_gpio, "disp_rst_n");
	if (rc) {
		pr_err("request reset gpio failed, rc=%d\n",
			rc);
		goto rst_gpio_err;
	}
	if (gpio_is_valid(ctrl_pdata->avdd_en_gpio)) {
		rc = gpio_request(ctrl_pdata->avdd_en_gpio,
						"avdd_enable");
		if (rc) {
			pr_err("request avdd_en gpio failed, rc=%d\n",
				       rc);
			goto avdd_en_gpio_err;
		}
	}
	if (gpio_is_valid(ctrl_pdata->lcd_mode_sel_gpio)) {
		rc = gpio_request(ctrl_pdata->lcd_mode_sel_gpio, "mode_sel");
		if (rc) {
			pr_err("request dsc/dual mode gpio failed,rc=%d\n",
								rc);
			goto lcd_mode_sel_gpio_err;
		}
	}

	return rc;

lcd_mode_sel_gpio_err:
	if (gpio_is_valid(ctrl_pdata->avdd_en_gpio))
		gpio_free(ctrl_pdata->avdd_en_gpio);
avdd_en_gpio_err:
	gpio_free(ctrl_pdata->rst_gpio);
rst_gpio_err:
	if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
		gpio_free(ctrl_pdata->disp_en_gpio);
disp_en_gpio_err:
	return rc;
}

int mdss_dsi_bl_gpio_ctrl(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int rc = 0, val = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (ctrl_pdata == NULL) {
		pr_err("%s: Invalid ctrl data\n", __func__);
		return -EINVAL;
	}

	/* if gpio is not valid */
	if (!gpio_is_valid(ctrl_pdata->bklt_en_gpio))
		return rc;

	pr_debug("%s: enable = %d\n", __func__, enable);

	/*
	 * if gpio state is false and enable (bl level) is
	 * non zero then toggle the gpio
	 */
	if (!ctrl_pdata->bklt_en_gpio_state && enable) {
		rc = gpio_request(ctrl_pdata->bklt_en_gpio, "bklt_enable");
		if (rc) {
			pr_err("request bklt gpio failed, rc=%d\n", rc);
			goto free;
		}

		if (ctrl_pdata->bklt_en_gpio_invert)
			val = 0;
		 else
			val = 1;

		rc = gpio_direction_output(ctrl_pdata->bklt_en_gpio, val);
		if (rc) {
			pr_err("%s: unable to set dir for bklt gpio val %d\n",
						__func__, val);
			goto free;
		}
		ctrl_pdata->bklt_en_gpio_state = true;
		goto ret;
	} else if (ctrl_pdata->bklt_en_gpio_state && !enable) {
		/*
		 * if gpio state is true and enable (bl level) is
		 * zero then toggle the gpio
		 */
		if (ctrl_pdata->bklt_en_gpio_invert)
			val = 1;
		 else
			val = 0;

		rc = gpio_direction_output(ctrl_pdata->bklt_en_gpio, val);
		if (rc)
			pr_err("%s: unable to set dir for bklt gpio val %d\n",
						__func__, val);
		goto free;
	}

	/* gpio state is true and bl level is non zero */
	goto ret;

free:
	pr_debug("%s: free bklt gpio\n", __func__);
	ctrl_pdata->bklt_en_gpio_state = false;
	gpio_free(ctrl_pdata->bklt_en_gpio);
ret:
	return rc;
}

int mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int i, rc = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pinfo = &(ctrl_pdata->panel_data.panel_info);
	if ((mdss_dsi_is_right_ctrl(ctrl_pdata) &&
		mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data)) ||
			pinfo->is_dba_panel) {
		pr_debug("%s:%d, right ctrl gpio configuration not needed\n",
			__func__, __LINE__);
		return rc;
	}

	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
	}

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return rc;
	}

	pr_debug("%s: enable = %d\n", __func__, enable);

	if (enable) {
		rc = mdss_dsi_request_gpios(ctrl_pdata);
		if (rc) {
			pr_err("gpio request failed\n");
			return rc;
		}
		if (!pinfo->cont_splash_enabled) {
			if (gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
				rc = gpio_direction_output(
					ctrl_pdata->disp_en_gpio, 1);
				if (rc) {
					pr_err("%s: unable to set dir for en gpio\n",
						__func__);
					goto exit;
				}
			}

			if (pdata->panel_info.rst_seq_len) {
				rc = gpio_direction_output(ctrl_pdata->rst_gpio,
					pdata->panel_info.rst_seq[0]);
				if (rc) {
					pr_err("%s: unable to set dir for rst gpio\n",
						__func__);
					goto exit;
				}
			}

			for (i = 0; i < pdata->panel_info.rst_seq_len; ++i) {
				gpio_set_value((ctrl_pdata->rst_gpio),
					pdata->panel_info.rst_seq[i]);
				if (pdata->panel_info.rst_seq[++i])
					usleep_range(pinfo->rst_seq[i] * 1000, pinfo->rst_seq[i] * 1000);
			}

			if (gpio_is_valid(ctrl_pdata->avdd_en_gpio)) {
				if (ctrl_pdata->avdd_en_gpio_invert) {
					rc = gpio_direction_output(
						ctrl_pdata->avdd_en_gpio, 0);
				} else {
					rc = gpio_direction_output(
						ctrl_pdata->avdd_en_gpio, 1);
				}
				if (rc) {
					pr_err("%s: unable to set dir for avdd_en gpio\n",
						__func__);
					goto exit;
				}
			}
		}

		if (gpio_is_valid(ctrl_pdata->lcd_mode_sel_gpio)) {
			bool out = false;

			if ((pinfo->mode_sel_state == MODE_SEL_SINGLE_PORT) ||
				(pinfo->mode_sel_state == MODE_GPIO_HIGH))
				out = true;
			else if ((pinfo->mode_sel_state == MODE_SEL_DUAL_PORT)
				|| (pinfo->mode_sel_state == MODE_GPIO_LOW))
				out = false;

			rc = gpio_direction_output(
					ctrl_pdata->lcd_mode_sel_gpio, out);
			if (rc) {
				pr_err("%s: unable to set dir for mode gpio\n",
					__func__);
				goto exit;
			}
		}

		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			pr_debug("%s: Panel Not properly turned OFF\n",
						__func__);
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
			pr_debug("%s: Reset panel done\n", __func__);
		}
	} else {
		if (gpio_is_valid(ctrl_pdata->avdd_en_gpio)) {
			if (ctrl_pdata->avdd_en_gpio_invert)
				gpio_set_value((ctrl_pdata->avdd_en_gpio), 1);
			else
				gpio_set_value((ctrl_pdata->avdd_en_gpio), 0);

			gpio_free(ctrl_pdata->avdd_en_gpio);
		}
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
			gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
			gpio_free(ctrl_pdata->disp_en_gpio);
		}
#ifndef ASUS_ZC600KL_PROJECT
		printk("[Display] panel reset disabling.\n");
		gpio_set_value((ctrl_pdata->rst_gpio), 0);

#else
		printk("[Display] skipped panel reset disabling. ZC600KL only.\n");
		//gpio_set_value((ctrl_pdata->rst_gpio), 0);
#endif

		gpio_free(ctrl_pdata->rst_gpio);

#ifdef ASUS_ZE620KL_PROJECT
		msleep(5);
#endif
		if (gpio_is_valid(ctrl_pdata->lcd_mode_sel_gpio)) {
			gpio_set_value(ctrl_pdata->lcd_mode_sel_gpio, 0);
			gpio_free(ctrl_pdata->lcd_mode_sel_gpio);
		}
	}

exit:
	return rc;
}

/**
 * mdss_dsi_roi_merge() -  merge two roi into single roi
 *
 * Function used by partial update with only one dsi intf take 2A/2B
 * (column/page) dcs commands.
 */
static int mdss_dsi_roi_merge(struct mdss_dsi_ctrl_pdata *ctrl,
					struct mdss_rect *roi)
{
	struct mdss_panel_info *l_pinfo;
	struct mdss_rect *l_roi;
	struct mdss_rect *r_roi;
	struct mdss_dsi_ctrl_pdata *other = NULL;
	int ans = 0;

	if (ctrl->ndx == DSI_CTRL_LEFT) {
		other = mdss_dsi_get_ctrl_by_index(DSI_CTRL_RIGHT);
		if (!other)
			return ans;
		l_pinfo = &(ctrl->panel_data.panel_info);
		l_roi = &(ctrl->panel_data.panel_info.roi);
		r_roi = &(other->panel_data.panel_info.roi);
	} else  {
		other = mdss_dsi_get_ctrl_by_index(DSI_CTRL_LEFT);
		if (!other)
			return ans;
		l_pinfo = &(other->panel_data.panel_info);
		l_roi = &(other->panel_data.panel_info.roi);
		r_roi = &(ctrl->panel_data.panel_info.roi);
	}

	if (l_roi->w == 0 && l_roi->h == 0) {
		/* right only */
		*roi = *r_roi;
		roi->x += l_pinfo->xres;/* add left full width to x-offset */
	} else {
		/* left only and left+righ */
		*roi = *l_roi;
		roi->w +=  r_roi->w; /* add right width */
		ans = 1;
	}

	return ans;
}

static char caset[] = {0x2a, 0x00, 0x00, 0x03, 0x00};	/* DTYPE_DCS_LWRITE */
static char paset[] = {0x2b, 0x00, 0x00, 0x05, 0x00};	/* DTYPE_DCS_LWRITE */

/*
 * Some panels can support multiple ROIs as part of the below commands
 */
static char caset_dual[] = {0x2a, 0x00, 0x00, 0x03, 0x00, 0x03,
				0x00, 0x00, 0x00, 0x00};/* DTYPE_DCS_LWRITE */
static char paset_dual[] = {0x2b, 0x00, 0x00, 0x05, 0x00, 0x03,
				0x00, 0x00, 0x00, 0x00};/* DTYPE_DCS_LWRITE */

/* pack into one frame before sent */
static struct dsi_cmd_desc set_col_page_addr_cmd[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(caset)}, caset},	/* packed */
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(paset)}, paset},
};

/* pack into one frame before sent */
static struct dsi_cmd_desc set_dual_col_page_addr_cmd[] = {	/*packed*/
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(caset_dual)}, caset_dual},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(paset_dual)}, paset_dual},
};


static void __mdss_dsi_send_col_page_addr(struct mdss_dsi_ctrl_pdata *ctrl,
		struct mdss_rect *roi, bool dual_roi)
{
	if (dual_roi) {
		struct mdss_rect *first, *second;

		first = &ctrl->panel_data.panel_info.dual_roi.first_roi;
		second = &ctrl->panel_data.panel_info.dual_roi.second_roi;

		caset_dual[1] = (((first->x) & 0xFF00) >> 8);
		caset_dual[2] = (((first->x) & 0xFF));
		caset_dual[3] = (((first->x - 1 + first->w) & 0xFF00) >> 8);
		caset_dual[4] = (((first->x - 1 + first->w) & 0xFF));
		/* skip the MPU setting byte*/
		caset_dual[6] = (((second->x) & 0xFF00) >> 8);
		caset_dual[7] = (((second->x) & 0xFF));
		caset_dual[8] = (((second->x - 1 + second->w) & 0xFF00) >> 8);
		caset_dual[9] = (((second->x - 1 + second->w) & 0xFF));
		set_dual_col_page_addr_cmd[0].payload = caset_dual;

		paset_dual[1] = (((first->y) & 0xFF00) >> 8);
		paset_dual[2] = (((first->y) & 0xFF));
		paset_dual[3] = (((first->y - 1 + first->h) & 0xFF00) >> 8);
		paset_dual[4] = (((first->y - 1 + first->h) & 0xFF));
		/* skip the MPU setting byte */
		paset_dual[6] = (((second->y) & 0xFF00) >> 8);
		paset_dual[7] = (((second->y) & 0xFF));
		paset_dual[8] = (((second->y - 1 + second->h) & 0xFF00) >> 8);
		paset_dual[9] = (((second->y - 1 + second->h) & 0xFF));
		set_dual_col_page_addr_cmd[1].payload = paset_dual;
	} else {
		caset[1] = (((roi->x) & 0xFF00) >> 8);
		caset[2] = (((roi->x) & 0xFF));
		caset[3] = (((roi->x - 1 + roi->w) & 0xFF00) >> 8);
		caset[4] = (((roi->x - 1 + roi->w) & 0xFF));
		set_col_page_addr_cmd[0].payload = caset;

		paset[1] = (((roi->y) & 0xFF00) >> 8);
		paset[2] = (((roi->y) & 0xFF));
		paset[3] = (((roi->y - 1 + roi->h) & 0xFF00) >> 8);
		paset[4] = (((roi->y - 1 + roi->h) & 0xFF));
		set_col_page_addr_cmd[1].payload = paset;
	}
	pr_debug("%s Sending 2A 2B cmnd with dual_roi=%d\n", __func__,
			dual_roi);

}
static void mdss_dsi_send_col_page_addr(struct mdss_dsi_ctrl_pdata *ctrl,
				struct mdss_rect *roi, int unicast)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_info *pinfo = &ctrl->panel_data.panel_info;
	bool dual_roi = pinfo->dual_roi.enabled;

	__mdss_dsi_send_col_page_addr(ctrl, roi, dual_roi);

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds_cnt = 2;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	if (unicast)
		cmdreq.flags |= CMD_REQ_UNICAST;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	/* Send default or dual roi 2A/2B cmd */
	cmdreq.cmds = dual_roi ? set_dual_col_page_addr_cmd :
		set_col_page_addr_cmd;
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

static int mdss_dsi_set_col_page_addr(struct mdss_panel_data *pdata,
		bool force_send)
{
	struct mdss_panel_info *pinfo;
	struct mdss_rect roi = {0};
	struct mdss_rect *p_roi;
	struct mdss_rect *c_roi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_dsi_ctrl_pdata *other = NULL;
	int left_or_both = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pinfo = &pdata->panel_info;
	p_roi = &pinfo->roi;

	/*
	 * to avoid keep sending same col_page info to panel,
	 * if roi_merge enabled, the roi of left ctrl is used
	 * to compare against new merged roi and saved new
	 * merged roi to it after comparing.
	 * if roi_merge disabled, then the calling ctrl's roi
	 * and pinfo's roi are used to compare.
	 */
	if (pinfo->partial_update_roi_merge) {
		left_or_both = mdss_dsi_roi_merge(ctrl, &roi);
		other = mdss_dsi_get_ctrl_by_index(DSI_CTRL_LEFT);
		c_roi = &other->roi;
	} else {
		c_roi = &ctrl->roi;
		roi = *p_roi;
	}

	/* roi had changed, do col_page update */
	if (force_send || !mdss_rect_cmp(c_roi, &roi)) {
		pr_debug("%s: ndx=%d x=%d y=%d w=%d h=%d\n",
				__func__, ctrl->ndx, p_roi->x,
				p_roi->y, p_roi->w, p_roi->h);

		*c_roi = roi; /* keep to ctrl */
		if (c_roi->w == 0 || c_roi->h == 0) {
			/* no new frame update */
			pr_debug("%s: ctrl=%d, no partial roi set\n",
						__func__, ctrl->ndx);
			return 0;
		}

		if (pinfo->dcs_cmd_by_left) {
			if (left_or_both && ctrl->ndx == DSI_CTRL_RIGHT) {
				/* 2A/2B sent by left already */
				return 0;
			}
		}

		if (!mdss_dsi_sync_wait_enable(ctrl)) {
			if (pinfo->dcs_cmd_by_left)
				ctrl = mdss_dsi_get_ctrl_by_index(
							DSI_CTRL_LEFT);
			mdss_dsi_send_col_page_addr(ctrl, &roi, 0);
		} else {
			/*
			 * when sync_wait_broadcast enabled,
			 * need trigger at right ctrl to
			 * start both dcs cmd transmission
			 */
			other = mdss_dsi_get_other_ctrl(ctrl);
			if (!other)
				goto end;

			if (mdss_dsi_is_left_ctrl(ctrl)) {
				if (pinfo->partial_update_roi_merge) {
					/*
					 * roi is the one after merged
					 * to dsi-1 only
					 */
					mdss_dsi_send_col_page_addr(other,
							&roi, 0);
				} else {
					mdss_dsi_send_col_page_addr(ctrl,
							&ctrl->roi, 1);
					mdss_dsi_send_col_page_addr(other,
							&other->roi, 1);
				}
			} else {
				if (pinfo->partial_update_roi_merge) {
					/*
					 * roi is the one after merged
					 * to dsi-1 only
					 */
					mdss_dsi_send_col_page_addr(ctrl,
							&roi, 0);
				} else {
					mdss_dsi_send_col_page_addr(other,
							&other->roi, 1);
					mdss_dsi_send_col_page_addr(ctrl,
							&ctrl->roi, 1);
				}
			}
		}
	}

end:
	return 0;
}

static int mdss_dsi_panel_apply_display_setting(struct mdss_panel_data *pdata,
							u32 mode)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct dsi_panel_cmds *lp_on_cmds;
	struct dsi_panel_cmds *lp_off_cmds;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	lp_on_cmds = &ctrl->lp_on_cmds;
	lp_off_cmds = &ctrl->lp_off_cmds;

	/* Apply display settings for low-persistence mode */
	if ((mode == MDSS_PANEL_LOW_PERSIST_MODE_ON) &&
			(lp_on_cmds->cmd_cnt))
		mdss_dsi_panel_apply_settings(ctrl, lp_on_cmds);
	else if ((mode == MDSS_PANEL_LOW_PERSIST_MODE_OFF) &&
			(lp_on_cmds->cmd_cnt))
		mdss_dsi_panel_apply_settings(ctrl, lp_off_cmds);
	else
		return -EINVAL;

	pr_debug("%s: Persistence mode %d applied\n", __func__, mode);
	return 0;
}

static void mdss_dsi_panel_switch_mode(struct mdss_panel_data *pdata,
							int mode)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mipi_panel_info *mipi;
	struct dsi_panel_cmds *pcmds;
	u32 flags = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	mipi  = &pdata->panel_info.mipi;

	if (!mipi->dms_mode)
		return;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (mipi->dms_mode != DYNAMIC_MODE_RESOLUTION_SWITCH_IMMEDIATE) {
		flags |= CMD_REQ_COMMIT;
		if (mode == SWITCH_TO_CMD_MODE)
			pcmds = &ctrl_pdata->video2cmd;
		else
			pcmds = &ctrl_pdata->cmd2video;
	} else if ((mipi->dms_mode ==
				DYNAMIC_MODE_RESOLUTION_SWITCH_IMMEDIATE)
			&& pdata->current_timing
			&& !list_empty(&pdata->timings_list)) {
		struct dsi_panel_timing *pt;

		pt = container_of(pdata->current_timing,
				struct dsi_panel_timing, timing);

		pr_debug("%s: sending switch commands\n", __func__);
		pcmds = &pt->switch_cmds;
		flags |= CMD_REQ_DMA_TPG;
		flags |= CMD_REQ_COMMIT;
	} else {
		pr_warn("%s: Invalid mode switch attempted\n", __func__);
		return;
	}

	if ((pdata->panel_info.compression_mode == COMPRESSION_DSC) &&
			(pdata->panel_info.send_pps_before_switch))
		mdss_dsi_panel_dsc_pps_send(ctrl_pdata, &pdata->panel_info);

	mdss_dsi_panel_cmds_send(ctrl_pdata, pcmds, flags);

	if ((pdata->panel_info.compression_mode == COMPRESSION_DSC) &&
			(!pdata->panel_info.send_pps_before_switch))
		mdss_dsi_panel_dsc_pps_send(ctrl_pdata, &pdata->panel_info);
}

#ifdef ASUS_ZC600KL_PROJECT
u32 g_last_bl = 0;
s32 g_wled_dimming_div = 6;
static void led_trigger_dim(int from, int to)
{
	int temp,temp1,lvl[g_wled_dimming_div+1];
	temp1 = (to-from) / g_wled_dimming_div;
	for (temp=1; temp<=g_wled_dimming_div; temp++)
	{
		lvl[temp] = from + ( temp1 * temp);
		//pr_debug("<===>lvl[%d]=%d,\n",temp,lvl[temp]);
		//pr_err("<===>lvl[%d]=%d,\n",temp,lvl[temp]);
		led_trigger_event(bl_led_trigger, lvl[temp]);
		msleep(1);
	}
	g_last_bl = lvl[g_wled_dimming_div];
}

int tp_suspend=0;

#else /* for ZE620KL/ZE554KL */
static void asus_lcd_led_trigger_dim(int from, int to)
{
	int temp;
	int wled_level = 0;

	for (temp = 0; temp <= g_wled_dimming_div; temp++) {
		wled_level = (WLED_MAX_LEVEL_ENABLE * (from * 10 + (to - from) * temp)) /\
				(g_bl_threshold * g_wled_dimming_div);
		led_trigger_event(bl_led_trigger, wled_level);
		msleep(10);
		pr_debug("%s: wled set to %d\n", __func__, wled_level);
	}
}
#endif

static void mdss_dsi_panel_bl_ctrl(struct mdss_panel_data *pdata,
							u32 bl_level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_dsi_ctrl_pdata *sctrl = NULL;

#ifndef ASUS_ZC600KL_PROJECT
	static bool wq = false;
#endif

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

#ifndef ASUS_ZC600KL_PROJECT
	spin_lock(&asus_lcd_bklt_spinlock);
	if (g_previous_bl == 0) {
		printk("[Display] %s: Turn on backlight from suspend\n", __func__);
		g_timer = true;
		wq = true;
	} else
		wq = false;
	g_previous_bl = bl_level;
	spin_unlock(&asus_lcd_bklt_spinlock);
	if (wq) {
		if (asus_lcd_display_commit_cnt == 5)
			queue_delayed_work(asus_lcd_delay_workqueue, &asus_lcd_delay_work, msecs_to_jiffies(16));
		else
			queue_delayed_work(asus_lcd_delay_workqueue, &asus_lcd_delay_work, 0);
	}

	if (g_timer)
		return;

	bl_level = g_update_bl; /* ASUS BSP Display, to fix workqueue's timing issue*/
#endif

	/*
	 * Some backlight controllers specify a minimum duty cycle
	 * for the backlight brightness. If the brightness is less
	 * than it, the controller can malfunction.
	 */
	pr_debug("%s: bl_level:%d\n", __func__, bl_level);

	/* do not allow backlight to change when panel in disable mode */
	if (pdata->panel_disable_mode && (bl_level != 0))
		return;

	if ((bl_level < pdata->panel_info.bl_min) && (bl_level != 0))
		bl_level = pdata->panel_info.bl_min;

	/* enable the backlight gpio if present */
	mdss_dsi_bl_gpio_ctrl(pdata, bl_level);

#ifndef ASUS_ZC600KL_PROJECT
	mutex_lock(&asus_lcd_bl_cmd_mutex);
#endif

	switch (ctrl_pdata->bklt_ctrl) {
	case BL_WLED:
#ifndef ASUS_ZC600KL_PROJECT
		led_trigger_event(bl_led_trigger, bl_level);
#else
		if (bl_level == 0) {
			led_trigger_event(bl_led_trigger, 0);
		} else {
			led_trigger_dim(g_last_bl, bl_level);
		}
#endif
		break;
	case BL_PWM:
		mdss_dsi_panel_bklt_pwm(ctrl_pdata, bl_level);
		break;
	case BL_DCS_CMD:
#ifndef ASUS_ZC600KL_PROJECT //for Ara/Titan
		if (bl_level == 0) {
			led_trigger_event(bl_led_trigger, WLED_MIN_LEVEL_DISABLE);
			pr_info("[Display] %s: g_bl_wled_enable ctrl disable\n", __func__);
			if (!mdss_dsi_sync_wait_enable(ctrl_pdata))
				mdss_dsi_panel_bklt_dcs(ctrl_pdata, 0);
			g_bl_wled_enable = false;
		} else if (bl_level < g_bl_threshold) { /*wled control*/
			if (g_last_bl == 0) {
				pr_info("[Display] %s: system resume set BL wled directly\n", __func__);
				led_trigger_event(bl_led_trigger, 4095*bl_level/g_bl_threshold);
				if (!mdss_dsi_sync_wait_enable(ctrl_pdata))
					mdss_dsi_panel_bklt_dcs(ctrl_pdata, g_bl_threshold);
			} else if (bl_level < g_last_bl) {
				if (!mdss_dsi_sync_wait_enable(ctrl_pdata) && g_bl_wled_enable)
					mdss_dsi_panel_bklt_dcs(ctrl_pdata, g_bl_threshold);
				asus_lcd_led_trigger_dim((g_last_bl >= g_bl_threshold)?g_bl_threshold:g_last_bl,bl_level);
			} else if (g_last_bl < bl_level) {
				asus_lcd_led_trigger_dim(g_last_bl,bl_level);
			} else
				pr_info("[Display] %s: Bypass backlight request with same level\n", __func__);
			g_bl_wled_enable = false;
		} else { /*dcs control*/
			if (!mdss_dsi_sync_wait_enable(ctrl_pdata)) {
				if (g_last_bl == 0) {
					led_trigger_event(bl_led_trigger, WLED_MAX_LEVEL_ENABLE);
					pr_info("[Display] %s: g_bl_wled_enable ctrl enable\n", __func__);
					mdss_dsi_panel_bklt_dcs(ctrl_pdata, bl_level);
				} else {
					mdss_dsi_panel_bklt_dcs(ctrl_pdata, bl_level);
					if (!g_bl_wled_enable)
						asus_lcd_led_trigger_dim(g_last_bl,g_bl_threshold);
				}
				g_bl_wled_enable = true;
				break;
			}
			/*
			 * DCS commands to update backlight are usually sent at
			 * the same time to both the controllers. However, if
			 * sync_wait is enabled, we need to ensure that the
			 * dcs commands are first sent to the non-trigger
			 * controller so that when the commands are triggered,
			 * both controllers receive it at the same time.
			 */
			sctrl = mdss_dsi_get_other_ctrl(ctrl_pdata);
			if (mdss_dsi_sync_wait_trigger(ctrl_pdata)) {
				if (sctrl)
					mdss_dsi_panel_bklt_dcs(sctrl, bl_level);
				mdss_dsi_panel_bklt_dcs(ctrl_pdata, bl_level);
			} else {
				mdss_dsi_panel_bklt_dcs(ctrl_pdata, bl_level);
				if (sctrl)
					mdss_dsi_panel_bklt_dcs(sctrl, bl_level);
			}
		}
#else //for ZC600KL
		if (!mdss_dsi_sync_wait_enable(ctrl_pdata)) {
			mdss_dsi_panel_bklt_dcs(ctrl_pdata, bl_level);
			break;
		}
		/*
		 * DCS commands to update backlight are usually sent at
		 * the same time to both the controllers. However, if
		 * sync_wait is enabled, we need to ensure that the
		 * dcs commands are first sent to the non-trigger
		 * controller so that when the commands are triggered,
		 * both controllers receive it at the same time.
		 */
		sctrl = mdss_dsi_get_other_ctrl(ctrl_pdata);
		if (mdss_dsi_sync_wait_trigger(ctrl_pdata)) {
			if (sctrl)
				mdss_dsi_panel_bklt_dcs(sctrl, bl_level);
			mdss_dsi_panel_bklt_dcs(ctrl_pdata, bl_level);
		} else {
			mdss_dsi_panel_bklt_dcs(ctrl_pdata, bl_level);
			if (sctrl)
				mdss_dsi_panel_bklt_dcs(sctrl, bl_level);
		}
#endif
		break;
	default:
		pr_err("%s: Unknown bl_ctrl configuration\n",
			__func__);
		break;
	}
#ifndef ASUS_ZC600KL_PROJECT
	g_last_bl = bl_level;
	mutex_unlock(&asus_lcd_bl_cmd_mutex);
#endif
}

static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo;
	struct dsi_panel_cmds *on_cmds;
	int ret = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

#ifdef CONFIG_POWERSUSPEND
	set_power_suspend_state_panel_hook(POWER_SUSPEND_INACTIVE);
#endif

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s: ndx=%d\n", __func__, ctrl->ndx);
	printk("[Display] %s: +++\n", __func__);

	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			goto end;
	}

#ifdef ASUS_ZE620KL_PROJECT
	if (asus_lcd_cabc_mode[1] == 0x0) {
		if (!asus_lcd_is_shipping())
			on_cmds = &ctrl->on_cmds_debug;
		else
			on_cmds = &ctrl->on_cmds;
	} else {
		printk("[Display] Panel on with CABC\n");
		if (!asus_lcd_is_shipping())
			on_cmds = &ctrl->cabc_on_cmds_debug;
		else
			on_cmds = &ctrl->cabc_on_cmds;
	}
#else
	on_cmds = &ctrl->on_cmds;
#endif

	if ((pinfo->mipi.dms_mode == DYNAMIC_MODE_SWITCH_IMMEDIATE) &&
			(pinfo->mipi.boot_mode != pinfo->mipi.mode))
		on_cmds = &ctrl->post_dms_on_cmds;

	pr_debug("%s: ndx=%d cmd_cnt=%d\n", __func__,
				ctrl->ndx, on_cmds->cmd_cnt);

	if (on_cmds->cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, on_cmds, CMD_REQ_COMMIT);

	if (pinfo->compression_mode == COMPRESSION_DSC)
		mdss_dsi_panel_dsc_pps_send(ctrl, pinfo);

	if (ctrl->ds_registered)
		mdss_dba_utils_video_on(pinfo->dba_data, pinfo);

	/* Ensure low persistence mode is set as before */
	mdss_dsi_panel_apply_display_setting(pdata, pinfo->persist_mode);

	if (pdata->event_handler)
		pdata->event_handler(pdata, MDSS_EVENT_UPDATE_LIVEDISPLAY,
				(void *)(unsigned long) MODE_UPDATE_ALL);
				
#ifdef ASUS_ZE620KL_PROJECT
	fts_ts_resume();
#endif

end:
	pr_debug("%s:-\n", __func__);
	printk("[Display] %s: ---\n", __func__);
	return ret;
}

static int mdss_dsi_post_panel_on(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo;
	struct dsi_panel_cmds *cmds;
	u32 vsync_period = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s: ctrl=%pK ndx=%d\n", __func__, ctrl, ctrl->ndx);
	printk("[Display] %s: +++\n", __func__);

	pinfo = &pdata->panel_info;
	if (pinfo->dcs_cmd_by_left && ctrl->ndx != DSI_CTRL_LEFT)
			goto end;

	cmds = &ctrl->post_panel_on_cmds;
	if (cmds->cmd_cnt) {
		msleep(VSYNC_DELAY);	/* wait for a vsync passed */
		mdss_dsi_panel_cmds_send(ctrl, cmds, CMD_REQ_COMMIT);
	}

	if (pinfo->is_dba_panel && pinfo->is_pluggable) {
		/* ensure at least 1 frame transfers to down stream device */
		vsync_period = (MSEC_PER_SEC / pinfo->mipi.frame_rate) + 1;
		msleep(vsync_period);
		mdss_dba_utils_hdcp_enable(pinfo->dba_data, true);
	}

end:
	pr_debug("%s:-\n", __func__);
	printk("[Display] %s: ---\n", __func__);
	return 0;
}

static int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s: ctrl=%pK ndx=%d\n", __func__, ctrl, ctrl->ndx);
	printk("[Display] %s: +++\n", __func__);

	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			goto end;
	}

	if (ctrl->off_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->off_cmds, CMD_REQ_COMMIT);

	if (ctrl->ds_registered && pinfo->is_pluggable) {
		mdss_dba_utils_video_off(pinfo->dba_data);
		mdss_dba_utils_hdcp_enable(pinfo->dba_data, false);
	}

#ifdef CONFIG_POWERSUSPEND
	set_power_suspend_state_panel_hook(POWER_SUSPEND_ACTIVE);
#endif

end:
	pr_debug("%s:-\n", __func__);
	printk("[Display] %s: ---\n", __func__);
	return 0;
}

static int mdss_dsi_panel_low_power_config(struct mdss_panel_data *pdata,
	int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s: ctrl=%pK ndx=%d enable=%d\n", __func__, ctrl, ctrl->ndx,
		enable);

	/* Any panel specific low power commands/config */

	pr_debug("%s:-\n", __func__);
	return 0;
}

static void mdss_dsi_parse_mdp_kickoff_threshold(struct device_node *np,
	struct mdss_panel_info *pinfo)
{
	int len, rc;
	const u32 *src;
	u32 tmp;
	u32 max_delay_us;

	pinfo->mdp_koff_thshold = false;
	src = of_get_property(np, "qcom,mdss-mdp-kickoff-threshold", &len);
	if (!src || (len == 0))
		return;

	rc = of_property_read_u32(np, "qcom,mdss-mdp-kickoff-delay", &tmp);
	if (!rc)
		pinfo->mdp_koff_delay = tmp;
	else
		return;

	if (pinfo->mipi.frame_rate == 0) {
		pr_err("cannot enable guard window, unexpected panel fps\n");
		return;
	}

	pinfo->mdp_koff_thshold_low = be32_to_cpu(src[0]);
	pinfo->mdp_koff_thshold_high = be32_to_cpu(src[1]);
	max_delay_us = 1000000 / pinfo->mipi.frame_rate;

	/* enable the feature if threshold is valid */
	if ((pinfo->mdp_koff_thshold_low < pinfo->mdp_koff_thshold_high) &&
	   ((pinfo->mdp_koff_delay > 0) ||
	    (pinfo->mdp_koff_delay < max_delay_us)))
		pinfo->mdp_koff_thshold = true;

	pr_debug("panel kickoff thshold:[%d, %d] delay:%d (max:%d) enable:%d\n",
		pinfo->mdp_koff_thshold_low,
		pinfo->mdp_koff_thshold_high,
		pinfo->mdp_koff_delay,
		max_delay_us,
		pinfo->mdp_koff_thshold);
}

//ASUS BSP Display, Panel debug control interface +++
#ifndef ASUS_ZC600KL_PROJECT //following interfaces are for Ara/Titan only

static void asus_lcd_delay_work_func(struct work_struct *ws)
{
	if (g_previous_bl) {
		if(g_mdss_first_boot) {
			printk("[Display] first boot detected.\n");
			asus_lcd_read_chip_unique_id();

			if (g_ftm_mode) {
				asus_tcon_cmd_set(asus_lcd_cabc_mode, ARRAY_SIZE(asus_lcd_cabc_mode));
			}
			g_mdss_first_boot--;

#ifdef ASUS_ZE620KL_PROJECT
			/* read Ara AUO panel HW ID and save it */
			asus_lcd_read_hw_id();
			if (!asus_lcd_is_shipping()) /* 1st MP = 0xA2, 2nd MP = 0x02 */
				pr_err("[Display] non shipping panel.\n");

			if (asus_lcd_is_second_source()) {
				g_asus_lcdID = ARA_LCD_AUO_2;
				qpnp_wled_fs_curr_ua_set(ARA_AUO_PANEL2_WLED_FS_CURR_UA);
			}
#endif
		}

		g_timer = false;
		//asus_tcon_cmd_set(asus_lcd_cabc_mode, ARRAY_SIZE(asus_lcd_cabc_mode));
		mdss_dsi_panel_bl_ctrl(g_mdss_pdata, g_previous_bl);
		msleep(32);
		asus_tcon_cmd_set(asus_lcd_dimming_cmd, ARRAY_SIZE(asus_lcd_dimming_cmd));
	}
}

void asus_tcon_cmd_set_unlocked(char *cmd, short len)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dsi_cmd_desc tcon_cmd = {
			{0x39, 1, 0, 0, 1, len}, cmd};

	ctrl_pdata = container_of(g_mdss_pdata, struct mdss_dsi_ctrl_pdata,
					panel_data);

	if (g_mdss_pdata->panel_info.panel_power_state == MDSS_PANEL_POWER_ON) {
		printk("[Display] write parameter: 0x%02x = 0x%02x\n", cmd[0], cmd[1]);
		memset(&cmdreq, 0, sizeof(cmdreq));
		cmdreq.cmds = &tcon_cmd;
		cmdreq.cmds_cnt = 1;
		cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);
	} else {
		pr_err("[Display] write parameter failed, asus_lcd_tcon_cmd_fence = %d\n", asus_lcd_tcon_cmd_fence);
	}
}

void asus_tcon_cmd_set(char *cmd, short len)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dsi_cmd_desc tcon_cmd = {
			{DTYPE_DCS_WRITE1, 1, 0, 0, 0, len}, cmd};

	if(len > 2)
		tcon_cmd.dchdr.dtype = DTYPE_GEN_LWRITE;

	ctrl_pdata = container_of(g_mdss_pdata, struct mdss_dsi_ctrl_pdata,
					panel_data);

	mutex_lock(&asus_lcd_tcon_cmd_mutex);

	if (!asus_lcd_tcon_cmd_fence && g_mdss_pdata->panel_info.panel_power_state == MDSS_PANEL_POWER_ON) {
		printk("[Display] write parameter: 0x%02x = 0x%02x\n", cmd[0], cmd[1]);
		memset(&cmdreq, 0, sizeof(cmdreq));
		cmdreq.cmds = &tcon_cmd;
		cmdreq.cmds_cnt = 1;
		cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);
	} else {
		pr_err("[Display] write parameter failed, asus_lcd_tcon_cmd_fence = %d\n", asus_lcd_tcon_cmd_fence);
	}
	mutex_unlock(&asus_lcd_tcon_cmd_mutex);
}
EXPORT_SYMBOL(asus_tcon_cmd_set);

void asus_tcon_cmd_get(char cmd, int rlen)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	char des_cmd[2] = {cmd, 0x00};
	struct dsi_cmd_desc tcon_cmd = {
			{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(des_cmd)}, des_cmd};
	char *rbuffer;
	char tmp[256];
	int i = 0;

	ctrl_pdata = container_of(g_mdss_pdata, struct mdss_dsi_ctrl_pdata,
					panel_data);

	mutex_lock(&asus_lcd_tcon_cmd_mutex);

	if (g_mdss_pdata->panel_info.panel_power_state == MDSS_PANEL_POWER_ON) {
		memset(&cmdreq, 0, sizeof(cmdreq));
		rbuffer = kmalloc(sizeof(ctrl_pdata->rx_buf.len), GFP_KERNEL);

		cmdreq.cmds = &tcon_cmd;
		cmdreq.cmds_cnt = 1;
		cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
		cmdreq.rlen = rlen;   //read back rlen byte
		cmdreq.rbuf = rbuffer;
		cmdreq.cb = NULL;  //fxn; /* call back */
		mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);

		for(i = 0; i < rlen; i++) {
			memset(tmp, 0, 256 * sizeof(char));
			pr_info("[Display] read parameter: 0x%02x = 0x%02x\n", des_cmd[0], *(cmdreq.rbuf+i));
			snprintf(tmp, sizeof(tmp), "0x%02x = 0x%02x\n", des_cmd[0], *(cmdreq.rbuf+i));
			strcat(g_reg_buffer,tmp);
		}
	} else {
		pr_err("[Display] read parameter failed\n");
	}

	mutex_unlock(&asus_lcd_tcon_cmd_mutex);
}

static ssize_t asus_lcd_cabc_proc_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
	char messages[256];

	memset(messages, 0, sizeof(messages));

	if (len > 256)
		len = 256;
	if (copy_from_user(messages, buff, len))
		return -EFAULT;

	if(strncmp(messages, "0", 1) == 0)  //off
		asus_lcd_cabc_mode[1] = 0x0;
	else if(strncmp(messages, "1", 1) == 0) //ui
		asus_lcd_cabc_mode[1] = 0x1;
	else if(strncmp(messages, "2", 1) == 0) //still
		asus_lcd_cabc_mode[1] = 0x2;
	else if(strncmp(messages, "3", 1) == 0) //moving
		asus_lcd_cabc_mode[1] = 0x3;

	asus_tcon_cmd_set(asus_lcd_cabc_mode, ARRAY_SIZE(asus_lcd_cabc_mode));

	return len;
}

static struct file_operations asus_lcd_cabc_proc_ops = {
	.write = asus_lcd_cabc_proc_write,
};

static ssize_t asus_lcd_reg_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
	char *messages, *tmp, *cur;
	char *token, *token_par;
	char *put_cmd;
	bool flag = 0;
	int *store;
	int i = 0, cnt = 0, cmd_cnt = 0;
	int ret = 0;
	uint8_t str_len = 0;

	messages = (char*) kmalloc(len*sizeof(char), GFP_KERNEL);
	if(!messages)
		return -EFAULT;

	tmp = (char*) kmalloc(len*sizeof(char), GFP_KERNEL);
	memset(tmp, 0, len*sizeof(char));
	store =  (int*) kmalloc((len/ASUS_LCD_REG_RW_MIN_LEN)*sizeof(int), GFP_KERNEL);
	put_cmd = (char*) kmalloc((len/ASUS_LCD_REG_RW_MIN_LEN)*sizeof(char), GFP_KERNEL);
	memset(g_reg_buffer, 0, 256*sizeof(char));

	if (copy_from_user(messages, buff, len)) {
		ret = -1;
		goto error;
	}
	cur = messages;
	*(cur+len-1) = '\0';

	pr_info("[Display] %s: (%s) +++\n", __func__, cur);

	if (strncmp(cur, "w", 1) == 0) //write
		flag = true;
	else if(strncmp(cur, "r", 1) == 0) //read
		flag = false;
	else {
		ret = -1;
		goto error;
	}

	while ((token = strsep(&cur, "wr")) != NULL) {
		str_len = strlen(token);

		if(str_len > 0) { /*filter zero length*/
			if(!(strncmp(token, ",", 1) == 0) || (str_len < ASUS_LCD_REG_RW_MAX_LEN)) {
				ret = -1;
				goto error;
			}

			memset(store, 0, (len/ASUS_LCD_REG_RW_MIN_LEN)*sizeof(int));
			memset(put_cmd, 0, (len/ASUS_LCD_REG_RW_MIN_LEN)*sizeof(char));
			cmd_cnt++;

			while ((token_par = strsep(&token, ",")) != NULL) {
				if(strlen(token_par) > ASUS_LCD_REG_RW_MIN_LEN) {
					ret = -1;
					goto error;
				}
				if(strlen(token_par)) {
					sscanf(token_par, "%x", &(store[cnt]));
					cnt++;
				}
			}

			for(i=0; i<cnt; i++)
				put_cmd[i] = store[i]&0xff;

			if(flag) {
				pr_info("[Display] write panel command\n");
				asus_tcon_cmd_set(put_cmd, cnt);
			}
			else {
				pr_info("[Display] read panel command\n");
				asus_tcon_cmd_get(put_cmd[0], store[1]);
			}

			if(cur != NULL) {
				if (*(tmp+str_len) == 'w')
					flag = true;
				else if (*(tmp+str_len) == 'r')
					flag = false;
			}
			cnt = 0;
		}

		memset(tmp, 0, len*sizeof(char));

		if(cur != NULL)
			strcpy(tmp, cur);

		msleep(10);
	}

	if(cmd_cnt == 0) {
		ret = -1;
		goto error;
	}

	ret = len;

error:
	pr_err("[Display] %s(%d)  --\n", __func__, ret);
	kfree(messages);
	kfree(tmp);
	kfree(store);
	kfree(put_cmd);
	return ret;
}

static ssize_t asus_lcd_reg_read(struct file *file, char __user *buf,
					size_t count, loff_t *ppos)
{
	int len = 0;
	ssize_t ret = 0;
	char *buff;

	buff = kmalloc(100, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	len += sprintf(buff, "%s\n", g_reg_buffer);
	ret = simple_read_from_buffer(buf, count, ppos, buff, len);
	kfree(buff);

	return ret;
}

static struct file_operations asus_lcd_reg_rw_ops = {
	.write = asus_lcd_reg_write,
	.read  = asus_lcd_reg_read,
};

//
// Read Unique ID / HW ID for each panel
//

#ifdef ASUS_ZE620KL_PROJECT
//read cmd for AUO panel
/* asus AUO panel page commands */
static char auo_enable_page_1[] = {0x00, 0x00};/* DTYPE_DCS_LWRITE */
static char auo_enable_page_2[] = {0xFF, 0x87, 0x16, 0x01};/* DTYPE_DCS_LWRITE */
static char auo_enable_page_3[] = {0x00, 0x80};/* DTYPE_DCS_LWRITE */
static char auo_enable_page_4[] = {0xFF, 0x87, 0x16};/* DTYPE_DCS_LWRITE */
static char auo_disable_page_1[] = {0xFF, 0x00, 0x00};/* DTYPE_DCS_LWRITE */
static char auo_disable_page_2[] = {0xFF, 0x00, 0x00, 0x00};/* DTYPE_DCS_LWRITE */

static struct dsi_cmd_desc auo_enable_page_cmd[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(auo_enable_page_1)}, auo_enable_page_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(auo_enable_page_2)}, auo_enable_page_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(auo_enable_page_3)}, auo_enable_page_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(auo_enable_page_4)}, auo_enable_page_4},
};

static struct dsi_cmd_desc auo_disable_page_cmd[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(auo_enable_page_3)}, auo_enable_page_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(auo_disable_page_1)}, auo_disable_page_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(auo_enable_page_1)}, auo_enable_page_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(auo_disable_page_2)}, auo_disable_page_2},
};

static char auo_id_cmd[] = {0xF3, 0x00};
static struct dsi_cmd_desc read_auo_id_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(auo_id_cmd)}, auo_id_cmd
};

static char auo_read_id_off[2] = {0x00, 0x94};

static char auo_hw_id_cmd1[] = {0xDA, 0x00};
static struct dsi_cmd_desc read_auo_hw_id_cmd1 = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(auo_hw_id_cmd1)}, auo_hw_id_cmd1
};
static char auo_hw_id_cmd2[] = {0xDB, 0x00};
static struct dsi_cmd_desc read_auo_hw_id_cmd2 = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(auo_hw_id_cmd2)}, auo_hw_id_cmd2
};
static char auo_hw_id_cmd3[] = {0xDC, 0x00};
static struct dsi_cmd_desc read_auo_hw_id_cmd3 = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(auo_hw_id_cmd3)}, auo_hw_id_cmd3
};

static void enable_auo_command_page(bool enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dcs_cmd_req cmdreq;

	ctrl_pdata = container_of(g_mdss_pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	printk("Display: %s AUO command page\n", enable ? "Enabling" : "Disabling");

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds_cnt = 4;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	cmdreq.cmds = enable ? auo_enable_page_cmd :
		auo_disable_page_cmd;
	mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);

	msleep(20);
}

void asus_lcd_read_hw_id(void)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dcs_cmd_req cmdreq;
	char *rbuffer;
	char panel_uniqueID[3] = {0};

	ctrl_pdata = container_of(g_mdss_pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	mutex_lock(&asus_lcd_tcon_cmd_mutex);
	memset(&cmdreq, 0, sizeof(cmdreq));
	rbuffer = kmalloc(sizeof(ctrl_pdata->rx_buf.len), GFP_KERNEL);

	cmdreq.cmds = &read_auo_hw_id_cmd1;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT | CMD_REQ_HS_MODE;
	cmdreq.rlen = 1;
	cmdreq.rbuf = rbuffer;
	cmdreq.cb = NULL;
	mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);
	panel_uniqueID[0] = *(cmdreq.rbuf);

	cmdreq.cmds = &read_auo_hw_id_cmd2;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT | CMD_REQ_HS_MODE;
	cmdreq.rlen = 1;
	cmdreq.rbuf = rbuffer;
	cmdreq.cb = NULL;
	mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);
	panel_uniqueID[1] = *(cmdreq.rbuf);

	cmdreq.cmds = &read_auo_hw_id_cmd3;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT | CMD_REQ_HS_MODE;
	cmdreq.rlen = 1;
	cmdreq.rbuf = rbuffer;
	cmdreq.cb = NULL;
	mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);
	panel_uniqueID[2] = *(cmdreq.rbuf);

	mutex_unlock(&asus_lcd_tcon_cmd_mutex);
	kfree(rbuffer);
	pr_info("[Display] AUO HW ID: 0x%x, 0x%x, 0x%x\n",
		*(panel_uniqueID), *(panel_uniqueID+1), *(panel_uniqueID+2));

	asus_lcd_hw_id_0xDA = panel_uniqueID[0];
	asus_lcd_hw_id_0xDC = panel_uniqueID[2];
}

bool asus_lcd_is_shipping(void)
{
	bool ret = true;
	if (asus_lcd_hw_id_0xDA == 0x61) {
		if (asus_lcd_hw_id_0xDC == 0xA2) {
			printk("[Display] This is 1st source panel: MP stage\n");
			ret = true;
		} else {
			printk("[Display] This is 1st source panel: ER2/PR stage\n");
			ret = false;
		}
	} else if (asus_lcd_hw_id_0xDA == 0x65) {
		if (asus_lcd_hw_id_0xDC == 0x02) {
			printk("[Display] This is 2nd source panel: MP stage\n");
			ret = true;
		} else {
			printk("[Display] This is 2nd source panel: 1st Sample\n");
			ret = false;
		}
	}
	return ret;
}

bool asus_lcd_is_second_source(void)
{
	if (asus_lcd_hw_id_0xDA == 0x65)
		return true;
	else
		return false;
}

#endif

static u32 asus_lcd_read_chip_unique_id(void)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dcs_cmd_req cmdreq;
	char *rbuffer;
	char panel_uniqueID[4] = {0};

	ctrl_pdata = container_of(g_mdss_pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	mutex_lock(&asus_lcd_tcon_cmd_mutex);
	memset(&cmdreq, 0, sizeof(cmdreq));
	rbuffer = kmalloc(sizeof(ctrl_pdata->rx_buf.len), GFP_KERNEL);

	switch (g_asus_lcdID) {

#ifdef ASUS_ZE620KL_PROJECT
	case ARA_LCD_AUO:
	case ARA_LCD_AUO_2:
		// first enable page 2 command
		enable_auo_command_page(true);

		// write read id offset
		asus_tcon_cmd_set_unlocked(auo_read_id_off, ARRAY_SIZE(auo_read_id_off));

		cmdreq.cmds = &read_auo_id_cmd;
		cmdreq.cmds_cnt = 1;
		cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT | CMD_REQ_HS_MODE;
		cmdreq.rlen = 5;
		cmdreq.rbuf = rbuffer;
		cmdreq.cb = NULL;

		mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);
		msleep(100);

		printk("[Display] AUO ID read: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n", *(cmdreq.rbuf), *(cmdreq.rbuf+1), *(cmdreq.rbuf+2), *(cmdreq.rbuf+3), *(cmdreq.rbuf+4));
		panel_uniqueID[0] = *(cmdreq.rbuf) ^ *(cmdreq.rbuf+3);
		panel_uniqueID[1] = *(cmdreq.rbuf+1);
		panel_uniqueID[2] = *(cmdreq.rbuf+2);
		panel_uniqueID[3] = *(cmdreq.rbuf+4);
		printk("[Display] AUO Unique ID: 0x%x, 0x%x, 0x%x, 0x%x\n", *(panel_uniqueID), *(panel_uniqueID+1), *(panel_uniqueID+2), *(panel_uniqueID+3));

		memset(&cmdreq, 0, sizeof(cmdreq));

		// disable page 2 command
		enable_auo_command_page(false);

		sprintf(lcd_unique_id, "%02x%02x%02x%02x", panel_uniqueID[0], panel_uniqueID[1], panel_uniqueID[2], panel_uniqueID[3]);
		break;
#endif
#ifdef ASUS_ZC600KL_PROJECT
	case ZC600KL_LCD_TD4310:
	case ZC600KL_LCD_NT36672:
		printk("[Display] ZC600KL does not support unique ID read.\n");
		break;
#endif

	default:
		pr_err("[Display] unknow lcd id(%d), skip reading unique ID. Invalid %p\n", g_asus_lcdID, panel_uniqueID);
		break;
	}

	mutex_unlock(&asus_lcd_tcon_cmd_mutex);
	kfree(rbuffer);

	return 0;
}

static ssize_t asus_lcd_unique_id_proc_read(struct file *file, char __user *buf,
			size_t count, loff_t *ppos)
{
	int len = 0;
	ssize_t ret = 0;
	char *buff;

	buff = kmalloc(100, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	if (0)
		asus_lcd_read_chip_unique_id();

	if (*lcd_unique_id == '\0')
		len += sprintf(buff, "%s\n", "bdeeeafd");
	else
		len += sprintf(buff, "%s\n", lcd_unique_id);

	ret = simple_read_from_buffer(buf, count, ppos, buff, len);
	kfree(buff);

	return ret;
}

static struct file_operations asus_lcd_unique_id_proc_ops = {
	.read = asus_lcd_unique_id_proc_read,
};

static ssize_t asus_lcd_id_proc_read(struct file *file, char __user *buf,
                    size_t count, loff_t *ppos)
{
	int len = 0;
	ssize_t ret = 0;
	char *buff;

	buff = kmalloc(100, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	len += sprintf(buff, "%d\n", g_asus_lcdID);

	ret = simple_read_from_buffer(buf, count, ppos, buff, len);
	kfree(buff);

	return ret;
}

static struct file_operations asus_lcd_id_proc_ops = {
	.read = asus_lcd_id_proc_read,
};

#endif
//ASUS BSP Display, Panel control interfaces ---

static void mdss_dsi_parse_trigger(struct device_node *np, char *trigger,
		char *trigger_key)
{
	const char *data;

	*trigger = DSI_CMD_TRIGGER_SW;
	data = of_get_property(np, trigger_key, NULL);
	if (data) {
		if (!strcmp(data, "none"))
			*trigger = DSI_CMD_TRIGGER_NONE;
		else if (!strcmp(data, "trigger_te"))
			*trigger = DSI_CMD_TRIGGER_TE;
		else if (!strcmp(data, "trigger_sw_seof"))
			*trigger = DSI_CMD_TRIGGER_SW_SEOF;
		else if (!strcmp(data, "trigger_sw_te"))
			*trigger = DSI_CMD_TRIGGER_SW_TE;
	}
}


static int mdss_dsi_parse_dcs_cmds(struct device_node *np,
		struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key)
{
	const char *data;
	int blen = 0, len;
	char *buf, *bp;
	struct dsi_ctrl_hdr *dchdr;
	int i, cnt;

	data = of_get_property(np, cmd_key, &blen);
	if (!data) {
		pr_err("%s: failed, key=%s\n", __func__, cmd_key);
		return -ENOMEM;
	}

	buf = kzalloc(sizeof(char) * blen, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy(buf, data, blen);

	/* scan dcs commands */
	bp = buf;
	len = blen;
	cnt = 0;
	while (len >= sizeof(*dchdr)) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		dchdr->dlen = ntohs(dchdr->dlen);
		if (dchdr->dlen > len) {
			pr_err("%s: dtsi cmd=%x error, len=%d",
				__func__, dchdr->dtype, dchdr->dlen);
			goto exit_free;
		}
		bp += sizeof(*dchdr);
		len -= sizeof(*dchdr);
		bp += dchdr->dlen;
		len -= dchdr->dlen;
		cnt++;
	}

	if (len != 0) {
		pr_err("%s: dcs_cmd=%x len=%d error!",
				__func__, buf[0], blen);
		goto exit_free;
	}

	pcmds->cmds = kzalloc(cnt * sizeof(struct dsi_cmd_desc),
						GFP_KERNEL);
	if (!pcmds->cmds)
		goto exit_free;

	pcmds->cmd_cnt = cnt;
	pcmds->buf = buf;
	pcmds->blen = blen;

	bp = buf;
	len = blen;
	for (i = 0; i < cnt; i++) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		len -= sizeof(*dchdr);
		bp += sizeof(*dchdr);
		pcmds->cmds[i].dchdr = *dchdr;
		pcmds->cmds[i].payload = bp;
		bp += dchdr->dlen;
		len -= dchdr->dlen;
	}

	/*Set default link state to LP Mode*/
	pcmds->link_state = DSI_LP_MODE;

	if (link_key) {
		data = of_get_property(np, link_key, NULL);
		if (data && !strcmp(data, "dsi_hs_mode"))
			pcmds->link_state = DSI_HS_MODE;
		else
			pcmds->link_state = DSI_LP_MODE;
	}

	pr_debug("%s: dcs_cmd=%x len=%d, cmd_cnt=%d link_state=%d\n", __func__,
		pcmds->buf[0], pcmds->blen, pcmds->cmd_cnt, pcmds->link_state);

	return 0;

exit_free:
	kfree(buf);
	return -ENOMEM;
}


int mdss_panel_get_dst_fmt(u32 bpp, char mipi_mode, u32 pixel_packing,
				char *dst_format)
{
	int rc = 0;
	switch (bpp) {
	case 3:
		*dst_format = DSI_CMD_DST_FORMAT_RGB111;
		break;
	case 8:
		*dst_format = DSI_CMD_DST_FORMAT_RGB332;
		break;
	case 12:
		*dst_format = DSI_CMD_DST_FORMAT_RGB444;
		break;
	case 16:
		switch (mipi_mode) {
		case DSI_VIDEO_MODE:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB565;
			break;
		case DSI_CMD_MODE:
			*dst_format = DSI_CMD_DST_FORMAT_RGB565;
			break;
		default:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB565;
			break;
		}
		break;
	case 18:
		switch (mipi_mode) {
		case DSI_VIDEO_MODE:
			if (pixel_packing == 0)
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666;
			else
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666_LOOSE;
			break;
		case DSI_CMD_MODE:
			*dst_format = DSI_CMD_DST_FORMAT_RGB666;
			break;
		default:
			if (pixel_packing == 0)
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666;
			else
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666_LOOSE;
			break;
		}
		break;
	case 24:
		switch (mipi_mode) {
		case DSI_VIDEO_MODE:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
			break;
		case DSI_CMD_MODE:
			*dst_format = DSI_CMD_DST_FORMAT_RGB888;
			break;
		default:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
			break;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int mdss_dsi_parse_fbc_params(struct device_node *np,
			struct mdss_panel_timing *timing)
{
	int rc, fbc_enabled = 0;
	u32 tmp;
	struct fbc_panel_info *fbc = &timing->fbc;

	fbc_enabled = of_property_read_bool(np,	"qcom,mdss-dsi-fbc-enable");
	if (fbc_enabled) {
		pr_debug("%s:%d FBC panel enabled.\n", __func__, __LINE__);
		fbc->enabled = 1;
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-bpp", &tmp);
		fbc->target_bpp = (!rc ? tmp : 24);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-packing",
				&tmp);
		fbc->comp_mode = (!rc ? tmp : 0);
		fbc->qerr_enable = of_property_read_bool(np,
			"qcom,mdss-dsi-fbc-quant-error");
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-bias", &tmp);
		fbc->cd_bias = (!rc ? tmp : 0);
		fbc->pat_enable = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-pat-mode");
		fbc->vlc_enable = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-vlc-mode");
		fbc->bflc_enable = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-bflc-mode");
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-h-line-budget",
				&tmp);
		fbc->line_x_budget = (!rc ? tmp : 0);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-budget-ctrl",
				&tmp);
		fbc->block_x_budget = (!rc ? tmp : 0);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-block-budget",
				&tmp);
		fbc->block_budget = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-lossless-threshold", &tmp);
		fbc->lossless_mode_thd = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-lossy-threshold", &tmp);
		fbc->lossy_mode_thd = (!rc ? tmp : 0);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-rgb-threshold",
				&tmp);
		fbc->lossy_rgb_thd = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-lossy-mode-idx", &tmp);
		fbc->lossy_mode_idx = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-slice-height", &tmp);
		fbc->slice_height = (!rc ? tmp : 0);
		fbc->pred_mode = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-2d-pred-mode");
		fbc->enc_mode = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-ver2-mode");
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-max-pred-err", &tmp);
		fbc->max_pred_err = (!rc ? tmp : 0);

		timing->compression_mode = COMPRESSION_FBC;
	} else {
		pr_debug("%s:%d Panel does not support FBC.\n",
				__func__, __LINE__);
		fbc->enabled = 0;
		fbc->target_bpp = 24;
	}
	return 0;
}

void mdss_dsi_panel_dsc_pps_send(struct mdss_dsi_ctrl_pdata *ctrl,
				struct mdss_panel_info *pinfo)
{
	struct dsi_panel_cmds pcmds;
	struct dsi_cmd_desc cmd;

	if (!pinfo || (pinfo->compression_mode != COMPRESSION_DSC))
		return;

	memset(&pcmds, 0, sizeof(pcmds));
	memset(&cmd, 0, sizeof(cmd));

	cmd.dchdr.dlen = mdss_panel_dsc_prepare_pps_buf(&pinfo->dsc,
		ctrl->pps_buf, 0);
	cmd.dchdr.dtype = DTYPE_PPS;
	cmd.dchdr.last = 1;
	cmd.dchdr.wait = 10;
	cmd.dchdr.vc = 0;
	cmd.dchdr.ack = 0;
	cmd.payload = ctrl->pps_buf;

	pcmds.cmd_cnt = 1;
	pcmds.cmds = &cmd;
	pcmds.link_state = DSI_LP_MODE;

	mdss_dsi_panel_cmds_send(ctrl, &pcmds, CMD_REQ_COMMIT);
}

static int mdss_dsi_parse_hdr_settings(struct device_node *np,
		struct mdss_panel_info *pinfo)
{
	int rc = 0;
	struct mdss_panel_hdr_properties *hdr_prop;

	if (!np) {
		pr_err("%s: device node pointer is NULL\n", __func__);
		return -EINVAL;
	}

	if (!pinfo) {
		pr_err("%s: panel info is NULL\n", __func__);
		return -EINVAL;
	}

	hdr_prop = &pinfo->hdr_properties;
	hdr_prop->hdr_enabled = of_property_read_bool(np,
		"qcom,mdss-dsi-panel-hdr-enabled");

	if (hdr_prop->hdr_enabled) {
		rc = of_property_read_u32_array(np,
				"qcom,mdss-dsi-panel-hdr-color-primaries",
				hdr_prop->display_primaries,
				DISPLAY_PRIMARIES_COUNT);
		if (rc) {
			pr_info("%s:%d, Unable to read color primaries,rc:%u",
					__func__, __LINE__,
					hdr_prop->hdr_enabled = false);
			}

		rc = of_property_read_u32(np,
			"qcom,mdss-dsi-panel-peak-brightness",
			&(hdr_prop->peak_brightness));
		if (rc) {
			pr_info("%s:%d, Unable to read hdr brightness, rc:%u",
				__func__, __LINE__, rc);
			hdr_prop->hdr_enabled = false;
		}

		rc = of_property_read_u32(np,
			"qcom,mdss-dsi-panel-blackness-level",
			&(hdr_prop->blackness_level));
		if (rc) {
			pr_info("%s:%d, Unable to read hdr brightness, rc:%u",
				__func__, __LINE__, rc);
			hdr_prop->hdr_enabled = false;
		}
	}
	return 0;
}

static int mdss_dsi_parse_split_link_settings(struct device_node *np,
		struct mdss_panel_info *pinfo)
{
	u32 tmp;
	int rc = 0;

	if (!np) {
		pr_err("%s: device node pointer is NULL\n", __func__);
		return -EINVAL;
	}

	if (!pinfo) {
		pr_err("%s: panel info is NULL\n", __func__);
		return -EINVAL;
	}

	pinfo->split_link_enabled = of_property_read_bool(np,
		"qcom,split-link-enabled");

	if (pinfo->split_link_enabled) {
		rc = of_property_read_u32(np,
			"qcom,sublinks-count", &tmp);
		/* default num of sublink is 1*/
		pinfo->mipi.num_of_sublinks = (!rc ? tmp : 1);

		rc = of_property_read_u32(np,
			"qcom,lanes-per-sublink", &tmp);
		/* default num of lanes per sublink is 1 */
		pinfo->mipi.lanes_per_sublink = (!rc ? tmp : 1);
	}

	pr_info("%s: enable %d sublinks-count %d lanes per sublink %d\n",
		__func__, pinfo->split_link_enabled,
		pinfo->mipi.num_of_sublinks,
		pinfo->mipi.lanes_per_sublink);
	return 0;
}

static int mdss_dsi_parse_dsc_version(struct device_node *np,
		struct mdss_panel_timing *timing)
{
	u32 data;
	int rc = 0;
	struct dsc_desc *dsc = &timing->dsc;

	rc = of_property_read_u32(np, "qcom,mdss-dsc-version", &data);
	if (rc) {
		dsc->version = 0x11;
		rc = 0;
	} else {
		dsc->version = data & 0xff;
		/* only support DSC 1.1 rev */
		if (dsc->version != 0x11) {
			pr_err("%s: DSC version:%d not supported\n", __func__,
				dsc->version);
			rc = -EINVAL;
			goto end;
		}
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsc-scr-version", &data);
	if (rc) {
		dsc->scr_rev = 0x0;
		rc = 0;
	} else {
		dsc->scr_rev = data & 0xff;
		/* only one scr rev supported */
		if (dsc->scr_rev > 0x1) {
			pr_err("%s: DSC scr version:%d not supported\n",
				__func__, dsc->scr_rev);
			rc = -EINVAL;
			goto end;
		}
	}

end:
	return rc;
}

static int mdss_dsi_parse_dsc_params(struct device_node *np,
		struct mdss_panel_timing *timing, bool is_split_display)
{
	u32 data, intf_width;
	int rc = 0;
	struct dsc_desc *dsc = &timing->dsc;

	if (!np) {
		pr_err("%s: device node pointer is NULL\n", __func__);
		return -EINVAL;
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsc-encoders", &data);
	if (rc) {
		if (!of_find_property(np, "qcom,mdss-dsc-encoders", NULL)) {
			/* property is not defined, default to 1 */
			data = 1;
		} else {
			pr_err("%s: Error parsing qcom,mdss-dsc-encoders\n",
				__func__);
			goto end;
		}
	}

	timing->dsc_enc_total = data;

	if (is_split_display && (timing->dsc_enc_total > 1)) {
		pr_err("%s: Error: for split displays, more than 1 dsc encoder per panel is not allowed.\n",
			__func__);
		goto end;
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsc-slice-height", &data);
	if (rc)
		goto end;
	dsc->slice_height = data;

	rc = of_property_read_u32(np, "qcom,mdss-dsc-slice-width", &data);
	if (rc)
		goto end;
	dsc->slice_width = data;
	intf_width = timing->xres;

	if (intf_width % dsc->slice_width) {
		pr_err("%s: Error: multiple of slice-width:%d should match panel-width:%d\n",
			__func__, dsc->slice_width, intf_width);
		goto end;
	}

	data = intf_width / dsc->slice_width;
	if (((timing->dsc_enc_total > 1) && ((data != 2) && (data != 4))) ||
	    ((timing->dsc_enc_total == 1) && (data > 2))) {
		pr_err("%s: Error: max 2 slice per encoder. slice-width:%d should match panel-width:%d dsc_enc_total:%d\n",
			__func__, dsc->slice_width,
			intf_width, timing->dsc_enc_total);
		goto end;
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsc-slice-per-pkt", &data);
	if (rc)
		goto end;
	dsc->slice_per_pkt = data;

	/*
	 * slice_per_pkt can be either 1 or all slices_per_intf
	 */
	if ((dsc->slice_per_pkt > 1) && (dsc->slice_per_pkt !=
			DIV_ROUND_UP(intf_width, dsc->slice_width))) {
		pr_err("Error: slice_per_pkt can be either 1 or all slices_per_intf\n");
		pr_err("%s: slice_per_pkt=%d, slice_width=%d intf_width=%d\n",
			__func__,
			dsc->slice_per_pkt, dsc->slice_width, intf_width);
		rc = -EINVAL;
		goto end;
	}

	pr_debug("%s: num_enc:%d :slice h=%d w=%d s_pkt=%d\n", __func__,
		timing->dsc_enc_total, dsc->slice_height,
		dsc->slice_width, dsc->slice_per_pkt);

	rc = of_property_read_u32(np, "qcom,mdss-dsc-bit-per-component", &data);
	if (rc)
		goto end;
	dsc->bpc = data;

	rc = of_property_read_u32(np, "qcom,mdss-dsc-bit-per-pixel", &data);
	if (rc)
		goto end;
	dsc->bpp = data;

	pr_debug("%s: bpc=%d bpp=%d\n", __func__,
		dsc->bpc, dsc->bpp);

	dsc->block_pred_enable = of_property_read_bool(np,
			"qcom,mdss-dsc-block-prediction-enable");

	dsc->enable_422 = 0;
	dsc->convert_rgb = 1;
	dsc->vbr_enable = 0;

	dsc->config_by_manufacture_cmd = of_property_read_bool(np,
		"qcom,mdss-dsc-config-by-manufacture-cmd");

	mdss_panel_dsc_parameters_calc(&timing->dsc);
	mdss_panel_dsc_pclk_param_calc(&timing->dsc, intf_width);

	timing->dsc.full_frame_slices =
		DIV_ROUND_UP(intf_width, timing->dsc.slice_width);

	timing->compression_mode = COMPRESSION_DSC;

end:
	return rc;
}

static struct device_node *mdss_dsi_panel_get_dsc_cfg_np(
		struct device_node *np, struct mdss_panel_data *panel_data,
		bool default_timing)
{
	struct device_node *dsc_cfg_np = NULL;


	/* Read the dsc config node specified by command line */
	if (default_timing) {
		dsc_cfg_np = of_get_child_by_name(np,
				panel_data->dsc_cfg_np_name);
		if (!dsc_cfg_np)
			pr_warn_once("%s: cannot find dsc config node:%s\n",
				__func__, panel_data->dsc_cfg_np_name);
	}

	/*
	 * Fall back to default from DT as nothing is specified
	 * in command line.
	 */
	if (!dsc_cfg_np && of_find_property(np, "qcom,config-select", NULL)) {
		dsc_cfg_np = of_parse_phandle(np, "qcom,config-select", 0);
		if (!dsc_cfg_np)
			pr_warn_once("%s:err parsing qcom,config-select\n",
					__func__);
	}

	return dsc_cfg_np;
}

static int mdss_dsi_parse_topology_config(struct device_node *np,
	struct dsi_panel_timing *pt, struct mdss_panel_data *panel_data,
	bool default_timing)
{
	int rc = 0;
	bool is_split_display = panel_data->panel_info.is_split_display;
	const char *data;
	struct mdss_panel_timing *timing = &pt->timing;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo;
	struct device_node *cfg_np = NULL;

	ctrl_pdata = container_of(panel_data, struct mdss_dsi_ctrl_pdata,
							panel_data);
	pinfo = &ctrl_pdata->panel_data.panel_info;

	cfg_np = mdss_dsi_panel_get_dsc_cfg_np(np,
				&ctrl_pdata->panel_data, default_timing);

	if (cfg_np) {
		if (!of_property_read_u32_array(cfg_np, "qcom,lm-split",
		    timing->lm_widths, 2)) {
			if (mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data)
			    && (timing->lm_widths[1] != 0)) {
				pr_err("%s: lm-split not allowed with split display\n",
					__func__);
				rc = -EINVAL;
				goto end;
			}
		}

		if (!of_property_read_string(cfg_np, "qcom,split-mode",
		    &data) && !strcmp(data, "pingpong-split"))
			pinfo->use_pingpong_split = true;

		if (((timing->lm_widths[0]) || (timing->lm_widths[1])) &&
		    pinfo->use_pingpong_split) {
			pr_err("%s: pingpong_split cannot be used when lm-split[%d,%d] is specified\n",
				__func__,
				timing->lm_widths[0], timing->lm_widths[1]);
			return -EINVAL;
		}

		pr_info("%s: cfg_node name %s lm_split:%dx%d pp_split:%s\n",
			__func__, cfg_np->name,
			timing->lm_widths[0], timing->lm_widths[1],
			pinfo->use_pingpong_split ? "yes" : "no");
	}

	if (!pinfo->use_pingpong_split &&
	    (timing->lm_widths[0] == 0) && (timing->lm_widths[1] == 0))
		timing->lm_widths[0] = pt->timing.xres;

	data = of_get_property(np, "qcom,compression-mode", NULL);
	if (data) {
		if (cfg_np && !strcmp(data, "dsc")) {
			rc = mdss_dsi_parse_dsc_version(np, &pt->timing);
			if (rc)
				goto end;

			pinfo->send_pps_before_switch =
				of_property_read_bool(np,
				"qcom,mdss-dsi-send-pps-before-switch");

			rc = mdss_dsi_parse_dsc_params(cfg_np, &pt->timing,
					is_split_display);
		} else if (!strcmp(data, "fbc")) {
			rc = mdss_dsi_parse_fbc_params(np, &pt->timing);
		}
	}

end:
	of_node_put(cfg_np);
	return rc;
}

static void mdss_panel_parse_te_params(struct device_node *np,
		struct mdss_panel_timing *timing)
{
	struct mdss_mdp_pp_tear_check *te = &timing->te;
	u32 tmp;
	int rc = 0;
	/*
	 * TE default: dsi byte clock calculated base on 70 fps;
	 * around 14 ms to complete a kickoff cycle if te disabled;
	 * vclk_line base on 60 fps; write is faster than read;
	 * init == start == rdptr;
	 */
	te->tear_check_en =
		!of_property_read_bool(np, "qcom,mdss-tear-check-disable");
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-sync-cfg-height", &tmp);
	te->sync_cfg_height = (!rc ? tmp : 0xfff0);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-sync-init-val", &tmp);
	te->vsync_init_val = (!rc ? tmp : timing->yres);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-sync-threshold-start", &tmp);
	te->sync_threshold_start = (!rc ? tmp : 4);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-sync-threshold-continue", &tmp);
	te->sync_threshold_continue = (!rc ? tmp : 4);
	rc = of_property_read_u32(np, "qcom,mdss-tear-check-frame-rate", &tmp);
	te->refx100 = (!rc ? tmp : 6000);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-start-pos", &tmp);
	te->start_pos = (!rc ? tmp : timing->yres);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-rd-ptr-trigger-intr", &tmp);
	te->rd_ptr_irq = (!rc ? tmp : timing->yres + 1);
	te->wr_ptr_irq = 0;
}


static int mdss_dsi_parse_reset_seq(struct device_node *np,
		u32 rst_seq[MDSS_DSI_RST_SEQ_LEN], u32 *rst_len,
		const char *name)
{
	int num = 0, i;
	int rc;
	struct property *data;
	u32 tmp[MDSS_DSI_RST_SEQ_LEN];
	*rst_len = 0;
	data = of_find_property(np, name, &num);
	num /= sizeof(u32);
	if (!data || !num || num > MDSS_DSI_RST_SEQ_LEN || num % 2) {
		pr_debug("%s:%d, error reading %s, length found = %d\n",
			__func__, __LINE__, name, num);
	} else {
		rc = of_property_read_u32_array(np, name, tmp, num);
		if (rc)
			pr_debug("%s:%d, error reading %s, rc = %d\n",
				__func__, __LINE__, name, rc);
		else {
			for (i = 0; i < num; ++i)
				rst_seq[i] = tmp[i];
			*rst_len = num;
		}
	}
	return 0;
}

static bool mdss_dsi_cmp_panel_reg_v2(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int i, j = 0;
	int len = 0, *lenp;
	int group = 0;

	lenp = ctrl->status_valid_params ?: ctrl->status_cmds_rlen;

	for (i = 0; i < ctrl->status_cmds.cmd_cnt; i++)
		len += lenp[i];

	for (j = 0; j < ctrl->groups; ++j) {
		for (i = 0; i < len; ++i) {
			pr_debug("[%i] return:0x%x status:0x%x\n",
				i, ctrl->return_buf[i],
				(unsigned int)ctrl->status_value[group + i]);
			MDSS_XLOG(ctrl->ndx, ctrl->return_buf[i],
					ctrl->status_value[group + i]);
			if (ctrl->return_buf[i] !=
				ctrl->status_value[group + i])
				break;
		}

		if (i == len)
			return true;
		group += len;
	}

	return false;
}

static int mdss_dsi_gen_read_status(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	if (!mdss_dsi_cmp_panel_reg_v2(ctrl_pdata)) {
		pr_err("%s: Read back value from panel is incorrect\n",
							__func__);
		return -EINVAL;
	} else {
		return 1;
	}
}

static int mdss_dsi_nt35596_read_status(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	if (!mdss_dsi_cmp_panel_reg(ctrl_pdata->status_buf,
		ctrl_pdata->status_value, 0)) {
		ctrl_pdata->status_error_count = 0;
		pr_err("%s: Read back value from panel is incorrect\n",
							__func__);
		return -EINVAL;
	} else {
		if (!mdss_dsi_cmp_panel_reg(ctrl_pdata->status_buf,
			ctrl_pdata->status_value, 3)) {
			ctrl_pdata->status_error_count = 0;
		} else {
			if (mdss_dsi_cmp_panel_reg(ctrl_pdata->status_buf,
				ctrl_pdata->status_value, 4) ||
				mdss_dsi_cmp_panel_reg(ctrl_pdata->status_buf,
				ctrl_pdata->status_value, 5))
				ctrl_pdata->status_error_count = 0;
			else
				ctrl_pdata->status_error_count++;
			if (ctrl_pdata->status_error_count >=
					ctrl_pdata->max_status_error_count) {
				ctrl_pdata->status_error_count = 0;
				pr_err("%s: Read value bad. Error_cnt = %i\n",
					 __func__,
					ctrl_pdata->status_error_count);
				return -EINVAL;
			}
		}
		return 1;
	}
}

static void mdss_dsi_parse_roi_alignment(struct device_node *np,
		struct dsi_panel_timing *pt)
{
	int len = 0;
	u32 value[6];
	struct property *data;
	struct mdss_panel_timing *timing = &pt->timing;

	data = of_find_property(np, "qcom,panel-roi-alignment", &len);
	len /= sizeof(u32);
	if (!data || (len != 6)) {
		pr_debug("%s: Panel roi alignment not found", __func__);
	} else {
		int rc = of_property_read_u32_array(np,
				"qcom,panel-roi-alignment", value, len);
		if (rc)
			pr_debug("%s: Error reading panel roi alignment values",
					__func__);
		else {
			timing->roi_alignment.xstart_pix_align = value[0];
			timing->roi_alignment.ystart_pix_align = value[1];
			timing->roi_alignment.width_pix_align = value[2];
			timing->roi_alignment.height_pix_align = value[3];
			timing->roi_alignment.min_width = value[4];
			timing->roi_alignment.min_height = value[5];
		}

		pr_debug("%s: ROI alignment: [%d, %d, %d, %d, %d, %d]",
			__func__, timing->roi_alignment.xstart_pix_align,
			timing->roi_alignment.width_pix_align,
			timing->roi_alignment.ystart_pix_align,
			timing->roi_alignment.height_pix_align,
			timing->roi_alignment.min_width,
			timing->roi_alignment.min_height);
	}
}

static void mdss_dsi_parse_dms_config(struct device_node *np,
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_info *pinfo = &ctrl->panel_data.panel_info;
	const char *data;
	bool dms_enabled;

	dms_enabled = of_property_read_bool(np,
		"qcom,dynamic-mode-switch-enabled");

	if (!dms_enabled) {
		pinfo->mipi.dms_mode = DYNAMIC_MODE_SWITCH_DISABLED;
		goto exit;
	}

	/* default mode is suspend_resume */
	pinfo->mipi.dms_mode = DYNAMIC_MODE_SWITCH_SUSPEND_RESUME;
	data = of_get_property(np, "qcom,dynamic-mode-switch-type", NULL);
	if (data && !strcmp(data, "dynamic-resolution-switch-immediate")) {
		if (!list_empty(&ctrl->panel_data.timings_list))
			pinfo->mipi.dms_mode =
				DYNAMIC_MODE_RESOLUTION_SWITCH_IMMEDIATE;
		else
			pinfo->mipi.dms_mode =
				DYNAMIC_MODE_SWITCH_DISABLED;
		goto exit;
	}

	if (data && !strcmp(data, "dynamic-switch-immediate"))
		pinfo->mipi.dms_mode = DYNAMIC_MODE_SWITCH_IMMEDIATE;
	else
		pr_debug("%s: default dms suspend/resume\n", __func__);

	mdss_dsi_parse_dcs_cmds(np, &ctrl->video2cmd,
		"qcom,video-to-cmd-mode-switch-commands",
		"qcom,mode-switch-commands-state");

	mdss_dsi_parse_dcs_cmds(np, &ctrl->cmd2video,
		"qcom,cmd-to-video-mode-switch-commands",
		"qcom,mode-switch-commands-state");

	mdss_dsi_parse_dcs_cmds(np, &ctrl->post_dms_on_cmds,
		"qcom,mdss-dsi-post-mode-switch-on-command",
		"qcom,mdss-dsi-post-mode-switch-on-command-state");

	if (pinfo->mipi.dms_mode == DYNAMIC_MODE_SWITCH_IMMEDIATE &&
		!ctrl->post_dms_on_cmds.cmd_cnt) {
		pr_warn("%s: No post dms on cmd specified\n", __func__);
		pinfo->mipi.dms_mode = DYNAMIC_MODE_SWITCH_DISABLED;
	}

	if (!ctrl->video2cmd.cmd_cnt || !ctrl->cmd2video.cmd_cnt) {
		pr_warn("%s: No commands specified for dynamic switch\n",
			__func__);
		pinfo->mipi.dms_mode = DYNAMIC_MODE_SWITCH_DISABLED;
	}
exit:
	pr_info("%s: dynamic switch feature enabled: %d\n", __func__,
		pinfo->mipi.dms_mode);
	return;
}

/* the length of all the valid values to be checked should not be great
 * than the length of returned data from read command.
 */
static bool
mdss_dsi_parse_esd_check_valid_params(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int i;

	for (i = 0; i < ctrl->status_cmds.cmd_cnt; ++i) {
		if (ctrl->status_valid_params[i] > ctrl->status_cmds_rlen[i]) {
			pr_debug("%s: ignore valid params!\n", __func__);
			return false;
		}
	}

	return true;
}

static bool mdss_dsi_parse_esd_status_len(struct device_node *np,
	char *prop_key, u32 **target, u32 cmd_cnt)
{
	int tmp;

	if (!of_find_property(np, prop_key, &tmp))
		return false;

	tmp /= sizeof(u32);
	if (tmp != cmd_cnt) {
		pr_err("%s: request property number(%d) not match command count(%d)\n",
			__func__, tmp, cmd_cnt);
		return false;
	}

	*target = kcalloc(tmp, sizeof(u32), GFP_KERNEL);
	if (IS_ERR_OR_NULL(*target)) {
		pr_err("%s: Error allocating memory for property\n",
			__func__);
		return false;
	}

	if (of_property_read_u32_array(np, prop_key, *target, tmp)) {
		pr_err("%s: cannot get values from dts\n", __func__);
		kfree(*target);
		*target = NULL;
		return false;
	}

	return true;
}

static void mdss_dsi_parse_esd_params(struct device_node *np,
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 tmp;
	u32 i, status_len, *lenp;
	int rc;
	struct property *data;
	const char *string;
	struct mdss_panel_info *pinfo = &ctrl->panel_data.panel_info;

	pinfo->esd_check_enabled = of_property_read_bool(np,
		"qcom,esd-check-enabled");

	if (!pinfo->esd_check_enabled)
		return;

	ctrl->status_mode = ESD_MAX;
	rc = of_property_read_string(np,
			"qcom,mdss-dsi-panel-status-check-mode", &string);
	if (!rc) {
		if (!strcmp(string, "bta_check")) {
			ctrl->status_mode = ESD_BTA;
		} else if (!strcmp(string, "reg_read")) {
			ctrl->status_mode = ESD_REG;
			ctrl->check_read_status =
				mdss_dsi_gen_read_status;
		} else if (!strcmp(string, "reg_read_nt35596")) {
			ctrl->status_mode = ESD_REG_NT35596;
			ctrl->status_error_count = 0;
			ctrl->check_read_status =
				mdss_dsi_nt35596_read_status;
		} else if (!strcmp(string, "te_signal_check")) {
			if (pinfo->mipi.mode == DSI_CMD_MODE) {
				ctrl->status_mode = ESD_TE;
			} else {
				pr_err("TE-ESD not valid for video mode\n");
				goto error;
			}
		} else {
			pr_err("No valid panel-status-check-mode string\n");
			goto error;
		}
	}

	if ((ctrl->status_mode == ESD_BTA) || (ctrl->status_mode == ESD_TE) ||
			(ctrl->status_mode == ESD_MAX))
		return;

	mdss_dsi_parse_dcs_cmds(np, &ctrl->status_cmds,
			"qcom,mdss-dsi-panel-status-command",
				"qcom,mdss-dsi-panel-status-command-state");

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-max-error-count",
		&tmp);
	ctrl->max_status_error_count = (!rc ? tmp : 0);

	if (!mdss_dsi_parse_esd_status_len(np,
		"qcom,mdss-dsi-panel-status-read-length",
		&ctrl->status_cmds_rlen, ctrl->status_cmds.cmd_cnt)) {
		pinfo->esd_check_enabled = false;
		return;
	}

	if (mdss_dsi_parse_esd_status_len(np,
		"qcom,mdss-dsi-panel-status-valid-params",
		&ctrl->status_valid_params, ctrl->status_cmds.cmd_cnt)) {
		if (!mdss_dsi_parse_esd_check_valid_params(ctrl))
			goto error1;
	}

	status_len = 0;
	lenp = ctrl->status_valid_params ?: ctrl->status_cmds_rlen;
	for (i = 0; i < ctrl->status_cmds.cmd_cnt; ++i)
		status_len += lenp[i];

	data = of_find_property(np, "qcom,mdss-dsi-panel-status-value", &tmp);
	tmp /= sizeof(u32);
	if (!IS_ERR_OR_NULL(data) && tmp != 0 && (tmp % status_len) == 0) {
		ctrl->groups = tmp / status_len;
	} else {
		pr_err("%s: Error parse panel-status-value\n", __func__);
		goto error1;
	}

	ctrl->status_value = kzalloc(sizeof(u32) * status_len * ctrl->groups,
				GFP_KERNEL);
	if (!ctrl->status_value)
		goto error1;

	ctrl->return_buf = kcalloc(status_len * ctrl->groups,
			sizeof(unsigned char), GFP_KERNEL);
	if (!ctrl->return_buf)
		goto error2;

	rc = of_property_read_u32_array(np,
		"qcom,mdss-dsi-panel-status-value",
		ctrl->status_value, ctrl->groups * status_len);
	if (rc) {
		pr_debug("%s: Error reading panel status values\n",
				__func__);
		memset(ctrl->status_value, 0, ctrl->groups * status_len);
	}

	return;

error2:
	kfree(ctrl->status_value);
error1:
	kfree(ctrl->status_valid_params);
	kfree(ctrl->status_cmds_rlen);
error:
	pinfo->esd_check_enabled = false;
}

static void mdss_dsi_parse_partial_update_caps(struct device_node *np,
		struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_info *pinfo;
	const char *data;

	pinfo = &ctrl->panel_data.panel_info;

	data = of_get_property(np, "qcom,partial-update-enabled", NULL);
	if (data && !strcmp(data, "single_roi"))
		pinfo->partial_update_supported =
			PU_SINGLE_ROI;
	else if (data && !strcmp(data, "dual_roi"))
		pinfo->partial_update_supported =
			PU_DUAL_ROI;
	else if (data && !strcmp(data, "none"))
		pinfo->partial_update_supported =
			PU_NOT_SUPPORTED;
	else
		pinfo->partial_update_supported =
			PU_NOT_SUPPORTED;

	if (pinfo->mipi.mode == DSI_CMD_MODE) {
		pinfo->partial_update_enabled = pinfo->partial_update_supported;
		pr_info("%s: partial_update_enabled=%d\n", __func__,
					pinfo->partial_update_enabled);
		ctrl->set_col_page_addr = mdss_dsi_set_col_page_addr;
		if (pinfo->partial_update_enabled) {
			pinfo->partial_update_roi_merge =
					of_property_read_bool(np,
					"qcom,partial-update-roi-merge");
		}
	}
}

static int mdss_dsi_parse_panel_features(struct device_node *np,
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_info *pinfo;

	if (!np || !ctrl) {
		pr_err("%s: Invalid arguments\n", __func__);
		return -ENODEV;
	}

	pinfo = &ctrl->panel_data.panel_info;

	mdss_dsi_parse_partial_update_caps(np, ctrl);

	pinfo->dcs_cmd_by_left = of_property_read_bool(np,
		"qcom,dcs-cmd-by-left");

	pinfo->ulps_feature_enabled = of_property_read_bool(np,
		"qcom,ulps-enabled");
	pr_info("%s: ulps feature %s\n", __func__,
		(pinfo->ulps_feature_enabled ? "enabled" : "disabled"));

	pinfo->ulps_suspend_enabled = of_property_read_bool(np,
		"qcom,suspend-ulps-enabled");
	pr_info("%s: ulps during suspend feature %s", __func__,
		(pinfo->ulps_suspend_enabled ? "enabled" : "disabled"));

	mdss_dsi_parse_dms_config(np, ctrl);

	pinfo->panel_ack_disabled = pinfo->sim_panel_mode ?
		1 : of_property_read_bool(np, "qcom,panel-ack-disabled");

	pinfo->allow_phy_power_off = of_property_read_bool(np,
		"qcom,panel-allow-phy-poweroff");

	mdss_dsi_parse_esd_params(np, ctrl);

	if (pinfo->panel_ack_disabled && pinfo->esd_check_enabled) {
		pr_warn("ESD should not be enabled if panel ACK is disabled\n");
		pinfo->esd_check_enabled = false;
	}

	if (ctrl->disp_en_gpio <= 0) {
		ctrl->disp_en_gpio = of_get_named_gpio(
			np,
			"qcom,5v-boost-gpio", 0);

		if (!gpio_is_valid(ctrl->disp_en_gpio))
			pr_debug("%s:%d, Disp_en gpio not specified\n",
					__func__, __LINE__);
	}

	mdss_dsi_parse_dcs_cmds(np, &ctrl->lp_on_cmds,
			"qcom,mdss-dsi-lp-mode-on", NULL);

	mdss_dsi_parse_dcs_cmds(np, &ctrl->lp_off_cmds,
			"qcom,mdss-dsi-lp-mode-off", NULL);

	return 0;
}

static void mdss_dsi_parse_panel_horizintal_line_idle(struct device_node *np,
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	const u32 *src;
	int i, len, cnt;
	struct panel_horizontal_idle *kp;

	if (!np || !ctrl) {
		pr_err("%s: Invalid arguments\n", __func__);
		return;
	}

	src = of_get_property(np, "qcom,mdss-dsi-hor-line-idle", &len);
	if (!src || len == 0)
		return;

	cnt = len % 3; /* 3 fields per entry */
	if (cnt) {
		pr_err("%s: invalid horizontal idle len=%d\n", __func__, len);
		return;
	}

	cnt = len / sizeof(u32);

	kp = kzalloc(sizeof(*kp) * (cnt / 3), GFP_KERNEL);
	if (kp == NULL) {
		pr_err("%s: No memory\n", __func__);
		return;
	}

	ctrl->line_idle = kp;
	for (i = 0; i < cnt; i += 3) {
		kp->min = be32_to_cpu(src[i]);
		kp->max = be32_to_cpu(src[i+1]);
		kp->idle = be32_to_cpu(src[i+2]);
		kp++;
		ctrl->horizontal_idle_cnt++;
	}

	/*
	 * idle is enabled for this controller, this will be used to
	 * enable/disable burst mode since both features are mutually
	 * exclusive.
	 */
	ctrl->idle_enabled = true;

	pr_debug("%s: horizontal_idle_cnt=%d\n", __func__,
				ctrl->horizontal_idle_cnt);
}

static int mdss_dsi_set_refresh_rate_range(struct device_node *pan_node,
		struct mdss_panel_info *pinfo)
{
	int rc = 0;
	rc = of_property_read_u32(pan_node,
			"qcom,mdss-dsi-min-refresh-rate",
			&pinfo->min_fps);
	if (rc) {
		pr_warn("%s:%d, Unable to read min refresh rate\n",
				__func__, __LINE__);

		/*
		 * If min refresh rate is not specified, set it to the
		 * default panel refresh rate.
		 */
		pinfo->min_fps = pinfo->mipi.frame_rate;
		rc = 0;
	}

	rc = of_property_read_u32(pan_node,
			"qcom,mdss-dsi-max-refresh-rate",
			&pinfo->max_fps);
	if (rc) {
		pr_warn("%s:%d, Unable to read max refresh rate\n",
				__func__, __LINE__);

		/*
		 * Since max refresh rate was not specified when dynamic
		 * fps is enabled, using the default panel refresh rate
		 * as max refresh rate supported.
		 */
		pinfo->max_fps = pinfo->mipi.frame_rate;
		rc = 0;
	}

	pr_info("dyn_fps: min = %d, max = %d\n",
			pinfo->min_fps, pinfo->max_fps);
	return rc;
}

static void mdss_dsi_parse_dfps_config(struct device_node *pan_node,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	const char *data;
	bool dynamic_fps, dynamic_bitclk;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);
	int rc = 0;

	dynamic_fps = of_property_read_bool(pan_node,
			"qcom,mdss-dsi-pan-enable-dynamic-fps");

	if (!dynamic_fps)
		goto dynamic_bitclk;

	pinfo->dynamic_fps = true;
	data = of_get_property(pan_node, "qcom,mdss-dsi-pan-fps-update", NULL);
	if (data) {
		if (!strcmp(data, "dfps_suspend_resume_mode")) {
			pinfo->dfps_update = DFPS_SUSPEND_RESUME_MODE;
			pr_debug("dfps mode: suspend/resume\n");
		} else if (!strcmp(data, "dfps_immediate_clk_mode")) {
			pinfo->dfps_update = DFPS_IMMEDIATE_CLK_UPDATE_MODE;
			pr_debug("dfps mode: Immediate clk\n");
		} else if (!strcmp(data, "dfps_immediate_porch_mode_hfp")) {
			pinfo->dfps_update =
				DFPS_IMMEDIATE_PORCH_UPDATE_MODE_HFP;
			pr_debug("dfps mode: Immediate porch HFP\n");
		} else if (!strcmp(data, "dfps_immediate_porch_mode_vfp")) {
			pinfo->dfps_update =
				DFPS_IMMEDIATE_PORCH_UPDATE_MODE_VFP;
			pr_debug("dfps mode: Immediate porch VFP\n");
		} else {
			pinfo->dfps_update = DFPS_SUSPEND_RESUME_MODE;
			pr_debug("default dfps mode: suspend/resume\n");
		}
	} else {
		pinfo->dynamic_fps = false;
		pr_debug("dfps update mode not configured: disable\n");
	}
	pinfo->new_fps = pinfo->mipi.frame_rate;
	pinfo->current_fps = pinfo->mipi.frame_rate;

dynamic_bitclk:
	dynamic_bitclk = of_property_read_bool(pan_node,
			"qcom,mdss-dsi-pan-enable-dynamic-bitclk");
	if (!dynamic_bitclk)
		return;

	of_find_property(pan_node, "qcom,mdss-dsi-dynamic-bitclk_freq",
		&pinfo->supp_bitclk_len);
	pinfo->supp_bitclk_len = pinfo->supp_bitclk_len/sizeof(u32);
	if (pinfo->supp_bitclk_len < 1)
		return;

	pinfo->supp_bitclks = kzalloc((sizeof(u32) * pinfo->supp_bitclk_len),
		GFP_KERNEL);
	if (!pinfo->supp_bitclks)
		return;

	rc = of_property_read_u32_array(pan_node,
		"qcom,mdss-dsi-dynamic-bitclk_freq", pinfo->supp_bitclks,
		pinfo->supp_bitclk_len);
	if (rc) {
		pr_err("Error from dynamic bitclk freq u64 array read\n");
		return;
	}
	pinfo->dynamic_bitclk = true;
	return;
}

int mdss_panel_parse_bl_settings(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	const char *data;
	int rc = 0;
	u32 tmp;

	ctrl_pdata->bklt_ctrl = UNKNOWN_CTRL;
	data = of_get_property(np, "qcom,mdss-dsi-bl-pmic-control-type", NULL);
	if (data) {
		if (!strcmp(data, "bl_ctrl_wled")) {
			led_trigger_register_simple("bkl-trigger",
				&bl_led_trigger);
			pr_debug("%s: SUCCESS-> WLED TRIGGER register\n",
				__func__);
			ctrl_pdata->bklt_ctrl = BL_WLED;
		} else if (!strcmp(data, "bl_ctrl_pwm")) {
			ctrl_pdata->bklt_ctrl = BL_PWM;
			ctrl_pdata->pwm_pmi = of_property_read_bool(np,
					"qcom,mdss-dsi-bl-pwm-pmi");
			rc = of_property_read_u32(np,
				"qcom,mdss-dsi-bl-pmic-pwm-frequency", &tmp);
			if (rc) {
				pr_err("%s:%d, Error, panel pwm_period\n",
						__func__, __LINE__);
				return -EINVAL;
			}
			ctrl_pdata->pwm_period = tmp;
			if (ctrl_pdata->pwm_pmi) {
				ctrl_pdata->pwm_bl = of_pwm_get(np, NULL);
				if (IS_ERR(ctrl_pdata->pwm_bl)) {
					pr_err("%s: Error, pwm device\n",
								__func__);
					ctrl_pdata->pwm_bl = NULL;
					return -EINVAL;
				}
			} else {
				rc = of_property_read_u32(np,
					"qcom,mdss-dsi-bl-pmic-bank-select",
								 &tmp);
				if (rc) {
					pr_err("%s:%d, Error, lpg channel\n",
							__func__, __LINE__);
					return -EINVAL;
				}
				ctrl_pdata->pwm_lpg_chan = tmp;
				tmp = of_get_named_gpio(np,
					"qcom,mdss-dsi-pwm-gpio", 0);
				ctrl_pdata->pwm_pmic_gpio = tmp;
				pr_debug("%s: Configured PWM bklt ctrl\n",
								 __func__);
			}
		} else if (!strcmp(data, "bl_ctrl_dcs")) {
			led_trigger_register_simple("bkl-trigger",
				&bl_led_trigger); //ASUS BSP +++ for wled register
			ctrl_pdata->bklt_ctrl = BL_DCS_CMD;
			data = of_get_property(np,
				"qcom,mdss-dsi-bl-dcs-command-state", NULL);
			if (data && !strcmp(data, "dsi_hs_mode"))
				ctrl_pdata->bklt_dcs_op_mode = DSI_HS_MODE;
			else
				ctrl_pdata->bklt_dcs_op_mode = DSI_LP_MODE;

			pr_debug("%s: Configured DCS_CMD bklt ctrl\n",
								__func__);
		}
	}
	return 0;
}

int mdss_dsi_panel_timing_switch(struct mdss_dsi_ctrl_pdata *ctrl,
			struct mdss_panel_timing *timing)
{
	struct dsi_panel_timing *pt;
	struct mdss_panel_info *pinfo = &ctrl->panel_data.panel_info;
	int i;

	if (!timing)
		return -EINVAL;

	if (timing == ctrl->panel_data.current_timing) {
		pr_warn("%s: panel timing \"%s\" already set\n", __func__,
				timing->name);
		return 0; /* nothing to do */
	}

	pr_debug("%s: ndx=%d switching to panel timing \"%s\"\n", __func__,
			ctrl->ndx, timing->name);

	mdss_panel_info_from_timing(timing, pinfo);

	pt = container_of(timing, struct dsi_panel_timing, timing);
	pinfo->mipi.t_clk_pre = pt->t_clk_pre;
	pinfo->mipi.t_clk_post = pt->t_clk_post;

	for (i = 0; i < ARRAY_SIZE(pt->phy_timing); i++)
		pinfo->mipi.dsi_phy_db.timing[i] = pt->phy_timing[i];

	for (i = 0; i < ARRAY_SIZE(pt->phy_timing_8996); i++)
		pinfo->mipi.dsi_phy_db.timing_8996[i] = pt->phy_timing_8996[i];

	ctrl->on_cmds = pt->on_cmds;
	ctrl->post_panel_on_cmds = pt->post_panel_on_cmds;

	ctrl->panel_data.current_timing = timing;
	if (!timing->clk_rate)
		ctrl->refresh_clk_rate = true;
	mdss_dsi_clk_refresh(&ctrl->panel_data, ctrl->update_phy_timing);

	return 0;
}

void mdss_dsi_unregister_bl_settings(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
#ifdef ASUS_ZC600KL_PROJECT
	if (ctrl_pdata->bklt_ctrl == BL_WLED)
#else
	if (ctrl_pdata->bklt_ctrl == BL_WLED || g_bl_wled_enable)
#endif
		led_trigger_unregister_simple(bl_led_trigger);
}

static int mdss_dsi_panel_timing_from_dt(struct device_node *np,
		struct dsi_panel_timing *pt,
		struct mdss_panel_data *panel_data)
{
	u32 tmp;
	u64 tmp64;
	int rc, i, len;
	const char *data;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;
	struct mdss_panel_info *pinfo;
	bool phy_timings_present = false;

	pinfo = &panel_data->panel_info;

	ctrl_pdata = container_of(panel_data, struct mdss_dsi_ctrl_pdata,
				panel_data);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-width", &tmp);
	if (rc) {
		pr_err("%s:%d, panel width not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	pt->timing.xres = tmp;

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-height", &tmp);
	if (rc) {
		pr_err("%s:%d, panel height not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	pt->timing.yres = tmp;

	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-front-porch", &tmp);
	pt->timing.h_front_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-back-porch", &tmp);
	pt->timing.h_back_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-pulse-width", &tmp);
	pt->timing.h_pulse_width = (!rc ? tmp : 2);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-sync-skew", &tmp);
	pt->timing.hsync_skew = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-back-porch", &tmp);
	pt->timing.v_back_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-front-porch", &tmp);
	pt->timing.v_front_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-pulse-width", &tmp);
	pt->timing.v_pulse_width = (!rc ? tmp : 2);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-left-border", &tmp);
	pt->timing.border_left = !rc ? tmp : 0;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-right-border", &tmp);
	pt->timing.border_right = !rc ? tmp : 0;

	/* overriding left/right borders for split display cases */
	if (mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data)) {
		if (panel_data->next)
			pt->timing.border_right = 0;
		else
			pt->timing.border_left = 0;
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-top-border", &tmp);
	pt->timing.border_top = !rc ? tmp : 0;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-bottom-border", &tmp);
	pt->timing.border_bottom = !rc ? tmp : 0;

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-framerate", &tmp);
	pt->timing.frame_rate = !rc ? tmp : DEFAULT_FRAME_RATE;
	rc = of_property_read_u64(np, "qcom,mdss-dsi-panel-clockrate", &tmp64);
	if (rc == -EOVERFLOW) {
		tmp64 = 0;
		rc = of_property_read_u32(np,
			"qcom,mdss-dsi-panel-clockrate", (u32 *)&tmp64);
	}
	pt->timing.clk_rate = !rc ? tmp64 : 0;

	data = of_get_property(np, "qcom,mdss-dsi-panel-timings", &len);
	if ((!data) || (len != 12)) {
		pr_debug("%s:%d, Unable to read Phy timing settings",
		       __func__, __LINE__);
	} else {
		for (i = 0; i < len; i++)
			pt->phy_timing[i] = data[i];
		phy_timings_present = true;
	}

	data = of_get_property(np, "qcom,mdss-dsi-panel-timings-phy-v2", &len);
	if ((!data) || (len != 40)) {
		pr_debug("%s:%d, Unable to read phy-v2 lane timing settings",
		       __func__, __LINE__);
	} else {
		for (i = 0; i < len; i++)
			pt->phy_timing_8996[i] = data[i];
		phy_timings_present = true;
	}
	if (!phy_timings_present) {
		pr_err("%s: phy timing settings not present\n", __func__);
		return -EINVAL;
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsi-t-clk-pre", &tmp);
	pt->t_clk_pre = (!rc ? tmp : 0x24);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-t-clk-post", &tmp);
	pt->t_clk_post = (!rc ? tmp : 0x03);

	if (np->name) {
		pt->timing.name = kstrdup(np->name, GFP_KERNEL);
		pr_info("%s: found new timing \"%s\" (%pK)\n", __func__,
				np->name, &pt->timing);
	}

	return 0;
}

static int  mdss_dsi_panel_config_res_properties(struct device_node *np,
		struct dsi_panel_timing *pt,
		struct mdss_panel_data *panel_data,
		bool default_timing)
{
	int rc = 0;

	mdss_dsi_parse_roi_alignment(np, pt);

	mdss_dsi_parse_dcs_cmds(np, &pt->on_cmds,
		"qcom,mdss-dsi-on-command",
		"qcom,mdss-dsi-on-command-state");

	mdss_dsi_parse_dcs_cmds(np, &pt->post_panel_on_cmds,
		"qcom,mdss-dsi-post-panel-on-command", NULL);

	mdss_dsi_parse_dcs_cmds(np, &pt->switch_cmds,
		"qcom,mdss-dsi-timing-switch-command",
		"qcom,mdss-dsi-timing-switch-command-state");

	rc = mdss_dsi_parse_topology_config(np, pt, panel_data, default_timing);
	if (rc) {
		pr_err("%s: parsing compression params failed. rc:%d\n",
			__func__, rc);
		return rc;
	}

	mdss_panel_parse_te_params(np, &pt->timing);
	return rc;
}

static int mdss_panel_parse_display_timings(struct device_node *np,
		struct mdss_panel_data *panel_data)
{
	struct mdss_dsi_ctrl_pdata *ctrl;
	struct dsi_panel_timing *modedb;
	struct device_node *timings_np;
	struct device_node *entry;
	int num_timings, rc;
	int i = 0, active_ndx = 0;
	bool default_timing = false;

	ctrl = container_of(panel_data, struct mdss_dsi_ctrl_pdata, panel_data);

	INIT_LIST_HEAD(&panel_data->timings_list);

	timings_np = of_get_child_by_name(np, "qcom,mdss-dsi-display-timings");
	if (!timings_np) {
		struct dsi_panel_timing *pt;

		pt = kzalloc(sizeof(*pt), GFP_KERNEL);
		if (!pt)
			return -ENOMEM;

		/*
		 * display timings node is not available, fallback to reading
		 * timings directly from root node instead
		 */
		pr_debug("reading display-timings from panel node\n");
		rc = mdss_dsi_panel_timing_from_dt(np, pt, panel_data);
		if (!rc) {
			mdss_dsi_panel_config_res_properties(np, pt,
					panel_data, true);
			rc = mdss_dsi_panel_timing_switch(ctrl, &pt->timing);
		} else {
			kfree(pt);
		}
		return rc;
	}

	num_timings = of_get_child_count(timings_np);
	if (num_timings == 0) {
		pr_err("no timings found within display-timings\n");
		rc = -EINVAL;
		goto exit;
	}

	modedb = kcalloc(num_timings, sizeof(*modedb), GFP_KERNEL);
	if (!modedb) {
		rc = -ENOMEM;
		goto exit;
	}

	for_each_child_of_node(timings_np, entry) {
		rc = mdss_dsi_panel_timing_from_dt(entry, (modedb + i),
				panel_data);
		if (rc) {
			kfree(modedb);
			goto exit;
		}

		default_timing = of_property_read_bool(entry,
				"qcom,mdss-dsi-timing-default");
		if (default_timing)
			active_ndx = i;

		mdss_dsi_panel_config_res_properties(entry, (modedb + i),
				panel_data, default_timing);

		list_add(&modedb[i].timing.list,
				&panel_data->timings_list);
		i++;
	}

	/* Configure default timing settings */
	rc = mdss_dsi_panel_timing_switch(ctrl, &modedb[active_ndx].timing);
	if (rc)
		pr_err("unable to configure default timing settings\n");

exit:
	of_node_put(timings_np);

	return rc;
}

static int mdss_panel_parse_dt(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	u32 tmp;
	int rc, len = 0;
	const char *data;
	static const char *pdest;
	const char *bridge_chip_name;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data))
		pinfo->is_split_display = true;

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-physical-width-dimension", &tmp);
	pinfo->physical_width = (!rc ? tmp : 0);
	rc = of_property_read_u32(np,
		"qcom,mdss-pan-physical-height-dimension", &tmp);
	pinfo->physical_height = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-bpp", &tmp);
	if (rc) {
		pr_err("%s:%d, bpp not specified\n", __func__, __LINE__);
		return -EINVAL;
	}
	pinfo->bpp = (!rc ? tmp : 24);
	pinfo->mipi.mode = DSI_VIDEO_MODE;
	data = of_get_property(np, "qcom,mdss-dsi-panel-type", NULL);
	if (data && !strncmp(data, "dsi_cmd_mode", 12))
		pinfo->mipi.mode = DSI_CMD_MODE;
	pinfo->mipi.boot_mode = pinfo->mipi.mode;
	tmp = 0;
	data = of_get_property(np, "qcom,mdss-dsi-pixel-packing", NULL);
	if (data && !strcmp(data, "loose"))
		pinfo->mipi.pixel_packing = 1;
	else
		pinfo->mipi.pixel_packing = 0;
	rc = mdss_panel_get_dst_fmt(pinfo->bpp,
		pinfo->mipi.mode, pinfo->mipi.pixel_packing,
		&(pinfo->mipi.dst_format));
	if (rc) {
		pr_debug("%s: problem determining dst format. Set Default\n",
			__func__);
		pinfo->mipi.dst_format =
			DSI_VIDEO_DST_FORMAT_RGB888;
	}
	pdest = of_get_property(np,
		"qcom,mdss-dsi-panel-destination", NULL);

	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-underflow-color", &tmp);
	pinfo->lcdc.underflow_clr = (!rc ? tmp : 0xff);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-border-color", &tmp);
	pinfo->lcdc.border_clr = (!rc ? tmp : 0);
	data = of_get_property(np, "qcom,mdss-dsi-panel-orientation", NULL);
	if (data) {
		pr_debug("panel orientation is %s\n", data);
		if (!strcmp(data, "180"))
			pinfo->panel_orientation = MDP_ROT_180;
		else if (!strcmp(data, "hflip"))
			pinfo->panel_orientation = MDP_FLIP_LR;
		else if (!strcmp(data, "vflip"))
			pinfo->panel_orientation = MDP_FLIP_UD;
	}

	rc = of_property_read_u32(np, "qcom,mdss-brightness-max-level", &tmp);
	pinfo->brightness_max = (!rc ? tmp : MDSS_MAX_BL_BRIGHTNESS);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-bl-min-level", &tmp);
	pinfo->bl_min = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-bl-max-level", &tmp);
	pinfo->bl_max = (!rc ? tmp : 255);
	ctrl_pdata->bklt_max = pinfo->bl_max;

	rc = of_property_read_u32(np, "qcom,mdss-dsi-interleave-mode", &tmp);
	pinfo->mipi.interleave_mode = (!rc ? tmp : 0);

	pinfo->mipi.vsync_enable = of_property_read_bool(np,
		"qcom,mdss-dsi-te-check-enable");

	if (pinfo->sim_panel_mode == SIM_SW_TE_MODE)
		pinfo->mipi.hw_vsync_mode = false;
	else
		pinfo->mipi.hw_vsync_mode = of_property_read_bool(np,
			"qcom,mdss-dsi-te-using-te-pin");

	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-h-sync-pulse", &tmp);
	pinfo->mipi.pulse_mode_hsa_he = (!rc ? tmp : false);

	pinfo->mipi.hfp_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-hfp-power-mode");
	pinfo->mipi.hsa_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-hsa-power-mode");
	pinfo->mipi.hbp_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-hbp-power-mode");
	pinfo->mipi.last_line_interleave_en = of_property_read_bool(np,
		"qcom,mdss-dsi-last-line-interleave");
	pinfo->mipi.bllp_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-bllp-power-mode");
	pinfo->mipi.eof_bllp_power_stop = of_property_read_bool(
		np, "qcom,mdss-dsi-bllp-eof-power-mode");
	pinfo->mipi.traffic_mode = DSI_NON_BURST_SYNCH_PULSE;
	data = of_get_property(np, "qcom,mdss-dsi-traffic-mode", NULL);
	if (data) {
		if (!strcmp(data, "non_burst_sync_event"))
			pinfo->mipi.traffic_mode = DSI_NON_BURST_SYNCH_EVENT;
		else if (!strcmp(data, "burst_mode"))
			pinfo->mipi.traffic_mode = DSI_BURST_MODE;
	}
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-te-dcs-command", &tmp);
	pinfo->mipi.insert_dcs_cmd =
			(!rc ? tmp : 1);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-wr-mem-continue", &tmp);
	pinfo->mipi.wr_mem_continue =
			(!rc ? tmp : 0x3c);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-wr-mem-start", &tmp);
	pinfo->mipi.wr_mem_start =
			(!rc ? tmp : 0x2c);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-te-pin-select", &tmp);
	pinfo->mipi.te_sel =
			(!rc ? tmp : 1);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-virtual-channel-id", &tmp);
	pinfo->mipi.vc = (!rc ? tmp : 0);
	pinfo->mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	data = of_get_property(np, "qcom,mdss-dsi-color-order", NULL);
	if (data) {
		if (!strcmp(data, "rgb_swap_rbg"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_RBG;
		else if (!strcmp(data, "rgb_swap_bgr"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_BGR;
		else if (!strcmp(data, "rgb_swap_brg"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_BRG;
		else if (!strcmp(data, "rgb_swap_grb"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_GRB;
		else if (!strcmp(data, "rgb_swap_gbr"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_GBR;
	}
	pinfo->mipi.data_lane0 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-0-state");
	pinfo->mipi.data_lane1 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-1-state");
	pinfo->mipi.data_lane2 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-2-state");
	pinfo->mipi.data_lane3 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-3-state");

	/* parse split link properties */
	rc = mdss_dsi_parse_split_link_settings(np, pinfo);
	if (rc)
		return rc;

	rc = mdss_panel_parse_display_timings(np, &ctrl_pdata->panel_data);
	if (rc)
		return rc;

	rc = mdss_dsi_parse_hdr_settings(np, pinfo);
	if (rc)
		return rc;

	pinfo->mipi.rx_eot_ignore = of_property_read_bool(np,
		"qcom,mdss-dsi-rx-eot-ignore");
	pinfo->mipi.tx_eot_append = of_property_read_bool(np,
		"qcom,mdss-dsi-tx-eot-append");

	rc = of_property_read_u32(np, "qcom,mdss-dsi-stream", &tmp);
	pinfo->mipi.stream = (!rc ? tmp : 0);

	data = of_get_property(np, "qcom,mdss-dsi-mode-sel-gpio-state", NULL);
	if (data) {
		if (!strcmp(data, "single_port"))
			pinfo->mode_sel_state = MODE_SEL_SINGLE_PORT;
		else if (!strcmp(data, "dual_port"))
			pinfo->mode_sel_state = MODE_SEL_DUAL_PORT;
		else if (!strcmp(data, "high"))
			pinfo->mode_sel_state = MODE_GPIO_HIGH;
		else if (!strcmp(data, "low"))
			pinfo->mode_sel_state = MODE_GPIO_LOW;
	} else {
		/* Set default mode as SPLIT mode */
		pinfo->mode_sel_state = MODE_SEL_DUAL_PORT;
	}

	rc = of_property_read_u32(np, "qcom,mdss-mdp-transfer-time-us", &tmp);
	pinfo->mdp_transfer_time_us = (!rc ? tmp : DEFAULT_MDP_TRANSFER_TIME);

	mdss_dsi_parse_mdp_kickoff_threshold(np, pinfo);

	pinfo->mipi.lp11_init = of_property_read_bool(np,
					"qcom,mdss-dsi-lp11-init");
	rc = of_property_read_u32(np, "qcom,mdss-dsi-init-delay-us", &tmp);
	pinfo->mipi.init_delay = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-post-init-delay", &tmp);
	pinfo->mipi.post_init_delay = (!rc ? tmp : 0);

	mdss_dsi_parse_trigger(np, &(pinfo->mipi.mdp_trigger),
		"qcom,mdss-dsi-mdp-trigger");

	mdss_dsi_parse_trigger(np, &(pinfo->mipi.dma_trigger),
		"qcom,mdss-dsi-dma-trigger");

	mdss_dsi_parse_reset_seq(np, pinfo->rst_seq, &(pinfo->rst_seq_len),
		"qcom,mdss-dsi-reset-sequence");

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->off_cmds,
		"qcom,mdss-dsi-off-command", "qcom,mdss-dsi-off-command-state");

#ifdef ASUS_ZE620KL_PROJECT
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->on_cmds_debug,
		"qcom,mdss-dsi-on-command-debug", "qcom,mdss-dsi-on-command-state");

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cabc_on_cmds,
		"qcom,mdss-dsi-cabc-on-command", "qcom,mdss-dsi-on-command-state");

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->cabc_on_cmds_debug,
		"qcom,mdss-dsi-cabc-on-command-debug", "qcom,mdss-dsi-on-command-state");
#endif

	rc = of_property_read_u32(np, "qcom,adjust-timer-wakeup-ms", &tmp);
	pinfo->adjust_timer_delay_ms = (!rc ? tmp : 0);

	pinfo->mipi.force_clk_lane_hs = of_property_read_bool(np,
		"qcom,mdss-dsi-force-clock-lane-hs");

	rc = mdss_dsi_parse_panel_features(np, ctrl_pdata);
	if (rc) {
		pr_err("%s: failed to parse panel features\n", __func__);
		goto error;
	}

	mdss_dsi_parse_panel_horizintal_line_idle(np, ctrl_pdata);

	mdss_dsi_parse_dfps_config(np, ctrl_pdata);

	mdss_dsi_set_refresh_rate_range(np, pinfo);

	pinfo->is_dba_panel = of_property_read_bool(np,
			"qcom,dba-panel");

	if (pinfo->is_dba_panel) {
		bridge_chip_name = of_get_property(np,
			"qcom,bridge-name", &len);
		if (!bridge_chip_name || len <= 0) {
			pr_err("%s:%d Unable to read qcom,bridge_name, data=%pK,len=%d\n",
				__func__, __LINE__, bridge_chip_name, len);
			rc = -EINVAL;
			goto error;
		}
		strlcpy(ctrl_pdata->bridge_name, bridge_chip_name,
			MSM_DBA_CHIP_NAME_MAX_LEN);
	}

	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-host-esc-clk-freq-hz",
		&pinfo->esc_clk_rate_hz);
	if (rc)
		pinfo->esc_clk_rate_hz = MDSS_DSI_MAX_ESC_CLK_RATE_HZ;
	pr_debug("%s: esc clk %d\n", __func__, pinfo->esc_clk_rate_hz);

	mdss_livedisplay_parse_dt(np, pinfo);

	return 0;

error:
	return -EINVAL;
}

int mdss_dsi_panel_init(struct device_node *node,
	struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	int ndx)
{
	int rc = 0;
	static const char *panel_name;
	struct mdss_panel_info *pinfo;

	if (!node || !ctrl_pdata) {
		pr_err("%s: Invalid arguments\n", __func__);
		return -ENODEV;
	}

	pinfo = &ctrl_pdata->panel_data.panel_info;

	pr_debug("%s:%d\n", __func__, __LINE__);
	pinfo->panel_name[0] = '\0';
	panel_name = of_get_property(node, "qcom,mdss-dsi-panel-name", NULL);
	if (!panel_name) {
		pr_info("%s:%d, Panel name not specified\n",
						__func__, __LINE__);
	} else {
		pr_info("%s: Panel Name = %s\n", __func__, panel_name);
		strlcpy(&pinfo->panel_name[0], panel_name, MDSS_MAX_PANEL_LEN);
	}
	rc = mdss_panel_parse_dt(node, ctrl_pdata);
	if (rc) {
		pr_err("%s:%d panel dt parse failed\n", __func__, __LINE__);
		return rc;
	}

	pinfo->dynamic_switch_pending = false;
	pinfo->is_lpm_mode = false;
	pinfo->esd_rdy = false;
	pinfo->persist_mode = false;

	ctrl_pdata->on = mdss_dsi_panel_on;
	ctrl_pdata->post_panel_on = mdss_dsi_post_panel_on;
	ctrl_pdata->off = mdss_dsi_panel_off;
	ctrl_pdata->low_power_config = mdss_dsi_panel_low_power_config;
	ctrl_pdata->panel_data.set_backlight = mdss_dsi_panel_bl_ctrl;
	ctrl_pdata->panel_data.apply_display_setting =
			mdss_dsi_panel_apply_display_setting;
	ctrl_pdata->switch_mode = mdss_dsi_panel_switch_mode;

#ifndef ASUS_ZC600KL_PROJECT
	/* set cabc off when this is factory build */
	if (g_ftm_mode) {
		printk("%s:: Factory Mode CABC off \n", __func__);
		asus_lcd_cabc_mode[1] = OFF_MODE;
	}

	mutex_init(&asus_lcd_tcon_cmd_mutex);
	mutex_init(&asus_lcd_bl_cmd_mutex);
	proc_create(ASUS_LCD_PROC_CABC,        0777, NULL, &asus_lcd_cabc_proc_ops);
	proc_create(ASUS_LCD_PROC_REGISTER_RW, 0640, NULL, &asus_lcd_reg_rw_ops);
	proc_create(ASUS_LCD_PROC_UNIQUE_ID,   0444, NULL, &asus_lcd_unique_id_proc_ops);
	proc_create(ASUS_LCD_PROC_PANEL_ID,    0444, NULL, &asus_lcd_id_proc_ops);
	proc_create(ASUS_LCD_PROC_DUMP_CALI_INFO, 0666, NULL, &asus_lcd_info_proc_ops);
	asus_lcd_delay_workqueue = create_workqueue("display_wq");

#ifdef ASUS_ZE620KL_PROJECT
	g_bl_threshold = LCD_BL_THRESHOLD_AUO;
	g_wled_dimming_div = 10;
	printk("[Display] %s:: AUO g_bl_threshold(%d) g_wled_dimming_div(%d)\n",
				__func__, g_bl_threshold, g_wled_dimming_div);
#else
	if (g_asus_lcdID == ARA_LCD_AUO_2 || g_asus_lcdID == ARA_LCD_AUO) {
		g_bl_threshold = LCD_BL_THRESHOLD_TM;
		g_wled_dimming_div = 10;
		printk("[Display] %s:: TM g_bl_threshold(%d) g_wled_dimming_div(%d)\n",
				__func__, g_bl_threshold, g_wled_dimming_div);
	} else {
		g_bl_threshold = LCD_BL_THRESHOLD_BOE;
		g_wled_dimming_div = 10;
		printk("[Display] %s:: BOE g_bl_threshold(%d) g_wled_dimming_div(%d)\n",
				__func__, g_bl_threshold, g_wled_dimming_div);
	}
#endif /* if ASUS_ZE620KL_PROJECT */
#endif /* if not ASUS_ZC600KL_PROJECT */

	return 0;
}
