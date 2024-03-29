/* Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"FG: %s: " fmt, __func__

#include <linux/ktime.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_batterydata.h>
#include <linux/platform_device.h>
#include <linux/iio/consumer.h>
#include <linux/qpnp/qpnp-revid.h>
#include "fg-core.h"
#include "fg-reg.h"
//ASUS_BSP +++
#include <linux/proc_fs.h>
#include <linux/switch.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/qpnp/qpnp-adc.h> //ASUS_BSP LiJen config PMIC GPIO2 to ADC channel 
#include <linux/wakelock.h>
//ASUS_BSP battery safety upgrade +++
#include <linux/mm.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/rtc.h>
//ASUS_BSP battery safety upgrade ---
//ASUS_BSP display callback function +++
#include <linux/fb.h>
#include <linux/notifier.h>
//ASUS_BSP display callback function ---
//ASUS_BSP ---

#define FG_GEN3_DEV_NAME	"qcom,fg-gen3"

#define PERPH_SUBTYPE_REG		0x05
#define FG_BATT_SOC_PMI8998		0x10
#define FG_BATT_INFO_PMI8998		0x11
#define FG_MEM_INFO_PMI8998		0x0D

/* SRAM address and offset in ascending order */
#define ESR_PULSE_THRESH_WORD		2
#define ESR_PULSE_THRESH_OFFSET		3
#define SLOPE_LIMIT_WORD		3
#define SLOPE_LIMIT_OFFSET		0
#define CUTOFF_CURR_WORD		4
#define CUTOFF_CURR_OFFSET		0
#define CUTOFF_VOLT_WORD		5
#define CUTOFF_VOLT_OFFSET		0
#define SYS_TERM_CURR_WORD		6
#define SYS_TERM_CURR_OFFSET		0
#define VBATT_FULL_WORD			7
#define VBATT_FULL_OFFSET		0
#define ESR_FILTER_WORD			8
#define ESR_UPD_TIGHT_OFFSET		0
#define ESR_UPD_BROAD_OFFSET		1
#define ESR_UPD_TIGHT_LOW_TEMP_OFFSET	2
#define ESR_UPD_BROAD_LOW_TEMP_OFFSET	3
#define KI_COEFF_MED_DISCHG_WORD	9
#define TIMEBASE_OFFSET			1
#define KI_COEFF_MED_DISCHG_OFFSET	3
#define KI_COEFF_HI_DISCHG_WORD		10
#define KI_COEFF_HI_DISCHG_OFFSET	0
#define KI_COEFF_LOW_DISCHG_WORD	10
#define KI_COEFF_LOW_DISCHG_OFFSET	2
#define KI_COEFF_FULL_SOC_WORD		12
#define KI_COEFF_FULL_SOC_OFFSET	2
#define DELTA_MSOC_THR_WORD		12
#define DELTA_MSOC_THR_OFFSET		3
#define DELTA_BSOC_THR_WORD		13
#define DELTA_BSOC_THR_OFFSET		2
#define RECHARGE_SOC_THR_WORD		14
#define RECHARGE_SOC_THR_OFFSET		0
#define CHG_TERM_CURR_WORD		14
#define CHG_TERM_CURR_OFFSET		1
#define EMPTY_VOLT_WORD			15
#define EMPTY_VOLT_OFFSET		0
#define VBATT_LOW_WORD			15
#define VBATT_LOW_OFFSET		1
#define ESR_TIMER_DISCHG_MAX_WORD	17
#define ESR_TIMER_DISCHG_MAX_OFFSET	0
#define ESR_TIMER_DISCHG_INIT_WORD	17
#define ESR_TIMER_DISCHG_INIT_OFFSET	2
#define ESR_TIMER_CHG_MAX_WORD		18
#define ESR_TIMER_CHG_MAX_OFFSET	0
#define ESR_TIMER_CHG_INIT_WORD		18
#define ESR_TIMER_CHG_INIT_OFFSET	2
#define ESR_EXTRACTION_ENABLE_WORD	19
#define ESR_EXTRACTION_ENABLE_OFFSET	0
#define PROFILE_LOAD_WORD		24
#define PROFILE_LOAD_OFFSET		0
#define ESR_RSLOW_DISCHG_WORD		34
#define ESR_RSLOW_DISCHG_OFFSET		0
#define ESR_RSLOW_CHG_WORD		51
#define ESR_RSLOW_CHG_OFFSET		0
#define NOM_CAP_WORD			58
#define NOM_CAP_OFFSET			0
#define ACT_BATT_CAP_BKUP_WORD		74
#define ACT_BATT_CAP_BKUP_OFFSET	0
#define CYCLE_COUNT_WORD		75
#define CYCLE_COUNT_OFFSET		0
#define PROFILE_INTEGRITY_WORD		79
#define SW_CONFIG_OFFSET		0
#define PROFILE_INTEGRITY_OFFSET	3
#define BATT_SOC_WORD			91
#define BATT_SOC_OFFSET			0
#define FULL_SOC_WORD			93
#define FULL_SOC_OFFSET			2
#define MONOTONIC_SOC_WORD		94
#define MONOTONIC_SOC_OFFSET		2
#define CC_SOC_WORD			95
#define CC_SOC_OFFSET			0
#define CC_SOC_SW_WORD			96
#define CC_SOC_SW_OFFSET		0
#define VOLTAGE_PRED_WORD		97
#define VOLTAGE_PRED_OFFSET		0
#define OCV_WORD			97
#define OCV_OFFSET			2
#define ESR_WORD			99
#define ESR_OFFSET			0
#define RSLOW_WORD			101
#define RSLOW_OFFSET			0
#define ACT_BATT_CAP_WORD		117
#define ACT_BATT_CAP_OFFSET		0
#define LAST_BATT_SOC_WORD		119
#define LAST_BATT_SOC_OFFSET		0
#define LAST_MONOTONIC_SOC_WORD		119
#define LAST_MONOTONIC_SOC_OFFSET	2
#define ALG_FLAGS_WORD			120
#define ALG_FLAGS_OFFSET		1

/* v2 SRAM address and offset in ascending order */
#define KI_COEFF_LOW_DISCHG_v2_WORD	9
#define KI_COEFF_LOW_DISCHG_v2_OFFSET	3
#define KI_COEFF_MED_DISCHG_v2_WORD	10
#define KI_COEFF_MED_DISCHG_v2_OFFSET	0
#define KI_COEFF_HI_DISCHG_v2_WORD	10
#define KI_COEFF_HI_DISCHG_v2_OFFSET	1
#define DELTA_BSOC_THR_v2_WORD		12
#define DELTA_BSOC_THR_v2_OFFSET	3
#define DELTA_MSOC_THR_v2_WORD		13
#define DELTA_MSOC_THR_v2_OFFSET	0
#define RECHARGE_SOC_THR_v2_WORD	14
#define RECHARGE_SOC_THR_v2_OFFSET	1
#define CHG_TERM_CURR_v2_WORD		15
#define CHG_TERM_BASE_CURR_v2_OFFSET	0
#define CHG_TERM_CURR_v2_OFFSET		1
#define EMPTY_VOLT_v2_WORD		15
#define EMPTY_VOLT_v2_OFFSET		3
#define VBATT_LOW_v2_WORD		16
#define VBATT_LOW_v2_OFFSET		0
#define RECHARGE_VBATT_THR_v2_WORD	16
#define RECHARGE_VBATT_THR_v2_OFFSET	1
#define FLOAT_VOLT_v2_WORD		16
#define FLOAT_VOLT_v2_OFFSET		2

//ASUS_BSP +++
#define BAT_TAG "[BAT][BMS]"
#define ERROR_TAG "[ERR]"
#define BAT_DBG(...)  printk(KERN_INFO BAT_TAG __VA_ARGS__)
#define BAT_DBG_E(...)  printk(KERN_ERR BAT_TAG ERROR_TAG __VA_ARGS__)
#define EVT_TAG "fg-evt:"

struct fg_chip * g_fgChip = NULL;
#define REPORT_CAPACITY_POLLING_TIME 180

int gauge_get_prop =0;
static struct delayed_work fix_maint_soc_work;
static struct delayed_work regular_check_soc_work;

struct delayed_work init_asus_capacity_work;
struct delayed_work backup_asus_capacity_work;
static int g_asusCapacityShift = -1;

//ASUS_BSP LiJen config PMIC GPIO2 VADC_BTM channel +++
static struct qpnp_adc_tm_chip *g_adc_tm_dev;
static struct qpnp_adc_tm_btm_param g_adc_param;
static int g_wp_state;
//ASUS_BSP LiJen config PMIC GPIO2 VADC_BTM channel ---

int g_reporting_soc =45;
int g_maint_msoc =45;
#define FAKE_TEMP_INIT	180
int fake_temp = FAKE_TEMP_INIT;

#define SOC_CHECK_INTERVAL	 30
#define FAKE_TEMP_INIT	180

#define BATT_GENERIC "batterydata_Generic"
#define BATT_LIBRA_SMP "Libra_2900mah_4p38v_SMP"
#define BATT_LIBRA_COSLIGHT "Libra_2900mah_4p38v_Coslight"
#define BATT_LIBRA_LG "Libra_2900mah_4p38v_LG"
#define BATT_LEO_SMP "batterydata_Leo_2530mAh_4p38V_SMP"
#ifdef ASUS_ZE620KL_PROJECT
#define BATT_TYPE_CSL "c11p1618cos_3150mah_apr18th2017"
#define BATT_TYPE_CSL_V2_4P35 "c11p1618cos_3150mah_apr18th2017"
#define BATT_TYPE_51K	"CSL_51K"
#else
#define BATT_TYPE_CSL "2992846_asus_c11p1705scud_2900mah_averaged_masterslave_aug2nd2017"
#define BATT_TYPE_CSL_V2_4P35 "2992846_asus_c11p1705scud_2900mah_averaged_masterslave_aug2nd2017"
#define BATT_TYPE_51K	"scud_51K"
#endif
#define BATT_TYPE_CSL_ER "3282423_asus_c11p1708_3150mah_averaged_masterslave_jan3rd2018"
#define BATT_TYPE_CSL_ER_V2_4P35 "3282423_asus_c11p1708_3150mah_averaged_masterslave_jan3rd2018"
#define BATT_TYPE_51K_ER	"CSL_51K"
#define BATT_MODELNAME_LIBRA "C11P1511"
#define BATT_MODELNAME_LEO "C11P1601"
#define BATT_MODELNAME_TITAN "C11P1618"
#define BATT_MODELNAME_ARA "C11P1708"
#define BATT_ID_51K_INDEX		1
#define BATT_ID_10K_INDEX		2
#define BATT_ID_100K_INDEX	3
#define BATT_ID_75K_INDEX	4
#define FIRST_PROFILE_VER	1
#define SECOND_PROFILE_VER	2
#define BATT_UNKNOWN	 "Unknown Battery"

#define DEFAULT_ASUS_FULL_MSOC_THR	 248
//ASUS_BSP battery safety upgrade +++
#define CYCLE_COUNT_DATA_MAGIC  0x85
#define CYCLE_COUNT_FILE_NAME   "/dev/block/platform/soc/c0c4000.sdhci/by-name/asuskey3"
#define BAT_PERCENT_FILE_NAME   "/asdf/Batpercentage"
#define BAT_SAFETY_FILE_NAME   "/asdf/bat_safety"
#define CYCLE_COUNT_SD_FILE_NAME   "/APD/.bs"
#define BAT_PERCENT_SD_FILE_NAME   "/APD/Batpercentage"
#define BAT_CYCLE_SD_FILE_NAME   "/asdf/Batcyclecount"
#define CYCLE_COUNT_DATA_OFFSET  0x0
#define FILE_OP_READ   0
#define FILE_OP_WRITE   1
#define	BATTERY_SAFETY_UPGRADE_TIME 1*60

static bool g_cyclecount_initialized = false;
extern bool rtc_probe_done;
static struct CYCLE_COUNT_DATA g_cycle_count_data = {
    .magic = CYCLE_COUNT_DATA_MAGIC,
    .cycle_count=0,
    .battery_total_time = 0,
    .high_vol_total_time = 0,
    .high_temp_total_time = 0,
    .high_temp_vol_time = 0,
    .reload_condition = 0
};
struct delayed_work battery_safety_work;
//ASUS_BSP battery safety upgrade ---

//ASUS_BS battery health upgrade +++
#define	BATTERY_HEALTH_UPGRADE_TIME 1 //ASUS_BS battery health upgrade
#define	BATTERY_METADATA_UPGRADE_TIME 60 //ASUS_BS battery health upgrade
#define BAT_HEALTH_DATA_OFFSET  0x0
#define BAT_HEALTH_DATA_MAGIC  0x86
#define BAT_HEALTH_DATA_BACKUP_MAGIC 0x87
#define ZE620KL_DESIGNED_CAPACITY 3150 //mAh
#define BAT_HEALTH_DATA_FILE_NAME   "/asdf/bat_health_binary"
#define BAT_HEALTH_DATA_SD_FILE_NAME   "/APD/.bh"
#define BAT_HEALTH_START_LEVEL 70
#define BAT_HEALTH_END_LEVEL 100
static bool g_bathealth_initialized = false;
static bool g_bathealth_trigger = false;
static bool g_last_bathealth_trigger = false;
static bool g_health_debug_enable = false;
static bool g_health_upgrade_enable = true;
static int g_health_upgrade_index = 0;
static int g_health_upgrade_start_level = BAT_HEALTH_START_LEVEL;
static int g_health_upgrade_end_level = BAT_HEALTH_END_LEVEL;
static int g_health_upgrade_upgrade_time = BATTERY_HEALTH_UPGRADE_TIME;
static int g_bat_health_avg;

static struct BAT_HEALTH_DATA g_bat_health_data = {
    .magic = BAT_HEALTH_DATA_MAGIC,
    .bat_current = 0,
    .bat_current_avg = 0,
    .accumulate_time = 0,
    .accumulate_current = 0,
    .bat_health = 0
};
static struct BAT_HEALTH_DATA_BACKUP g_bat_health_data_backup[BAT_HEALTH_NUMBER_MAX] = {
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0}
};
struct delayed_work battery_health_work;
struct delayed_work battery_metadata_work;
struct wake_lock bat_health_lock;
//ASUS_BS battery health upgrade ---
static bool g_screen_on = false; //ASUS_BSP display callback function

static int fg_decode_voltage_15b(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val);
static int fg_decode_value_16b(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val);
static int fg_decode_default(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val);
static int fg_decode_cc_soc(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int value);
static void fg_encode_voltage(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val_mv, u8 *buf);
static void fg_encode_current(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val_ma, u8 *buf);
static void fg_encode_default(struct fg_sram_param *sp,
	enum fg_sram_param_id id, int val, u8 *buf);

static struct fg_irq_info fg_irqs[FG_IRQ_MAX];

#define PARAM(_id, _addr_word, _addr_byte, _len, _num, _den, _offset,	\
	      _enc, _dec)						\
	[FG_SRAM_##_id] = {						\
		.addr_word	= _addr_word,				\
		.addr_byte	= _addr_byte,				\
		.len		= _len,					\
		.numrtr		= _num,					\
		.denmtr		= _den,					\
		.offset		= _offset,				\
		.encode		= _enc,					\
		.decode		= _dec,					\
	}								\

static struct fg_sram_param pmi8998_v1_sram_params[] = {
	PARAM(BATT_SOC, BATT_SOC_WORD, BATT_SOC_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_default),
	PARAM(FULL_SOC, FULL_SOC_WORD, FULL_SOC_OFFSET, 2, 1, 1, 0, NULL,
		fg_decode_default),
	PARAM(VOLTAGE_PRED, VOLTAGE_PRED_WORD, VOLTAGE_PRED_OFFSET, 2, 1000,
		244141, 0, NULL, fg_decode_voltage_15b),
	PARAM(OCV, OCV_WORD, OCV_OFFSET, 2, 1000, 244141, 0, NULL,
		fg_decode_voltage_15b),
	PARAM(ESR, ESR_WORD, ESR_OFFSET, 2, 1000, 244141, 0, fg_encode_default,
		fg_decode_value_16b),
	PARAM(RSLOW, RSLOW_WORD, RSLOW_OFFSET, 2, 1000, 244141, 0, NULL,
		fg_decode_value_16b),
	PARAM(ALG_FLAGS, ALG_FLAGS_WORD, ALG_FLAGS_OFFSET, 1, 1, 1, 0, NULL,
		fg_decode_default),
	PARAM(CC_SOC, CC_SOC_WORD, CC_SOC_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_cc_soc),
	PARAM(CC_SOC_SW, CC_SOC_SW_WORD, CC_SOC_SW_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_cc_soc),
	PARAM(ACT_BATT_CAP, ACT_BATT_CAP_BKUP_WORD, ACT_BATT_CAP_BKUP_OFFSET, 2,
		1, 1, 0, NULL, fg_decode_default),
	/* Entries below here are configurable during initialization */
	PARAM(CUTOFF_VOLT, CUTOFF_VOLT_WORD, CUTOFF_VOLT_OFFSET, 2, 1000000,
		244141, 0, fg_encode_voltage, NULL),
	PARAM(EMPTY_VOLT, EMPTY_VOLT_WORD, EMPTY_VOLT_OFFSET, 1, 100000, 390625,
		-2500, fg_encode_voltage, NULL),
	PARAM(VBATT_LOW, VBATT_LOW_WORD, VBATT_LOW_OFFSET, 1, 100000, 390625,
		-2500, fg_encode_voltage, NULL),
	PARAM(VBATT_FULL, VBATT_FULL_WORD, VBATT_FULL_OFFSET, 2, 1000,
		244141, 0, fg_encode_voltage, fg_decode_voltage_15b),
	PARAM(SYS_TERM_CURR, SYS_TERM_CURR_WORD, SYS_TERM_CURR_OFFSET, 3,
		1000000, 122070, 0, fg_encode_current, NULL),
	PARAM(CHG_TERM_CURR, CHG_TERM_CURR_WORD, CHG_TERM_CURR_OFFSET, 1,
		100000, 390625, 0, fg_encode_current, NULL),
	PARAM(CUTOFF_CURR, CUTOFF_CURR_WORD, CUTOFF_CURR_OFFSET, 3,
		1000000, 122070, 0, fg_encode_current, NULL),
	PARAM(DELTA_MSOC_THR, DELTA_MSOC_THR_WORD, DELTA_MSOC_THR_OFFSET, 1,
		2048, 100, 0, fg_encode_default, NULL),
	PARAM(DELTA_BSOC_THR, DELTA_BSOC_THR_WORD, DELTA_BSOC_THR_OFFSET, 1,
		2048, 100, 0, fg_encode_default, NULL),
	PARAM(RECHARGE_SOC_THR, RECHARGE_SOC_THR_WORD, RECHARGE_SOC_THR_OFFSET,
		1, 256, 100, 0, fg_encode_default, NULL),
	PARAM(ESR_TIMER_DISCHG_MAX, ESR_TIMER_DISCHG_MAX_WORD,
		ESR_TIMER_DISCHG_MAX_OFFSET, 2, 1, 1, 0, fg_encode_default,
		NULL),
	PARAM(ESR_TIMER_DISCHG_INIT, ESR_TIMER_DISCHG_INIT_WORD,
		ESR_TIMER_DISCHG_INIT_OFFSET, 2, 1, 1, 0, fg_encode_default,
		NULL),
	PARAM(ESR_TIMER_CHG_MAX, ESR_TIMER_CHG_MAX_WORD,
		ESR_TIMER_CHG_MAX_OFFSET, 2, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_TIMER_CHG_INIT, ESR_TIMER_CHG_INIT_WORD,
		ESR_TIMER_CHG_INIT_OFFSET, 2, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_PULSE_THRESH, ESR_PULSE_THRESH_WORD, ESR_PULSE_THRESH_OFFSET,
		1, 100000, 390625, 0, fg_encode_default, NULL),
	PARAM(KI_COEFF_MED_DISCHG, KI_COEFF_MED_DISCHG_WORD,
		KI_COEFF_MED_DISCHG_OFFSET, 1, 1000, 244141, 0,
		fg_encode_default, NULL),
	PARAM(KI_COEFF_HI_DISCHG, KI_COEFF_HI_DISCHG_WORD,
		KI_COEFF_HI_DISCHG_OFFSET, 1, 1000, 244141, 0,
		fg_encode_default, NULL),
	PARAM(KI_COEFF_FULL_SOC, KI_COEFF_FULL_SOC_WORD,
		KI_COEFF_FULL_SOC_OFFSET, 1, 1000, 244141, 0,
		fg_encode_default, NULL),
	PARAM(ESR_TIGHT_FILTER, ESR_FILTER_WORD, ESR_UPD_TIGHT_OFFSET,
		1, 512, 1000000, 0, fg_encode_default, NULL),
	PARAM(ESR_BROAD_FILTER, ESR_FILTER_WORD, ESR_UPD_BROAD_OFFSET,
		1, 512, 1000000, 0, fg_encode_default, NULL),
	PARAM(SLOPE_LIMIT, SLOPE_LIMIT_WORD, SLOPE_LIMIT_OFFSET, 1, 8192, 1000,
		0, fg_encode_default, NULL),
};

static struct fg_sram_param pmi8998_v2_sram_params[] = {
	PARAM(BATT_SOC, BATT_SOC_WORD, BATT_SOC_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_default),
	PARAM(FULL_SOC, FULL_SOC_WORD, FULL_SOC_OFFSET, 2, 1, 1, 0, NULL,
		fg_decode_default),
	PARAM(VOLTAGE_PRED, VOLTAGE_PRED_WORD, VOLTAGE_PRED_OFFSET, 2, 1000,
		244141, 0, NULL, fg_decode_voltage_15b),
	PARAM(OCV, OCV_WORD, OCV_OFFSET, 2, 1000, 244141, 0, NULL,
		fg_decode_voltage_15b),
	PARAM(ESR, ESR_WORD, ESR_OFFSET, 2, 1000, 244141, 0, fg_encode_default,
		fg_decode_value_16b),
	PARAM(RSLOW, RSLOW_WORD, RSLOW_OFFSET, 2, 1000, 244141, 0, NULL,
		fg_decode_value_16b),
	PARAM(ALG_FLAGS, ALG_FLAGS_WORD, ALG_FLAGS_OFFSET, 1, 1, 1, 0, NULL,
		fg_decode_default),
	PARAM(CC_SOC, CC_SOC_WORD, CC_SOC_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_cc_soc),
	PARAM(CC_SOC_SW, CC_SOC_SW_WORD, CC_SOC_SW_OFFSET, 4, 1, 1, 0, NULL,
		fg_decode_cc_soc),
	PARAM(ACT_BATT_CAP, ACT_BATT_CAP_BKUP_WORD, ACT_BATT_CAP_BKUP_OFFSET, 2,
		1, 1, 0, NULL, fg_decode_default),
	PARAM(TIMEBASE, KI_COEFF_MED_DISCHG_WORD, TIMEBASE_OFFSET, 2, 1000,
		61000, 0, fg_encode_default, NULL),
	/* Entries below here are configurable during initialization */
	PARAM(CUTOFF_VOLT, CUTOFF_VOLT_WORD, CUTOFF_VOLT_OFFSET, 2, 1000000,
		244141, 0, fg_encode_voltage, NULL),
	PARAM(EMPTY_VOLT, EMPTY_VOLT_v2_WORD, EMPTY_VOLT_v2_OFFSET, 1, 1000,
		15625, -2000, fg_encode_voltage, NULL),
	PARAM(VBATT_LOW, VBATT_LOW_v2_WORD, VBATT_LOW_v2_OFFSET, 1, 1000,
		15625, -2000, fg_encode_voltage, NULL),
	PARAM(FLOAT_VOLT, FLOAT_VOLT_v2_WORD, FLOAT_VOLT_v2_OFFSET, 1, 1000,
		15625, -2000, fg_encode_voltage, NULL),
	PARAM(VBATT_FULL, VBATT_FULL_WORD, VBATT_FULL_OFFSET, 2, 1000,
		244141, 0, fg_encode_voltage, fg_decode_voltage_15b),
	PARAM(SYS_TERM_CURR, SYS_TERM_CURR_WORD, SYS_TERM_CURR_OFFSET, 3,
		1000000, 122070, 0, fg_encode_current, NULL),
	PARAM(CHG_TERM_CURR, CHG_TERM_CURR_v2_WORD, CHG_TERM_CURR_v2_OFFSET, 1,
		100000, 390625, 0, fg_encode_current, NULL),
	PARAM(CHG_TERM_BASE_CURR, CHG_TERM_CURR_v2_WORD,
		CHG_TERM_BASE_CURR_v2_OFFSET, 1, 1024, 1000, 0,
		fg_encode_current, NULL),
	PARAM(CUTOFF_CURR, CUTOFF_CURR_WORD, CUTOFF_CURR_OFFSET, 3,
		1000000, 122070, 0, fg_encode_current, NULL),
	PARAM(DELTA_MSOC_THR, DELTA_MSOC_THR_v2_WORD, DELTA_MSOC_THR_v2_OFFSET,
		1, 2048, 100, 0, fg_encode_default, NULL),
	PARAM(DELTA_BSOC_THR, DELTA_BSOC_THR_v2_WORD, DELTA_BSOC_THR_v2_OFFSET,
		1, 2048, 100, 0, fg_encode_default, NULL),
	PARAM(RECHARGE_SOC_THR, RECHARGE_SOC_THR_v2_WORD,
		RECHARGE_SOC_THR_v2_OFFSET, 1, 256, 100, 0, fg_encode_default,
		NULL),
	PARAM(RECHARGE_VBATT_THR, RECHARGE_VBATT_THR_v2_WORD,
		RECHARGE_VBATT_THR_v2_OFFSET, 1, 1000, 15625, -2000,
		fg_encode_voltage, NULL),
	PARAM(ESR_TIMER_DISCHG_MAX, ESR_TIMER_DISCHG_MAX_WORD,
		ESR_TIMER_DISCHG_MAX_OFFSET, 2, 1, 1, 0, fg_encode_default,
		NULL),
	PARAM(ESR_TIMER_DISCHG_INIT, ESR_TIMER_DISCHG_INIT_WORD,
		ESR_TIMER_DISCHG_INIT_OFFSET, 2, 1, 1, 0, fg_encode_default,
		NULL),
	PARAM(ESR_TIMER_CHG_MAX, ESR_TIMER_CHG_MAX_WORD,
		ESR_TIMER_CHG_MAX_OFFSET, 2, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_TIMER_CHG_INIT, ESR_TIMER_CHG_INIT_WORD,
		ESR_TIMER_CHG_INIT_OFFSET, 2, 1, 1, 0, fg_encode_default, NULL),
	PARAM(ESR_PULSE_THRESH, ESR_PULSE_THRESH_WORD, ESR_PULSE_THRESH_OFFSET,
		1, 100000, 390625, 0, fg_encode_default, NULL),
	PARAM(KI_COEFF_MED_DISCHG, KI_COEFF_MED_DISCHG_v2_WORD,
		KI_COEFF_MED_DISCHG_v2_OFFSET, 1, 1000, 244141, 0,
		fg_encode_default, NULL),
	PARAM(KI_COEFF_HI_DISCHG, KI_COEFF_HI_DISCHG_v2_WORD,
		KI_COEFF_HI_DISCHG_v2_OFFSET, 1, 1000, 244141, 0,
		fg_encode_default, NULL),
	PARAM(KI_COEFF_FULL_SOC, KI_COEFF_FULL_SOC_WORD,
		KI_COEFF_FULL_SOC_OFFSET, 1, 1000, 244141, 0,
		fg_encode_default, NULL),
	PARAM(ESR_TIGHT_FILTER, ESR_FILTER_WORD, ESR_UPD_TIGHT_OFFSET,
		1, 512, 1000000, 0, fg_encode_default, NULL),
	PARAM(ESR_BROAD_FILTER, ESR_FILTER_WORD, ESR_UPD_BROAD_OFFSET,
		1, 512, 1000000, 0, fg_encode_default, NULL),
	PARAM(SLOPE_LIMIT, SLOPE_LIMIT_WORD, SLOPE_LIMIT_OFFSET, 1, 8192, 1000,
		0, fg_encode_default, NULL),
};

static struct fg_alg_flag pmi8998_v1_alg_flags[] = {
	[ALG_FLAG_SOC_LT_OTG_MIN]	= {
		.name	= "SOC_LT_OTG_MIN",
		.bit	= BIT(0),
	},
	[ALG_FLAG_SOC_LT_RECHARGE]	= {
		.name	= "SOC_LT_RECHARGE",
		.bit	= BIT(1),
	},
	[ALG_FLAG_IBATT_LT_ITERM]	= {
		.name	= "IBATT_LT_ITERM",
		.bit	= BIT(2),
	},
	[ALG_FLAG_IBATT_GT_HPM]		= {
		.name	= "IBATT_GT_HPM",
		.bit	= BIT(3),
	},
	[ALG_FLAG_IBATT_GT_UPM]		= {
		.name	= "IBATT_GT_UPM",
		.bit	= BIT(4),
	},
	[ALG_FLAG_VBATT_LT_RECHARGE]	= {
		.name	= "VBATT_LT_RECHARGE",
		.bit	= BIT(5),
	},
	[ALG_FLAG_VBATT_GT_VFLOAT]	= {
		.invalid = true,
	},
};

static struct fg_alg_flag pmi8998_v2_alg_flags[] = {
	[ALG_FLAG_SOC_LT_OTG_MIN]	= {
		.name	= "SOC_LT_OTG_MIN",
		.bit	= BIT(0),
	},
	[ALG_FLAG_SOC_LT_RECHARGE]	= {
		.name	= "SOC_LT_RECHARGE",
		.bit	= BIT(1),
	},
	[ALG_FLAG_IBATT_LT_ITERM]	= {
		.name	= "IBATT_LT_ITERM",
		.bit	= BIT(2),
	},
	[ALG_FLAG_IBATT_GT_HPM]		= {
		.name	= "IBATT_GT_HPM",
		.bit	= BIT(4),
	},
	[ALG_FLAG_IBATT_GT_UPM]		= {
		.name	= "IBATT_GT_UPM",
		.bit	= BIT(5),
	},
	[ALG_FLAG_VBATT_LT_RECHARGE]	= {
		.name	= "VBATT_LT_RECHARGE",
		.bit	= BIT(6),
	},
	[ALG_FLAG_VBATT_GT_VFLOAT]	= {
		.name	= "VBATT_GT_VFLOAT",
		.bit	= BIT(7),
	},
};

static int fg_gen3_debug_mask;
module_param_named(
	debug_mask, fg_gen3_debug_mask, int, S_IRUSR | S_IWUSR
);

static bool fg_profile_dump;
module_param_named(
	profile_dump, fg_profile_dump, bool, S_IRUSR | S_IWUSR
);

static int fg_sram_dump_period_ms = 20000;
module_param_named(
	sram_dump_period_ms, fg_sram_dump_period_ms, int, S_IRUSR | S_IWUSR
);

static int fg_restart;
static bool fg_sram_dump;
extern bool fg_batt_id_ready; //ASUS_BSP

/* All getters HERE */

#define VOLTAGE_15BIT_MASK	GENMASK(14, 0)
static int fg_decode_voltage_15b(struct fg_sram_param *sp,
				enum fg_sram_param_id id, int value)
{
	value &= VOLTAGE_15BIT_MASK;
	sp[id].value = div_u64((u64)value * sp[id].denmtr, sp[id].numrtr);
	pr_debug("id: %d raw value: %x decoded value: %x\n", id, value,
		sp[id].value);
	return sp[id].value;
}

static int fg_decode_cc_soc(struct fg_sram_param *sp,
				enum fg_sram_param_id id, int value)
{
	sp[id].value = div_s64((s64)value * sp[id].denmtr, sp[id].numrtr);
	sp[id].value = sign_extend32(sp[id].value, 31);
	pr_debug("id: %d raw value: %x decoded value: %x\n", id, value,
		sp[id].value);
	return sp[id].value;
}

static int fg_decode_value_16b(struct fg_sram_param *sp,
				enum fg_sram_param_id id, int value)
{
	sp[id].value = div_u64((u64)(u16)value * sp[id].denmtr, sp[id].numrtr);
	pr_debug("id: %d raw value: %x decoded value: %x\n", id, value,
		sp[id].value);
	return sp[id].value;
}

static int fg_decode_default(struct fg_sram_param *sp, enum fg_sram_param_id id,
				int value)
{
	sp[id].value = value;
	return sp[id].value;
}

static int fg_decode(struct fg_sram_param *sp, enum fg_sram_param_id id,
			int value)
{
	if (!sp[id].decode) {
		pr_err("No decoding function for parameter %d\n", id);
		return -EINVAL;
	}

	return sp[id].decode(sp, id, value);
}

static void fg_encode_voltage(struct fg_sram_param *sp,
				enum fg_sram_param_id  id, int val_mv, u8 *buf)
{
	int i, mask = 0xff;
	int64_t temp;

	val_mv += sp[id].offset;
	temp = (int64_t)div_u64((u64)val_mv * sp[id].numrtr, sp[id].denmtr);
	pr_debug("temp: %llx id: %d, val_mv: %d, buf: [ ", temp, id, val_mv);
	for (i = 0; i < sp[id].len; i++) {
		buf[i] = temp & mask;
		temp >>= 8;
		pr_debug("%x ", buf[i]);
	}
	pr_debug("]\n");
}

static void fg_encode_current(struct fg_sram_param *sp,
				enum fg_sram_param_id  id, int val_ma, u8 *buf)
{
	int i, mask = 0xff;
	int64_t temp;
	s64 current_ma;

	current_ma = val_ma;
	temp = (int64_t)div_s64(current_ma * sp[id].numrtr, sp[id].denmtr);
	pr_debug("temp: %llx id: %d, val: %d, buf: [ ", temp, id, val_ma);
	for (i = 0; i < sp[id].len; i++) {
		buf[i] = temp & mask;
		temp >>= 8;
		pr_debug("%x ", buf[i]);
	}
	pr_debug("]\n");
}

static void fg_encode_default(struct fg_sram_param *sp,
				enum fg_sram_param_id  id, int val, u8 *buf)
{
	int i, mask = 0xff;
	int64_t temp;

	temp = (int64_t)div_s64((s64)val * sp[id].numrtr, sp[id].denmtr);
	pr_debug("temp: %llx id: %d, val: %d, buf: [ ", temp, id, val);
	for (i = 0; i < sp[id].len; i++) {
		buf[i] = temp & mask;
		temp >>= 8;
		pr_debug("%x ", buf[i]);
	}
	pr_debug("]\n");
}

static void fg_encode(struct fg_sram_param *sp, enum fg_sram_param_id id,
			int val, u8 *buf)
{
	if (!sp[id].encode) {
		pr_err("No encoding function for parameter %d\n", id);
		return;
	}

	sp[id].encode(sp, id, val, buf);
}

/*
 * Please make sure *_sram_params table has the entry for the parameter
 * obtained through this function. In addition to address, offset,
 * length from where this SRAM parameter is read, a decode function
 * need to be specified.
 */
static int fg_get_sram_prop(struct fg_chip *chip, enum fg_sram_param_id id,
				int *val)
{
	int temp, rc, i;
	u8 buf[4];

	if (id < 0 || id > FG_SRAM_MAX || chip->sp[id].len > sizeof(buf))
		return -EINVAL;

	if (chip->battery_missing)
		return -ENODATA;

	rc = fg_sram_read(chip, chip->sp[id].addr_word, chip->sp[id].addr_byte,
		buf, chip->sp[id].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error reading address 0x%04x[%d] rc=%d\n",
			chip->sp[id].addr_word, chip->sp[id].addr_byte, rc);
		return rc;
	}

	for (i = 0, temp = 0; i < chip->sp[id].len; i++)
		temp |= buf[i] << (8 * i);

	*val = fg_decode(chip->sp, id, temp);
	return 0;
}

#define CC_SOC_30BIT	GENMASK(29, 0)
static int fg_get_charge_raw(struct fg_chip *chip, int *val)
{
	int rc, cc_soc;

	rc = fg_get_sram_prop(chip, FG_SRAM_CC_SOC, &cc_soc);
	if (rc < 0) {
		pr_err("Error in getting CC_SOC, rc=%d\n", rc);
		return rc;
	}

	*val = div_s64(cc_soc * chip->cl.nom_cap_uah, CC_SOC_30BIT);
	return 0;
}

#define BATT_SOC_32BIT	GENMASK(31, 0)
static int fg_get_charge_counter_shadow(struct fg_chip *chip, int *val)
{
	int rc, batt_soc;

	rc = fg_get_sram_prop(chip, FG_SRAM_BATT_SOC, &batt_soc);
	if (rc < 0) {
		pr_err("Error in getting BATT_SOC, rc=%d\n", rc);
		return rc;
	}

	*val = div_u64((u32)batt_soc * chip->cl.learned_cc_uah, BATT_SOC_32BIT);
	return 0;
}

static int fg_get_charge_counter(struct fg_chip *chip, int *val)
{
	int rc, cc_soc;

	rc = fg_get_sram_prop(chip, FG_SRAM_CC_SOC_SW, &cc_soc);
	if (rc < 0) {
		pr_err("Error in getting CC_SOC_SW, rc=%d\n", rc);
		return rc;
	}

	*val = div_s64(cc_soc * chip->cl.learned_cc_uah, CC_SOC_30BIT);
	return 0;
}

static int fg_get_jeita_threshold(struct fg_chip *chip,
				enum jeita_levels level, int *temp_decidegC)
{
	int rc;
	u8 val;
	u16 reg;

	switch (level) {
	case JEITA_COLD:
		reg = BATT_INFO_JEITA_TOO_COLD(chip);
		break;
	case JEITA_COOL:
		reg = BATT_INFO_JEITA_COLD(chip);
		break;
	case JEITA_WARM:
		reg = BATT_INFO_JEITA_HOT(chip);
		break;
	case JEITA_HOT:
		reg = BATT_INFO_JEITA_TOO_HOT(chip);
		break;
	default:
		return -EINVAL;
	}

	rc = fg_read(chip, reg, &val, 1);
	if (rc < 0) {
		pr_err("Error in reading jeita level %d, rc=%d\n", level, rc);
		return rc;
	}

	/* Resolution is 0.5C. Base is -30C. */
	*temp_decidegC = (((5 * val) / 10) - 30) * 10;
	return 0;
}

#define BATT_TEMP_NUMR		1
#define BATT_TEMP_DENR		1
static int fg_get_battery_temp(struct fg_chip *chip, int *val)
{
	int rc = 0, temp;
	u8 buf[2];

	rc = fg_read(chip, BATT_INFO_BATT_TEMP_LSB(chip), buf, 2);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			BATT_INFO_BATT_TEMP_LSB(chip), rc);
		return rc;
	}

	temp = ((buf[1] & BATT_TEMP_MSB_MASK) << 8) |
		(buf[0] & BATT_TEMP_LSB_MASK);
	temp = DIV_ROUND_CLOSEST(temp, 4);

	/* Value is in Kelvin; Convert it to deciDegC */
	temp = (temp - 273) * 10;
	*val = temp;
	return 0;
}

static int fg_get_battery_resistance(struct fg_chip *chip, int *val)
{
	int rc, esr_uohms, rslow_uohms;

	rc = fg_get_sram_prop(chip, FG_SRAM_ESR, &esr_uohms);
	if (rc < 0) {
		pr_err("failed to get ESR, rc=%d\n", rc);
		return rc;
	}

	rc = fg_get_sram_prop(chip, FG_SRAM_RSLOW, &rslow_uohms);
	if (rc < 0) {
		pr_err("failed to get Rslow, rc=%d\n", rc);
		return rc;
	}

	*val = esr_uohms + rslow_uohms;
	return 0;
}

#define BATT_CURRENT_NUMR	488281
#define BATT_CURRENT_DENR	1000
static int fg_get_battery_current(struct fg_chip *chip, int *val)
{
	int rc = 0;
	int64_t temp = 0;
	u8 buf[2];

	rc = fg_read(chip, BATT_INFO_IBATT_LSB(chip), buf, 2);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			BATT_INFO_IBATT_LSB(chip), rc);
		return rc;
	}

	if (chip->wa_flags & PMI8998_V1_REV_WA)
		temp = buf[0] << 8 | buf[1];
	else
		temp = buf[1] << 8 | buf[0];

	pr_debug("buf: %x %x temp: %llx\n", buf[0], buf[1], temp);
	/* Sign bit is bit 15 */
	temp = twos_compliment_extend(temp, 15);
	*val = div_s64((s64)temp * BATT_CURRENT_NUMR, BATT_CURRENT_DENR);
	return 0;
}

#define BATT_VOLTAGE_NUMR	122070
#define BATT_VOLTAGE_DENR	1000
static int fg_get_battery_voltage(struct fg_chip *chip, int *val)
{
	int rc = 0;
	u16 temp = 0;
	u8 buf[2];

	rc = fg_read(chip, BATT_INFO_VBATT_LSB(chip), buf, 2);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			BATT_INFO_VBATT_LSB(chip), rc);
		return rc;
	}

	if (chip->wa_flags & PMI8998_V1_REV_WA)
		temp = buf[0] << 8 | buf[1];
	else
		temp = buf[1] << 8 | buf[0];

	pr_debug("buf: %x %x temp: %x\n", buf[0], buf[1], temp);
	*val = div_u64((u64)temp * BATT_VOLTAGE_NUMR, BATT_VOLTAGE_DENR);
	return 0;
}

#define MAX_TRIES_SOC		5
static int fg_get_msoc_raw(struct fg_chip *chip, int *val)
{
	u8 cap[2];
	int rc, tries = 0;

	while (tries < MAX_TRIES_SOC) {
		rc = fg_read(chip, BATT_SOC_FG_MONOTONIC_SOC(chip), cap, 2);
		if (rc < 0) {
			pr_err("failed to read addr=0x%04x, rc=%d\n",
				BATT_SOC_FG_MONOTONIC_SOC(chip), rc);
			return rc;
		}

		if (cap[0] == cap[1])
			break;

		tries++;
	}

	if (tries == MAX_TRIES_SOC) {
		pr_err("shadow registers do not match\n");
		return -EINVAL;
	}

	fg_dbg(chip, FG_POWER_SUPPLY, "raw: 0x%02x\n", cap[0]);
	*val = cap[0];
	return 0;
}

#define FULL_CAPACITY	100
#define FULL_SOC_RAW	255
static int fg_get_msoc(struct fg_chip *chip, int *msoc)
{
	int rc;

	rc = fg_get_msoc_raw(chip, msoc);
	if (rc < 0)
		return rc;

	/*
	 * To have better endpoints for 0 and 100, it is good to tune the
	 * calculation discarding values 0 and 255 while rounding off. Rest
	 * of the values 1-254 will be scaled to 1-99. DIV_ROUND_UP will not
	 * be suitable here as it rounds up any value higher than 252 to 100.
	 */
	if (*msoc == FULL_SOC_RAW)
		*msoc = 100;
	else if (*msoc == 0)
		*msoc = 0;
	else
		*msoc = DIV_ROUND_CLOSEST((*msoc - 1) * (FULL_CAPACITY - 2),
				FULL_SOC_RAW - 2) + 1;
	return 0;
}

static bool is_batt_empty(struct fg_chip *chip)
{
	u8 status;
	int rc, vbatt_uv, msoc;

	rc = fg_read(chip, BATT_SOC_INT_RT_STS(chip), &status, 1);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			BATT_SOC_INT_RT_STS(chip), rc);
		return false;
	}

	if (!(status & MSOC_EMPTY_BIT))
		return false;

	rc = fg_get_battery_voltage(chip, &vbatt_uv);
	if (rc < 0) {
		pr_err("failed to get battery voltage, rc=%d\n", rc);
		return false;
	}

	rc = fg_get_msoc(chip, &msoc);
	if (!rc)
		pr_warn("batt_soc_rt_sts: %x vbatt: %d uV msoc:%d\n", status,
			vbatt_uv, msoc);

	return ((vbatt_uv < chip->dt.cutoff_volt_mv * 1000) ? true : false);
}

static int fg_get_debug_batt_id(struct fg_chip *chip, int *batt_id)
{
	int rc;
	u64 temp;
	u8 buf[2];

	rc = fg_read(chip, ADC_RR_FAKE_BATT_LOW_LSB(chip), buf, 2);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			ADC_RR_FAKE_BATT_LOW_LSB(chip), rc);
		return rc;
	}

	/*
	 * Fake battery threshold is encoded in the following format.
	 * Threshold (code) = (battery_id in Ohms) * 0.00015 * 2^10 / 2.5
	 */
	temp = (buf[1] << 8 | buf[0]) * 2500000;
	do_div(temp, 150 * 1024);
	batt_id[0] = temp;
	rc = fg_read(chip, ADC_RR_FAKE_BATT_HIGH_LSB(chip), buf, 2);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			ADC_RR_FAKE_BATT_HIGH_LSB(chip), rc);
		return rc;
	}

	temp = (buf[1] << 8 | buf[0]) * 2500000;
	do_div(temp, 150 * 1024);
	batt_id[1] = temp;
	pr_debug("debug batt_id range: [%d %d]\n", batt_id[0], batt_id[1]);
	return 0;
}

static bool is_debug_batt_id(struct fg_chip *chip)
{
	int debug_batt_id[2], rc;

	if (!chip->batt_id_ohms)
		return false;

	rc = fg_get_debug_batt_id(chip, debug_batt_id);
	if (rc < 0) {
		pr_err("Failed to get debug batt_id, rc=%d\n", rc);
		return false;
	}

	if (is_between(debug_batt_id[0], debug_batt_id[1],
		chip->batt_id_ohms)) {
		fg_dbg(chip, FG_POWER_SUPPLY, "Debug battery id: %dohms\n",
			chip->batt_id_ohms);
		return true;
	}

	return false;
}

#define DEBUG_BATT_SOC	67
#define BATT_MISS_SOC	50
#define EMPTY_SOC	0
bool	report_maint = false; //ASUS_BSP
static int fg_get_prop_capacity(struct fg_chip *chip, int *val)
{
	int rc, msoc, soc255; //ASUS_BSP

	if (is_debug_batt_id(chip)) {
		*val = DEBUG_BATT_SOC;
		BAT_DBG("%s: DEBUG_BATT_SOC(%d)\n",__func__,DEBUG_BATT_SOC); //ASUS_BSP
		return 0;
	}

	if (chip->fg_restarting) {
		*val = chip->last_soc;
		BAT_DBG("%s: chip->last_soc(%d)\n",__func__,chip->last_soc); //ASUS_BSP		
		return 0;
	}

	if (chip->battery_missing) {
		*val = BATT_MISS_SOC;
		BAT_DBG("%s: BATT_MISS_SOC(%d)\n",__func__,BATT_MISS_SOC); //ASUS_BSP	
		return 0;
	}

	if (is_batt_empty(chip)) {
		*val = EMPTY_SOC;
		BAT_DBG("%s: EMPTY_SOC(%d)\n",__func__,EMPTY_SOC); //ASUS_BSP	
		return 0;
	}

	if (chip->charge_full || chip->reporting_charge_full) { //ASUS_BSP
		*val = FULL_CAPACITY;
		return 0;
	}

	rc = fg_get_msoc(chip, &msoc);
	if (rc < 0)
		return rc;

//ASUS_BSP +++
	if (chip->dt.linearize_soc && chip->delta_soc > 0){
		if(report_maint)
			*val = DIV_ROUND_CLOSEST((chip->maint_soc) * FULL_CAPACITY, FULL_SOC_RAW);		
		else{
			fg_get_msoc_raw(chip, &soc255);
			*val = DIV_ROUND_CLOSEST((chip->delta_soc+soc255) * FULL_CAPACITY, FULL_SOC_RAW);
		}
	}
	else
		*val = msoc;

	//Since delta_soc+soc255 may excceed 255 due to delta_soc not modified yet.
	if(*val > FULL_CAPACITY)
		*val = FULL_CAPACITY;
//ASUS_BSP ---
	
	return 0;
}

#define DEFAULT_BATT_TYPE	"Unknown Battery"
#define MISSING_BATT_TYPE	"Missing Battery"
#define LOADING_BATT_TYPE	"Loading Battery"
static const char *fg_get_battery_type(struct fg_chip *chip)
{
	if (chip->battery_missing)
		return MISSING_BATT_TYPE;

	if (chip->bp.batt_type_str) {
		if (chip->profile_loaded)
			return chip->bp.batt_type_str;
		else if (chip->profile_available)
			return LOADING_BATT_TYPE;
	}

	return DEFAULT_BATT_TYPE;
}

static int fg_batt_missing_config(struct fg_chip *chip, bool enable)
{
	int rc;

	rc = fg_masked_write(chip, BATT_INFO_BATT_MISS_CFG(chip),
			BM_FROM_BATT_ID_BIT, enable ? BM_FROM_BATT_ID_BIT : 0);
	if (rc < 0)
		pr_err("Error in writing to %04x, rc=%d\n",
			BATT_INFO_BATT_MISS_CFG(chip), rc);
	return rc;
}

static int fg_get_batt_id(struct fg_chip *chip)
{
	int rc, ret, batt_id = 0;

	if (!chip->batt_id_chan)
		return -EINVAL;

	rc = fg_batt_missing_config(chip, false);
	if (rc < 0) {
		pr_err("Error in disabling BMD, rc=%d\n", rc);
		return rc;
	}

	rc = iio_read_channel_processed(chip->batt_id_chan, &batt_id);
	if (rc < 0) {
		pr_err("Error in reading batt_id channel, rc:%d\n", rc);
		goto out;
	}

	/* Wait for 200ms before enabling BMD again */
	msleep(200);

	fg_dbg(chip, FG_STATUS, "batt_id: %d\n", batt_id);
	chip->batt_id_ohms = batt_id;
out:
	ret = fg_batt_missing_config(chip, true);
	if (ret < 0) {
		pr_err("Error in enabling BMD, ret=%d\n", ret);
		return ret;
	}

	vote(chip->batt_miss_irq_en_votable, BATT_MISS_IRQ_VOTER, true, 0);
	return rc;
}

static int fg_get_batt_profile(struct fg_chip *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct device_node *batt_node, *profile_node;
	const char *data;
	int rc, len;

	batt_node = of_find_node_by_name(node, "qcom,battery-data");
	if (!batt_node) {
		pr_err("Batterydata not available\n");
		return -ENXIO;
	}

	profile_node = of_batterydata_get_best_profile(batt_node,
				chip->batt_id_ohms / 1000, NULL);
	if (IS_ERR(profile_node))
		return PTR_ERR(profile_node);

	if (!profile_node) {
		pr_err("couldn't find profile handle\n");
		return -ENODATA;
	}

	rc = of_property_read_string(profile_node, "qcom,battery-type",
			&chip->bp.batt_type_str);
	if (rc < 0) {
		pr_err("battery type unavailable, rc:%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(profile_node, "qcom,max-voltage-uv",
			&chip->bp.float_volt_uv);
	if (rc < 0) {
		pr_err("battery float voltage unavailable, rc:%d\n", rc);
		chip->bp.float_volt_uv = -EINVAL;
	}

	BAT_DBG("max-voltage %d\n",chip->bp.float_volt_uv); //ASUS_BSP
	rc = of_property_read_u32(profile_node, "qcom,fastchg-current-ma",
			&chip->bp.fastchg_curr_ma);
	if (rc < 0) {
		pr_err("battery fastchg current unavailable, rc:%d\n", rc);
		chip->bp.fastchg_curr_ma = -EINVAL;
	}

	rc = of_property_read_u32(profile_node, "qcom,fg-cc-cv-threshold-mv",
			&chip->bp.vbatt_full_mv);
	if (rc < 0) {
		pr_err("battery cc_cv threshold unavailable, rc:%d\n", rc);
		chip->bp.vbatt_full_mv = -EINVAL;
	}
	BAT_DBG("cc_cv %d\n",chip->bp.vbatt_full_mv); //ASUS_BSP

	data = of_get_property(profile_node, "qcom,fg-profile-data", &len);
	if (!data) {
		pr_err("No profile data available\n");
		return -ENODATA;
	}

	if (len != PROFILE_LEN) {
		pr_err("battery profile incorrect size: %d\n", len);
		return -EINVAL;
	}

	chip->profile_available = true;
	memcpy(chip->batt_profile, data, len);

	return 0;
}

static inline void get_batt_temp_delta(int delta, u8 *val)
{
	switch (delta) {
	case 2:
		*val = BTEMP_DELTA_2K;
		break;
	case 4:
		*val = BTEMP_DELTA_4K;
		break;
	case 6:
		*val = BTEMP_DELTA_6K;
		break;
	case 10:
		*val = BTEMP_DELTA_10K;
		break;
	default:
		*val = BTEMP_DELTA_2K;
		break;
	};
}

static inline void get_esr_meas_current(int curr_ma, u8 *val)
{
	switch (curr_ma) {
	case 60:
		*val = ESR_MEAS_CUR_60MA;
		break;
	case 120:
		*val = ESR_MEAS_CUR_120MA;
		break;
	case 180:
		*val = ESR_MEAS_CUR_180MA;
		break;
	case 240:
		*val = ESR_MEAS_CUR_240MA;
		break;
	default:
		*val = ESR_MEAS_CUR_120MA;
		break;
	};

	*val <<= ESR_PULL_DOWN_IVAL_SHIFT;
}

static int fg_set_esr_timer(struct fg_chip *chip, int cycles_init,
				int cycles_max, bool charging, int flags)
{
	u8 buf[2];
	int rc, timer_max, timer_init;

	if (cycles_init < 0 || cycles_max < 0)
		return 0;

	if (charging) {
		timer_max = FG_SRAM_ESR_TIMER_CHG_MAX;
		timer_init = FG_SRAM_ESR_TIMER_CHG_INIT;
	} else {
		timer_max = FG_SRAM_ESR_TIMER_DISCHG_MAX;
		timer_init = FG_SRAM_ESR_TIMER_DISCHG_INIT;
	}

	fg_encode(chip->sp, timer_max, cycles_max, buf);
	rc = fg_sram_write(chip,
			chip->sp[timer_max].addr_word,
			chip->sp[timer_max].addr_byte, buf,
			chip->sp[timer_max].len, flags);
	if (rc < 0) {
		pr_err("Error in writing esr_timer_dischg_max, rc=%d\n",
			rc);
		return rc;
	}

	fg_encode(chip->sp, timer_init, cycles_init, buf);
	rc = fg_sram_write(chip,
			chip->sp[timer_init].addr_word,
			chip->sp[timer_init].addr_byte, buf,
			chip->sp[timer_init].len, flags);
	if (rc < 0) {
		pr_err("Error in writing esr_timer_dischg_init, rc=%d\n",
			rc);
		return rc;
	}

	fg_dbg(chip, FG_STATUS, "esr_%s_timer set to %d/%d\n",
		charging ? "charging" : "discharging", cycles_init, cycles_max);
	return 0;
}

/* Other functions HERE */

static void fg_notify_charger(struct fg_chip *chip)
{
	union power_supply_propval prop = {0, };
	int rc;

	if (!chip->batt_psy)
		return;

	if (!chip->profile_available)
		return;

	prop.intval = chip->bp.float_volt_uv;
	rc = power_supply_set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, &prop);
	if (rc < 0) {
		pr_err("Error in setting voltage_max property on batt_psy, rc=%d\n",
			rc);
		return;
	}

	prop.intval = chip->bp.fastchg_curr_ma * 1000;
	rc = power_supply_set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &prop);
	if (rc < 0) {
		pr_err("Error in setting constant_charge_current_max property on batt_psy, rc=%d\n",
			rc);
		return;
	}

	fg_dbg(chip, FG_STATUS, "Notified charger on float voltage and FCC\n");
}

static int fg_batt_miss_irq_en_cb(struct votable *votable, void *data,
					int enable, const char *client)
{
	struct fg_chip *chip = data;

	if (!chip->irqs[BATT_MISSING_IRQ].irq)
		return 0;

	if (enable) {
		enable_irq(chip->irqs[BATT_MISSING_IRQ].irq);
		enable_irq_wake(chip->irqs[BATT_MISSING_IRQ].irq);
	} else {
		disable_irq_wake(chip->irqs[BATT_MISSING_IRQ].irq);
		disable_irq_nosync(chip->irqs[BATT_MISSING_IRQ].irq);
	}

	return 0;
}

static int fg_delta_bsoc_irq_en_cb(struct votable *votable, void *data,
					int enable, const char *client)
{
	struct fg_chip *chip = data;

	if (!chip->irqs[BSOC_DELTA_IRQ].irq)
		return 0;

	if (enable) {
		enable_irq(chip->irqs[BSOC_DELTA_IRQ].irq);
		enable_irq_wake(chip->irqs[BSOC_DELTA_IRQ].irq);
	} else {
		disable_irq_wake(chip->irqs[BSOC_DELTA_IRQ].irq);
		disable_irq_nosync(chip->irqs[BSOC_DELTA_IRQ].irq);
	}

	return 0;
}

static int fg_awake_cb(struct votable *votable, void *data, int awake,
			const char *client)
{
	struct fg_chip *chip = data;

	if (awake)
		pm_wakeup_event(chip->dev, 500);
	else
		pm_relax(chip->dev);

	pr_debug("client: %s awake: %d\n", client, awake);
	return 0;
}

static bool batt_psy_initialized(struct fg_chip *chip)
{
	if (chip->batt_psy)
		return true;

	chip->batt_psy = power_supply_get_by_name("battery");
	if (!chip->batt_psy)
		return false;

	/* batt_psy is initialized, set the fcc and fv */
	fg_notify_charger(chip);

	return true;
}

static bool usb_psy_initialized(struct fg_chip *chip)
{
	if (chip->usb_psy)
		return true;

	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy)
		return false;

	return true;
}

static bool pc_port_psy_initialized(struct fg_chip *chip)
{
	if (chip->pc_port_psy)
		return true;

	chip->pc_port_psy = power_supply_get_by_name("pc_port");
	if (!chip->pc_port_psy)
		return false;

	return true;
}

static bool dc_psy_initialized(struct fg_chip *chip)
{
	if (chip->dc_psy)
		return true;

	chip->dc_psy = power_supply_get_by_name("dc");
	if (!chip->dc_psy)
		return false;

	return true;
}

static bool is_parallel_charger_available(struct fg_chip *chip)
{
	if (!chip->parallel_psy)
		chip->parallel_psy = power_supply_get_by_name("parallel");

	if (!chip->parallel_psy)
		return false;

	return true;
}

static int fg_prime_cc_soc_sw(struct fg_chip *chip, int cc_soc_sw)
{
	int rc;

	rc = fg_sram_write(chip, chip->sp[FG_SRAM_CC_SOC_SW].addr_word,
		chip->sp[FG_SRAM_CC_SOC_SW].addr_byte, (u8 *)&cc_soc_sw,
		chip->sp[FG_SRAM_CC_SOC_SW].len, FG_IMA_ATOMIC);
	if (rc < 0)
		pr_err("Error in writing cc_soc_sw, rc=%d\n", rc);
	else
		fg_dbg(chip, FG_STATUS, "cc_soc_sw: %x\n", cc_soc_sw);

	return rc;
}

static int fg_save_learned_cap_to_sram(struct fg_chip *chip)
{
	int16_t cc_mah;
	int rc;

	if (chip->battery_missing || !chip->cl.learned_cc_uah)
		return -EPERM;

	cc_mah = div64_s64(chip->cl.learned_cc_uah, 1000);
	/* Write to a backup register to use across reboot */
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_ACT_BATT_CAP].addr_word,
			chip->sp[FG_SRAM_ACT_BATT_CAP].addr_byte, (u8 *)&cc_mah,
			chip->sp[FG_SRAM_ACT_BATT_CAP].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing act_batt_cap_bkup, rc=%d\n", rc);
		return rc;
	}

	/* Write to actual capacity register for coulomb counter operation */
	rc = fg_sram_write(chip, ACT_BATT_CAP_WORD, ACT_BATT_CAP_OFFSET,
			(u8 *)&cc_mah, chip->sp[FG_SRAM_ACT_BATT_CAP].len,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing act_batt_cap, rc=%d\n", rc);
		return rc;
	}

	fg_dbg(chip, FG_CAP_LEARN, "learned capacity %llduah/%dmah stored\n",
		chip->cl.learned_cc_uah, cc_mah);
	return 0;
}

#define CAPACITY_DELTA_DECIPCT	500
static int fg_load_learned_cap_from_sram(struct fg_chip *chip)
{
	int rc, act_cap_mah;
	int64_t delta_cc_uah, pct_nom_cap_uah;

	rc = fg_get_sram_prop(chip, FG_SRAM_ACT_BATT_CAP, &act_cap_mah);
	if (rc < 0) {
		pr_err("Error in getting ACT_BATT_CAP, rc=%d\n", rc);
		return rc;
	}

	chip->cl.learned_cc_uah = act_cap_mah * 1000;

	if (chip->cl.learned_cc_uah != chip->cl.nom_cap_uah) {
		if (chip->cl.learned_cc_uah == 0)
			chip->cl.learned_cc_uah = chip->cl.nom_cap_uah;

		delta_cc_uah = abs(chip->cl.learned_cc_uah -
					chip->cl.nom_cap_uah);
		pct_nom_cap_uah = div64_s64((int64_t)chip->cl.nom_cap_uah *
				CAPACITY_DELTA_DECIPCT, 1000);
		/*
		 * If the learned capacity is out of range by 50% from the
		 * nominal capacity, then overwrite the learned capacity with
		 * the nominal capacity.
		 */
		if (chip->cl.nom_cap_uah && delta_cc_uah > pct_nom_cap_uah) {
			fg_dbg(chip, FG_CAP_LEARN, "learned_cc_uah: %lld is higher than expected, capping it to nominal: %lld\n",
				chip->cl.learned_cc_uah, chip->cl.nom_cap_uah);
			chip->cl.learned_cc_uah = chip->cl.nom_cap_uah;
		}

		rc = fg_save_learned_cap_to_sram(chip);
		if (rc < 0)
			pr_err("Error in saving learned_cc_uah, rc=%d\n", rc);
	}

	fg_dbg(chip, FG_CAP_LEARN, "learned_cc_uah:%lld nom_cap_uah: %lld\n",
		chip->cl.learned_cc_uah, chip->cl.nom_cap_uah);
	return 0;
}

static bool is_temp_valid_cap_learning(struct fg_chip *chip)
{
	int rc, batt_temp;

	rc = fg_get_battery_temp(chip, &batt_temp);
	if (rc < 0) {
		pr_err("Error in getting batt_temp\n");
		return false;
	}

	if (batt_temp > chip->dt.cl_max_temp ||
		batt_temp < chip->dt.cl_min_temp) {
		fg_dbg(chip, FG_CAP_LEARN, "batt temp %d out of range [%d %d]\n",
			batt_temp, chip->dt.cl_min_temp, chip->dt.cl_max_temp);
		return false;
	}

	return true;
}

#define QNOVO_CL_SKEW_DECIPCT	-30
static void fg_cap_learning_post_process(struct fg_chip *chip)
{
	int64_t max_inc_val, min_dec_val, old_cap;
	int rc;

	if (is_qnovo_en(chip)) {
		fg_dbg(chip, FG_CAP_LEARN, "applying skew %d on current learnt capacity %lld\n",
			QNOVO_CL_SKEW_DECIPCT, chip->cl.final_cc_uah);
		chip->cl.final_cc_uah = chip->cl.final_cc_uah *
						(1000 + QNOVO_CL_SKEW_DECIPCT);
		do_div(chip->cl.final_cc_uah, 1000);
	}

	max_inc_val = chip->cl.learned_cc_uah
			* (1000 + chip->dt.cl_max_cap_inc);
	do_div(max_inc_val, 1000);

	min_dec_val = chip->cl.learned_cc_uah
			* (1000 - chip->dt.cl_max_cap_dec);
	do_div(min_dec_val, 1000);

	old_cap = chip->cl.learned_cc_uah;
	if (chip->cl.final_cc_uah > max_inc_val)
		chip->cl.learned_cc_uah = max_inc_val;
	else if (chip->cl.final_cc_uah < min_dec_val)
		chip->cl.learned_cc_uah = min_dec_val;
	else
		chip->cl.learned_cc_uah =
			chip->cl.final_cc_uah;

	if (chip->dt.cl_max_cap_limit) {
		max_inc_val = (int64_t)chip->cl.nom_cap_uah * (1000 +
				chip->dt.cl_max_cap_limit);
		do_div(max_inc_val, 1000);
		if (chip->cl.final_cc_uah > max_inc_val) {
			fg_dbg(chip, FG_CAP_LEARN, "learning capacity %lld goes above max limit %lld\n",
				chip->cl.final_cc_uah, max_inc_val);
			chip->cl.learned_cc_uah = max_inc_val;
		}
	}

	if (chip->dt.cl_min_cap_limit) {
		min_dec_val = (int64_t)chip->cl.nom_cap_uah * (1000 -
				chip->dt.cl_min_cap_limit);
		do_div(min_dec_val, 1000);
		if (chip->cl.final_cc_uah < min_dec_val) {
			fg_dbg(chip, FG_CAP_LEARN, "learning capacity %lld goes below min limit %lld\n",
				chip->cl.final_cc_uah, min_dec_val);
			chip->cl.learned_cc_uah = min_dec_val;
		}
	}

	rc = fg_save_learned_cap_to_sram(chip);
	if (rc < 0)
		pr_err("Error in saving learned_cc_uah, rc=%d\n", rc);

	fg_dbg(chip, FG_CAP_LEARN, "final cc_uah = %lld, learned capacity %lld -> %lld uah\n",
		chip->cl.final_cc_uah, old_cap, chip->cl.learned_cc_uah);
}

static int fg_cap_learning_process_full_data(struct fg_chip *chip)
{
	int rc, cc_soc_sw, cc_soc_delta_pct;
	int64_t delta_cc_uah;

	rc = fg_get_sram_prop(chip, FG_SRAM_CC_SOC_SW, &cc_soc_sw);
	if (rc < 0) {
		pr_err("Error in getting CC_SOC_SW, rc=%d\n", rc);
		return rc;
	}

	cc_soc_delta_pct =
		div64_s64((int64_t)(cc_soc_sw - chip->cl.init_cc_soc_sw) * 100,
			CC_SOC_30BIT);

	/* If the delta is < 50%, then skip processing full data */
	if (cc_soc_delta_pct < 50) {
		pr_err("cc_soc_delta_pct: %d\n", cc_soc_delta_pct);
		return -ERANGE;
	}

	delta_cc_uah = div64_s64(chip->cl.learned_cc_uah * cc_soc_delta_pct,
				100);
	chip->cl.final_cc_uah = chip->cl.init_cc_uah + delta_cc_uah;
	fg_dbg(chip, FG_CAP_LEARN, "Current cc_soc=%d cc_soc_delta_pct=%d total_cc_uah=%lld\n",
		cc_soc_sw, cc_soc_delta_pct, chip->cl.final_cc_uah);
	return 0;
}

static int fg_cap_learning_begin(struct fg_chip *chip, u32 batt_soc)
{
	int rc, cc_soc_sw, batt_soc_msb;

	batt_soc_msb = batt_soc >> 24;
	if (DIV_ROUND_CLOSEST(batt_soc_msb * 100, FULL_SOC_RAW) >
		chip->dt.cl_start_soc) {
		fg_dbg(chip, FG_CAP_LEARN, "Battery SOC %d is high!, not starting\n",
			batt_soc_msb);
		return -EINVAL;
	}

	chip->cl.init_cc_uah = div64_s64(chip->cl.learned_cc_uah * batt_soc_msb,
					FULL_SOC_RAW);

	/* Prime cc_soc_sw with battery SOC when capacity learning begins */
	cc_soc_sw = div64_s64((int64_t)batt_soc * CC_SOC_30BIT,
				BATT_SOC_32BIT);
	rc = fg_prime_cc_soc_sw(chip, cc_soc_sw);
	if (rc < 0) {
		pr_err("Error in writing cc_soc_sw, rc=%d\n", rc);
		goto out;
	}

	chip->cl.init_cc_soc_sw = cc_soc_sw;
	fg_dbg(chip, FG_CAP_LEARN, "Capacity learning started @ battery SOC %d init_cc_soc_sw:%d\n",
		batt_soc_msb, chip->cl.init_cc_soc_sw);
out:
	return rc;
}

static int fg_cap_learning_done(struct fg_chip *chip)
{
	int rc, cc_soc_sw;

	rc = fg_cap_learning_process_full_data(chip);
	if (rc < 0) {
		pr_err("Error in processing cap learning full data, rc=%d\n",
			rc);
		goto out;
	}

	/* Write a FULL value to cc_soc_sw */
	cc_soc_sw = CC_SOC_30BIT;
	rc = fg_prime_cc_soc_sw(chip, cc_soc_sw);
	if (rc < 0) {
		pr_err("Error in writing cc_soc_sw, rc=%d\n", rc);
		goto out;
	}

	fg_cap_learning_post_process(chip);
out:
	return rc;
}

static void fg_cap_learning_update(struct fg_chip *chip)
{
	int rc, batt_soc, batt_soc_msb, cc_soc_sw;
	bool input_present = is_input_present(chip);
	bool prime_cc = false;

	mutex_lock(&chip->cl.lock);

	if (!is_temp_valid_cap_learning(chip) || !chip->cl.learned_cc_uah ||
		chip->battery_missing) {
		fg_dbg(chip, FG_CAP_LEARN, "Aborting cap_learning %lld\n",
			chip->cl.learned_cc_uah);
		chip->cl.active = false;
		chip->cl.init_cc_uah = 0;
		goto out;
	}

	if (chip->charge_status == chip->prev_charge_status)
		goto out;

	rc = fg_get_sram_prop(chip, FG_SRAM_BATT_SOC, &batt_soc);
	if (rc < 0) {
		pr_err("Error in getting ACT_BATT_CAP, rc=%d\n", rc);
		goto out;
	}

	batt_soc_msb = (u32)batt_soc >> 24;
	fg_dbg(chip, FG_CAP_LEARN, "Chg_status: %d cl_active: %d batt_soc: %d\n",
		chip->charge_status, chip->cl.active, batt_soc_msb);

	/* Initialize the starting point of learning capacity */
	if (!chip->cl.active) {
		if (chip->charge_status == POWER_SUPPLY_STATUS_CHARGING || chip->charge_status == POWER_SUPPLY_STATUS_QUICK_CHARGING  //ASUS_BSP
			|| chip->charge_status == POWER_SUPPLY_STATUS_NOT_QUICK_CHARGING) { //ASUS_BSP
			rc = fg_cap_learning_begin(chip, batt_soc);
			chip->cl.active = (rc == 0);
		} else {
			if ((chip->charge_status ==
					POWER_SUPPLY_STATUS_DISCHARGING) ||
					chip->charge_done)
				prime_cc = true;
		}
	} else {
		if (chip->charge_done) {
			rc = fg_cap_learning_done(chip);
			if (rc < 0)
				pr_err("Error in completing capacity learning, rc=%d\n",
					rc);

			chip->cl.active = false;
			chip->cl.init_cc_uah = 0;
		}

		if (chip->charge_status == POWER_SUPPLY_STATUS_DISCHARGING) {
			if (!input_present) {
				fg_dbg(chip, FG_CAP_LEARN, "Capacity learning aborted @ battery SOC %d\n",
					 batt_soc_msb);
				chip->cl.active = false;
				chip->cl.init_cc_uah = 0;
				prime_cc = true;
			}
		}

		if (chip->charge_status == POWER_SUPPLY_STATUS_NOT_CHARGING) {
			if (is_qnovo_en(chip) && input_present) {
				/*
				 * Don't abort the capacity learning when qnovo
				 * is enabled and input is present where the
				 * charging status can go to "not charging"
				 * intermittently.
				 */
			} else {
				fg_dbg(chip, FG_CAP_LEARN, "Capacity learning aborted @ battery SOC %d\n",
					batt_soc_msb);
				chip->cl.active = false;
				chip->cl.init_cc_uah = 0;
				prime_cc = true;
			}
		}
	}

	/*
	 * Prime CC_SOC_SW when the device is not charging or during charge
	 * termination when the capacity learning is not active.
	 */

	if (prime_cc) {
		if (chip->charge_done)
			cc_soc_sw = CC_SOC_30BIT;
		else
			cc_soc_sw = div_u64((u32)batt_soc *
					CC_SOC_30BIT, BATT_SOC_32BIT);

		rc = fg_prime_cc_soc_sw(chip, cc_soc_sw);
		if (rc < 0)
			pr_err("Error in writing cc_soc_sw, rc=%d\n",
				rc);
	}

out:
	mutex_unlock(&chip->cl.lock);
}

#define KI_COEFF_MED_DISCHG_DEFAULT	1500
#define KI_COEFF_HI_DISCHG_DEFAULT	2200
static int fg_adjust_ki_coeff_dischg(struct fg_chip *chip)
{
	int rc, i, msoc;
	int ki_coeff_med = KI_COEFF_MED_DISCHG_DEFAULT;
	int ki_coeff_hi = KI_COEFF_HI_DISCHG_DEFAULT;
	u8 val;

	if (!chip->ki_coeff_dischg_en)
		return 0;

	rc = fg_get_prop_capacity(chip, &msoc);
	if (rc < 0) {
		pr_err("Error in getting capacity, rc=%d\n", rc);
		return rc;
	}

	if (chip->charge_status == POWER_SUPPLY_STATUS_DISCHARGING) {
		for (i = KI_COEFF_SOC_LEVELS - 1; i >= 0; i--) {
			if (msoc < chip->dt.ki_coeff_soc[i]) {
				ki_coeff_med = chip->dt.ki_coeff_med_dischg[i];
				ki_coeff_hi = chip->dt.ki_coeff_hi_dischg[i];
			}
		}
	}

	fg_encode(chip->sp, FG_SRAM_KI_COEFF_MED_DISCHG, ki_coeff_med, &val);
	rc = fg_sram_write(chip,
			chip->sp[FG_SRAM_KI_COEFF_MED_DISCHG].addr_word,
			chip->sp[FG_SRAM_KI_COEFF_MED_DISCHG].addr_byte, &val,
			chip->sp[FG_SRAM_KI_COEFF_MED_DISCHG].len,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ki_coeff_med, rc=%d\n", rc);
		return rc;
	}

	fg_encode(chip->sp, FG_SRAM_KI_COEFF_HI_DISCHG, ki_coeff_hi, &val);
	rc = fg_sram_write(chip,
			chip->sp[FG_SRAM_KI_COEFF_HI_DISCHG].addr_word,
			chip->sp[FG_SRAM_KI_COEFF_HI_DISCHG].addr_byte, &val,
			chip->sp[FG_SRAM_KI_COEFF_HI_DISCHG].len,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ki_coeff_hi, rc=%d\n", rc);
		return rc;
	}

	fg_dbg(chip, FG_STATUS, "Wrote ki_coeff_med %d ki_coeff_hi %d\n",
		ki_coeff_med, ki_coeff_hi);
	return 0;
}

#define KI_COEFF_FULL_SOC_DEFAULT	733
static int fg_adjust_ki_coeff_full_soc(struct fg_chip *chip, int batt_temp)
{
	int rc, ki_coeff_full_soc;
	u8 val;

	if (batt_temp < 0)
		ki_coeff_full_soc = 0;
	else if (chip->charge_status == POWER_SUPPLY_STATUS_DISCHARGING)
		ki_coeff_full_soc = chip->dt.ki_coeff_full_soc_dischg;
	else
		ki_coeff_full_soc = KI_COEFF_FULL_SOC_DEFAULT;

	if (chip->ki_coeff_full_soc == ki_coeff_full_soc)
		return 0;

	fg_encode(chip->sp, FG_SRAM_KI_COEFF_FULL_SOC, ki_coeff_full_soc, &val);
	rc = fg_sram_write(chip,
			chip->sp[FG_SRAM_KI_COEFF_FULL_SOC].addr_word,
			chip->sp[FG_SRAM_KI_COEFF_FULL_SOC].addr_byte, &val,
			chip->sp[FG_SRAM_KI_COEFF_FULL_SOC].len,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ki_coeff_full_soc, rc=%d\n", rc);
		return rc;
	}

	chip->ki_coeff_full_soc = ki_coeff_full_soc;
	fg_dbg(chip, FG_STATUS, "Wrote ki_coeff_full_soc %d\n",
		ki_coeff_full_soc);
	return 0;
}

static int fg_set_recharge_voltage(struct fg_chip *chip, int voltage_mv)
{
	u8 buf;
	int rc;

	if (chip->dt.auto_recharge_soc)
		return 0;

	/* This configuration is available only for pmicobalt v2.0 and above */
	if (chip->wa_flags & PMI8998_V1_REV_WA)
		return 0;

	if (voltage_mv == chip->last_recharge_volt_mv)
		return 0;

	fg_dbg(chip, FG_STATUS, "Setting recharge voltage to %dmV\n",
		voltage_mv);
	fg_encode(chip->sp, FG_SRAM_RECHARGE_VBATT_THR, voltage_mv, &buf);
	rc = fg_sram_write(chip,
			chip->sp[FG_SRAM_RECHARGE_VBATT_THR].addr_word,
			chip->sp[FG_SRAM_RECHARGE_VBATT_THR].addr_byte,
			&buf, chip->sp[FG_SRAM_RECHARGE_VBATT_THR].len,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing recharge_vbatt_thr, rc=%d\n",
			rc);
		return rc;
	}

	chip->last_recharge_volt_mv = voltage_mv;
	return 0;
}

static int fg_configure_full_soc(struct fg_chip *chip, int bsoc)
{
	int rc;
	u8 full_soc[2] = {0xFF, 0xFF};

	/*
	 * Once SOC masking condition is cleared, FULL_SOC and MONOTONIC_SOC
	 * needs to be updated to reflect the same. Write battery SOC to
	 * FULL_SOC and write a full value to MONOTONIC_SOC.
	 */
	rc = fg_sram_write(chip, FULL_SOC_WORD, FULL_SOC_OFFSET,
			(u8 *)&bsoc, 2, FG_IMA_ATOMIC);
	if (rc < 0) {
		pr_err("failed to write full_soc rc=%d\n", rc);
		return rc;
	}

	rc = fg_sram_write(chip, MONOTONIC_SOC_WORD, MONOTONIC_SOC_OFFSET,
			full_soc, 2, FG_IMA_ATOMIC);
	if (rc < 0) {
		pr_err("failed to write monotonic_soc rc=%d\n", rc);
		return rc;
	}

	return 0;
}

//ASUS_BSP +++
void backup_delta_soc(int delta_soc)
{
	g_asusCapacityShift = delta_soc;
	schedule_delayed_work(&backup_asus_capacity_work,0);
}
//ASUS_BSP ---

#define AUTO_RECHG_VOLT_LOW_LIMIT_MV	3700
#define MAINT_SOC_DELTA_LIMIT	20 //ASUS_BSP this should larger than 255 - DEFAULT_ASUS_FULL_MSOC_THR

static int fg_charge_full_update(struct fg_chip *chip)
{
	union power_supply_propval prop = {0, };
	int rc, msoc, bsoc, recharge_soc, msoc_raw,msoc2; //ASUS_BSP

	if (!chip->dt.hold_soc_while_full)
		return 0;

	if (!batt_psy_initialized(chip))
		return 0;

	mutex_lock(&chip->charge_full_lock);
	vote(chip->delta_bsoc_irq_en_votable, DELTA_BSOC_IRQ_VOTER,
		chip->charge_done, 0);
	rc = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_HEALTH,
		&prop);
	if (rc < 0) {
		pr_err("Error in getting battery health, rc=%d\n", rc);
		goto out;
	}

	chip->health = prop.intval;
	recharge_soc = chip->dt.recharge_soc_thr;
	recharge_soc = DIV_ROUND_CLOSEST(recharge_soc * FULL_SOC_RAW,
				FULL_CAPACITY);
	recharge_soc =  DEFAULT_ASUS_FULL_MSOC_THR; //ASUS_BSP cover it
	rc = fg_get_sram_prop(chip, FG_SRAM_BATT_SOC, &bsoc);
	if (rc < 0) {
		pr_err("Error in getting BATT_SOC, rc=%d\n", rc);
		goto out;
	}

	/* We need 2 most significant bytes here */
	bsoc = (u32)bsoc >> 16;
	rc = fg_get_msoc(chip, &msoc); //ASUS_BSP
	if (rc < 0) {
		pr_err("Error in getting msoc, rc=%d\n", rc); //ASUS_BSP
		goto out;
	}
	msoc_raw = DIV_ROUND_CLOSEST(msoc * FULL_SOC_RAW, FULL_CAPACITY); //ASUS_BSP

//ASUS_BSP +++
	if(msoc == FULL_CAPACITY && !chip->reporting_charge_full){
		chip->reporting_charge_full = true;
		BAT_DBG("Setting reporting_charge_full to true\n");
	}
	
	fg_get_msoc_raw(chip, &msoc2);
	
	BAT_DBG("msoc: %d bsoc: %d msoc_255: %d health: %d status: %d chg-full:%d repo-full:%d delta:%d maint:%d msoc_raw:%d recharge_soc:%d\n",
		msoc, bsoc >> 8, msoc2, chip->health, chip->charge_status,
		chip->charge_full,chip->reporting_charge_full,chip->delta_soc ,chip->maint_soc,msoc_raw,recharge_soc);
//ASUS_BSP ---
	if (chip->charge_done && !chip->charge_full) {
		if (msoc >= 99 && chip->health == POWER_SUPPLY_HEALTH_GOOD) {
			BAT_DBG("Setting charge_full and reporting_charge_full to true\n"); //ASUS_BSP
			chip->charge_full = true;
			chip->reporting_charge_full = true; //ASUS_BSP
			/*
			 * Lower the recharge voltage so that VBAT_LT_RECHG
			 * signal will not be asserted soon.
			 */
			rc = fg_set_recharge_voltage(chip,
					AUTO_RECHG_VOLT_LOW_LIMIT_MV);
			if (rc < 0) {
				pr_err("Error in reducing recharge voltage, rc=%d\n",
					rc);
				goto out;
			}
		} else {
			fg_dbg(chip, FG_STATUS, "Terminated charging @ SOC%d\n",
				msoc);
		}
	} else if (msoc_raw <= recharge_soc && (chip->charge_full||chip->reporting_charge_full)) { //ASUS_BSP
		if (chip->dt.linearize_soc) {
			chip->delta_soc = FULL_SOC_RAW - msoc2;

			/*
			 * We're spreading out the delta SOC over every 10%
			 * change in monotonic SOC. We cannot spread more than
			 * 9% in the range of 0-100 skipping the first 10%.
			 */
		if (chip->delta_soc > MAINT_SOC_DELTA_LIMIT) { //ASUS_BSP
				chip->delta_soc = 0;
				chip->maint_soc = 0;
			} else {
			chip->maint_soc = FULL_SOC_RAW; //ASUS_BSP
			chip->last_msoc = msoc2; //ASUS_BSP
		}

//ASUS_BSP +++
		chip->charge_full = false;
		chip->reporting_charge_full = false;

		backup_delta_soc(chip->delta_soc);
//ASUS_BSP ---

		/*
		 * Raise the recharge voltage so that VBAT_LT_RECHG signal
		 * will be asserted soon as battery SOC had dropped below
		 * the recharge SOC threshold.
		 */
		rc = fg_set_recharge_voltage(chip,
					chip->dt.recharge_volt_thr_mv);
		if (rc < 0) {
			pr_err("Error in setting recharge voltage, rc=%d\n",
				rc);
			goto out;
		}

		/*
		 * If charge_done is still set, wait for recharging or
		 * discharging to happen.
		 */
		if (!chip->charge_full) //ASUS_BSP
			goto out;
		}

		rc = fg_configure_full_soc(chip, bsoc);
		if (rc < 0)
			goto out;

		BAT_DBG("trigger keeping 100%%. bsoc: %d recharge_soc: %d delta_soc: %d\n",
			bsoc >> 8, recharge_soc, chip->delta_soc);
	} 
//ASUS_BSP +++
	else if(chip->charge_full||chip->reporting_charge_full){
		//calc delta before trigger, for case such as 255 drop to 252 then reboot
		if(g_asusCapacityShift != FULL_SOC_RAW - msoc2)
			backup_delta_soc(FULL_SOC_RAW - msoc2);
	}
	else {
		goto out;
	}
	BAT_DBG("Set charge_full to true @ soc %d\n", msoc);
//ASUS_BSP ---

out:
	mutex_unlock(&chip->charge_full_lock);
	return rc;
}

#define RCONN_CONFIG_BIT	BIT(0)
static int fg_rconn_config(struct fg_chip *chip)
{
	int rc, esr_uohms;
	u64 scaling_factor;
	u32 val = 0;

	if (!chip->dt.rconn_mohms)
		return 0;

	rc = fg_sram_read(chip, PROFILE_INTEGRITY_WORD,
			SW_CONFIG_OFFSET, (u8 *)&val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading SW_CONFIG_OFFSET, rc=%d\n", rc);
		return rc;
	}

	if (val & RCONN_CONFIG_BIT) {
		fg_dbg(chip, FG_STATUS, "Rconn already configured: %x\n", val);
		return 0;
	}

	rc = fg_get_sram_prop(chip, FG_SRAM_ESR, &esr_uohms);
	if (rc < 0) {
		pr_err("failed to get ESR, rc=%d\n", rc);
		return rc;
	}

	scaling_factor = div64_u64((u64)esr_uohms * 1000,
				esr_uohms + (chip->dt.rconn_mohms * 1000));

	rc = fg_sram_read(chip, ESR_RSLOW_CHG_WORD,
			ESR_RSLOW_CHG_OFFSET, (u8 *)&val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading ESR_RSLOW_CHG_OFFSET, rc=%d\n", rc);
		return rc;
	}

	val *= scaling_factor;
	do_div(val, 1000);
	rc = fg_sram_write(chip, ESR_RSLOW_CHG_WORD,
			ESR_RSLOW_CHG_OFFSET, (u8 *)&val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR_RSLOW_CHG_OFFSET, rc=%d\n", rc);
		return rc;
	}
	fg_dbg(chip, FG_STATUS, "esr_rslow_chg modified to %x\n", val & 0xFF);

	rc = fg_sram_read(chip, ESR_RSLOW_DISCHG_WORD,
			ESR_RSLOW_DISCHG_OFFSET, (u8 *)&val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading ESR_RSLOW_DISCHG_OFFSET, rc=%d\n", rc);
		return rc;
	}

	val *= scaling_factor;
	do_div(val, 1000);
	rc = fg_sram_write(chip, ESR_RSLOW_DISCHG_WORD,
			ESR_RSLOW_DISCHG_OFFSET, (u8 *)&val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR_RSLOW_DISCHG_OFFSET, rc=%d\n", rc);
		return rc;
	}
	fg_dbg(chip, FG_STATUS, "esr_rslow_dischg modified to %x\n",
		val & 0xFF);

	val = RCONN_CONFIG_BIT;
	rc = fg_sram_write(chip, PROFILE_INTEGRITY_WORD,
			SW_CONFIG_OFFSET, (u8 *)&val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing SW_CONFIG_OFFSET, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int fg_set_jeita_threshold(struct fg_chip *chip,
				enum jeita_levels level, int temp_decidegC)
{
	int rc;
	u8 val;
	u16 reg;

	if (temp_decidegC < -300 || temp_decidegC > 970)
		return -EINVAL;

	/* Resolution is 0.5C. Base is -30C. */
	val = DIV_ROUND_CLOSEST(((temp_decidegC / 10) + 30) * 10, 5);
	switch (level) {
	case JEITA_COLD:
		reg = BATT_INFO_JEITA_TOO_COLD(chip);
		break;
	case JEITA_COOL:
		reg = BATT_INFO_JEITA_COLD(chip);
		break;
	case JEITA_WARM:
		reg = BATT_INFO_JEITA_HOT(chip);
		break;
	case JEITA_HOT:
		reg = BATT_INFO_JEITA_TOO_HOT(chip);
		break;
	default:
		return -EINVAL;
	}

	rc = fg_write(chip, reg, &val, 1);
	if (rc < 0) {
		pr_err("Error in setting jeita level %d, rc=%d\n", level, rc);
		return rc;
	}

	return 0;
}

static int fg_set_constant_chg_voltage(struct fg_chip *chip, int volt_uv)
{
	u8 buf[2];
	int rc;

	if (volt_uv <= 0 || volt_uv > 15590000) {
		pr_err("Invalid voltage %d\n", volt_uv);
		return -EINVAL;
	}

	fg_encode(chip->sp, FG_SRAM_VBATT_FULL, volt_uv, buf);

	rc = fg_sram_write(chip, chip->sp[FG_SRAM_VBATT_FULL].addr_word,
		chip->sp[FG_SRAM_VBATT_FULL].addr_byte, buf,
		chip->sp[FG_SRAM_VBATT_FULL].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing vbatt_full, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int fg_set_recharge_soc(struct fg_chip *chip, int recharge_soc)
{
	u8 buf;
	int rc;

	if (!chip->dt.auto_recharge_soc)
		return 0;

	if (recharge_soc < 0 || recharge_soc > FULL_CAPACITY)
		return 0;

	fg_encode(chip->sp, FG_SRAM_RECHARGE_SOC_THR, recharge_soc, &buf);
	rc = fg_sram_write(chip,
			chip->sp[FG_SRAM_RECHARGE_SOC_THR].addr_word,
			chip->sp[FG_SRAM_RECHARGE_SOC_THR].addr_byte, &buf,
			chip->sp[FG_SRAM_RECHARGE_SOC_THR].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing recharge_soc_thr, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int fg_adjust_recharge_soc(struct fg_chip *chip)
{
	union power_supply_propval prop = {0, };
	int rc, msoc, recharge_soc, new_recharge_soc = 0;
	bool recharge_soc_status;

	if (!chip->dt.auto_recharge_soc)
		return 0;

	rc = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_HEALTH,
		&prop);
	if (rc < 0) {
		pr_err("Error in getting battery health, rc=%d\n", rc);
		return rc;
	}
	chip->health = prop.intval;

	recharge_soc = chip->dt.recharge_soc_thr;
	recharge_soc_status = chip->recharge_soc_adjusted;
	/*
	 * If the input is present and charging had been terminated, adjust
	 * the recharge SOC threshold based on the monotonic SOC at which
	 * the charge termination had happened.
	 */
	if (is_input_present(chip)) {
		if (chip->charge_done) {
			if (!chip->recharge_soc_adjusted) {
				/* Get raw monotonic SOC for calculation */
				rc = fg_get_msoc(chip, &msoc);
				if (rc < 0) {
					pr_err("Error in getting msoc, rc=%d\n",
						rc);
					return rc;
				}

				/* Adjust the recharge_soc threshold */
				new_recharge_soc = msoc - (FULL_CAPACITY -
								recharge_soc);
				chip->recharge_soc_adjusted = true;
			} else {
				/* adjusted already, do nothing */
				return 0;
			}
		} else {
			if (!chip->recharge_soc_adjusted)
				return 0;

			if (chip->health != POWER_SUPPLY_HEALTH_GOOD)
				return 0;

			/* Restore the default value */
			new_recharge_soc = recharge_soc;
			chip->recharge_soc_adjusted = false;
		}
	} else {
		/* Restore the default value */
		new_recharge_soc = recharge_soc;
		chip->recharge_soc_adjusted = false;
	}

	rc = fg_set_recharge_soc(chip, new_recharge_soc);
	if (rc < 0) {
		chip->recharge_soc_adjusted = recharge_soc_status;
		pr_err("Couldn't set resume SOC for FG, rc=%d\n", rc);
		return rc;
	}

	fg_dbg(chip, FG_STATUS, "resume soc set to %d\n", new_recharge_soc);
	return 0;
}

static int fg_adjust_recharge_voltage(struct fg_chip *chip)
{
	int rc, recharge_volt_mv;

	if (chip->dt.auto_recharge_soc)
		return 0;

	fg_dbg(chip, FG_STATUS, "health: %d chg_status: %d chg_done: %d\n",
		chip->health, chip->charge_status, chip->charge_done);

	recharge_volt_mv = chip->dt.recharge_volt_thr_mv;

	/* Lower the recharge voltage in soft JEITA */
	if (chip->health == POWER_SUPPLY_HEALTH_WARM ||
			chip->health == POWER_SUPPLY_HEALTH_COOL)
		recharge_volt_mv -= 200;

	rc = fg_set_recharge_voltage(chip, recharge_volt_mv);
	if (rc < 0) {
		pr_err("Error in setting recharge_voltage, rc=%d\n",
			rc);
		return rc;
	}

	return 0;
}

static int fg_slope_limit_config(struct fg_chip *chip, int batt_temp)
{
	enum slope_limit_status status;
	int rc;
	u8 buf;

	if (!chip->slope_limit_en)
		return 0;

	if (chip->charge_status == POWER_SUPPLY_STATUS_CHARGING ||
		chip->charge_status == POWER_SUPPLY_STATUS_FULL || //ASUS_BSP
		chip->charge_status == POWER_SUPPLY_STATUS_QUICK_CHARGING || //ASUS_BSP
		chip->charge_status == POWER_SUPPLY_STATUS_NOT_QUICK_CHARGING) { //ASUS_BSP
		if (batt_temp < chip->dt.slope_limit_temp)
			status = LOW_TEMP_CHARGE;
		else
			status = HIGH_TEMP_CHARGE;
	} else {
		if (batt_temp < chip->dt.slope_limit_temp)
			status = LOW_TEMP_DISCHARGE;
		else
			status = HIGH_TEMP_DISCHARGE;
	}

	if (chip->slope_limit_sts == status)
		return 0;

	fg_encode(chip->sp, FG_SRAM_SLOPE_LIMIT,
		chip->dt.slope_limit_coeffs[status], &buf);
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_SLOPE_LIMIT].addr_word,
			chip->sp[FG_SRAM_SLOPE_LIMIT].addr_byte, &buf,
			chip->sp[FG_SRAM_SLOPE_LIMIT].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in configuring slope_limit coefficient, rc=%d\n",
			rc);
		return rc;
	}

	chip->slope_limit_sts = status;
	fg_dbg(chip, FG_STATUS, "Slope limit status: %d value: %x\n", status,
		buf);
	return 0;
}

static int __fg_esr_filter_config(struct fg_chip *chip,
				enum esr_filter_status esr_flt_sts)
{
	u8 esr_tight_flt, esr_broad_flt;
	int esr_tight_flt_upct, esr_broad_flt_upct;
	int rc;

	if (esr_flt_sts == chip->esr_flt_sts)
		return 0;

	if (esr_flt_sts == ROOM_TEMP) {
		esr_tight_flt_upct = chip->dt.esr_tight_flt_upct;
		esr_broad_flt_upct = chip->dt.esr_broad_flt_upct;
	} else if (esr_flt_sts == LOW_TEMP) {
		esr_tight_flt_upct = chip->dt.esr_tight_lt_flt_upct;
		esr_broad_flt_upct = chip->dt.esr_broad_lt_flt_upct;
	} else if (esr_flt_sts == RELAX_TEMP) {
		esr_tight_flt_upct = chip->dt.esr_tight_rt_flt_upct;
		esr_broad_flt_upct = chip->dt.esr_broad_rt_flt_upct;
	} else {
		pr_err("Unknown esr filter config\n");
		return 0;
	}

	fg_encode(chip->sp, FG_SRAM_ESR_TIGHT_FILTER, esr_tight_flt_upct,
		&esr_tight_flt);
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_ESR_TIGHT_FILTER].addr_word,
			chip->sp[FG_SRAM_ESR_TIGHT_FILTER].addr_byte,
			&esr_tight_flt,
			chip->sp[FG_SRAM_ESR_TIGHT_FILTER].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR LT tight filter, rc=%d\n", rc);
		return rc;
	}

	fg_encode(chip->sp, FG_SRAM_ESR_BROAD_FILTER, esr_broad_flt_upct,
		&esr_broad_flt);
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_ESR_BROAD_FILTER].addr_word,
			chip->sp[FG_SRAM_ESR_BROAD_FILTER].addr_byte,
			&esr_broad_flt,
			chip->sp[FG_SRAM_ESR_BROAD_FILTER].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR LT broad filter, rc=%d\n", rc);
		return rc;
	}

	chip->esr_flt_sts = esr_flt_sts;
	fg_dbg(chip, FG_STATUS, "applied ESR filter %d values\n", esr_flt_sts);
	return 0;
}

#define DT_IRQ_COUNT			3
#define DELTA_TEMP_IRQ_TIME_MS		300000
#define ESR_FILTER_ALARM_TIME_MS	900000
static int fg_esr_filter_config(struct fg_chip *chip, int batt_temp,
				bool override)
{
	enum esr_filter_status esr_flt_sts = ROOM_TEMP;
	bool qnovo_en, input_present, count_temp_irq = false;
	s64 time_ms;
	int rc;

	/*
	 * If the battery temperature is lower than -20 C, then skip modifying
	 * ESR filter.
	 */
	if (batt_temp < -210)
		return 0;

	qnovo_en = is_qnovo_en(chip);
	input_present = is_input_present(chip);

	/*
	 * If Qnovo is enabled, after hitting a lower battery temperature of
	 * say 6 C, count the delta battery temperature interrupts for a
	 * certain period of time when the battery temperature increases.
	 * Switch to relaxed filter coefficients once the temperature increase
	 * is qualified so that ESR accuracy can be improved.
	 */
	if (qnovo_en && !override) {
		if (input_present) {
			if (chip->esr_flt_sts == RELAX_TEMP) {
				/* do nothing */
				return 0;
			}

			count_temp_irq =  true;
			if (chip->delta_temp_irq_count) {
				/* Don't count when temperature is dropping. */
				if (batt_temp <= chip->last_batt_temp)
					count_temp_irq = false;
			} else {
				/*
				 * Starting point for counting. Check if the
				 * temperature is qualified.
				 */
				if (batt_temp > chip->dt.esr_flt_rt_switch_temp)
					count_temp_irq = false;
				else
					chip->last_delta_temp_time =
						ktime_get();
			}
		} else {
			chip->delta_temp_irq_count = 0;
			rc = alarm_try_to_cancel(&chip->esr_filter_alarm);
			if (rc < 0)
				pr_err("Couldn't cancel esr_filter_alarm\n");
		}
	}

	/*
	 * If battery temperature is lesser than 10 C (default), then apply the
	 * ESR low temperature tight and broad filter values to ESR room
	 * temperature tight and broad filters. If battery temperature is higher
	 * than 10 C, then apply back the room temperature ESR filter
	 * coefficients to ESR room temperature tight and broad filters.
	 */
	if (batt_temp > chip->dt.esr_flt_switch_temp)
		esr_flt_sts = ROOM_TEMP;
	else
		esr_flt_sts = LOW_TEMP;

	if (count_temp_irq) {
		time_ms = ktime_ms_delta(ktime_get(),
				chip->last_delta_temp_time);
		chip->delta_temp_irq_count++;
		fg_dbg(chip, FG_STATUS, "dt_irq_count: %d\n",
			chip->delta_temp_irq_count);

		if (chip->delta_temp_irq_count >= DT_IRQ_COUNT
			&& time_ms <= DELTA_TEMP_IRQ_TIME_MS) {
			fg_dbg(chip, FG_STATUS, "%d interrupts in %lld ms\n",
				chip->delta_temp_irq_count, time_ms);
			esr_flt_sts = RELAX_TEMP;
		}
	}

	rc = __fg_esr_filter_config(chip, esr_flt_sts);
	if (rc < 0)
		return rc;

	if (esr_flt_sts == RELAX_TEMP)
		alarm_start_relative(&chip->esr_filter_alarm,
			ms_to_ktime(ESR_FILTER_ALARM_TIME_MS));

	return 0;
}

#define FG_ESR_FILTER_RESTART_MS	60000
static void esr_filter_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
			struct fg_chip, esr_filter_work);
	int rc, batt_temp;

	rc = fg_get_battery_temp(chip, &batt_temp);
	if (rc < 0) {
		pr_err("Error in getting batt_temp\n");
		alarm_start_relative(&chip->esr_filter_alarm,
			ms_to_ktime(FG_ESR_FILTER_RESTART_MS));
	}

	rc = fg_esr_filter_config(chip, batt_temp, true);
	if (rc < 0) {
		pr_err("Error in configuring ESR filter rc:%d\n", rc);
		alarm_start_relative(&chip->esr_filter_alarm,
			ms_to_ktime(FG_ESR_FILTER_RESTART_MS));
	}

	chip->delta_temp_irq_count = 0;
	pm_relax(chip->dev);
}

static enum alarmtimer_restart fg_esr_filter_alarm_cb(struct alarm *alarm,
							ktime_t now)
{
	struct fg_chip *chip = container_of(alarm, struct fg_chip,
					esr_filter_alarm);

	fg_dbg(chip, FG_STATUS, "ESR filter alarm triggered %lld\n",
		ktime_to_ms(now));
	/*
	 * We cannot vote for awake votable here as that takes a mutex lock
	 * and this is executed in an atomic context.
	 */
	pm_stay_awake(chip->dev);
	schedule_work(&chip->esr_filter_work);

	return ALARMTIMER_NORESTART;
}

static int fg_esr_fcc_config(struct fg_chip *chip)
{
	union power_supply_propval prop = {0, };
	int rc;
	bool parallel_en = false, qnovo_en;

	if (is_parallel_charger_available(chip)) {
		rc = power_supply_get_property(chip->parallel_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &prop);
		if (rc < 0) {
			pr_err("Error in reading charging_enabled from parallel_psy, rc=%d\n",
				rc);
			return rc;
		}
		parallel_en = prop.intval;
	}

	qnovo_en = is_qnovo_en(chip);

	fg_dbg(chip, FG_POWER_SUPPLY, "chg_sts: %d par_en: %d qnov_en: %d esr_fcc_ctrl_en: %d\n",
		chip->charge_status, parallel_en, qnovo_en,
		chip->esr_fcc_ctrl_en);

	if ((chip->charge_status == POWER_SUPPLY_STATUS_CHARGING || chip->charge_status == POWER_SUPPLY_STATUS_QUICK_CHARGING //ASUS_BSP
		|| chip->charge_status == POWER_SUPPLY_STATUS_NOT_QUICK_CHARGING) && (parallel_en||qnovo_en)) { //ASUS_BSP
		if (chip->esr_fcc_ctrl_en)
			return 0;

		/*
		 * When parallel charging or Qnovo is enabled, configure ESR
		 * FCC to 300mA to trigger an ESR pulse. Without this, FG can
		 * request the main charger to increase FCC when it is supposed
		 * to decrease it.
		 */
		rc = fg_masked_write(chip, BATT_INFO_ESR_FAST_CRG_CFG(chip),
				ESR_FAST_CRG_IVAL_MASK |
				ESR_FAST_CRG_CTL_EN_BIT,
				ESR_FCC_300MA | ESR_FAST_CRG_CTL_EN_BIT);
		if (rc < 0) {
			pr_err("Error in writing to %04x, rc=%d\n",
				BATT_INFO_ESR_FAST_CRG_CFG(chip), rc);
			return rc;
		}

		chip->esr_fcc_ctrl_en = true;
	} else {
		if (!chip->esr_fcc_ctrl_en)
			return 0;

		/*
		 * If we're here, then it means either the device is not in
		 * charging state or parallel charging / Qnovo is disabled.
		 * Disable ESR fast charge current control in SW.
		 */
		rc = fg_masked_write(chip, BATT_INFO_ESR_FAST_CRG_CFG(chip),
				ESR_FAST_CRG_CTL_EN_BIT, 0);
		if (rc < 0) {
			pr_err("Error in writing to %04x, rc=%d\n",
				BATT_INFO_ESR_FAST_CRG_CFG(chip), rc);
			return rc;
		}

		chip->esr_fcc_ctrl_en = false;
	}

	fg_dbg(chip, FG_STATUS, "esr_fcc_ctrl_en set to %d\n",
		chip->esr_fcc_ctrl_en);
	return 0;
}

static int fg_esr_timer_config(struct fg_chip *chip, bool sleep)
{
	int rc, cycles_init, cycles_max;
	bool end_of_charge = false;

	end_of_charge = is_input_present(chip) && chip->charge_done;
	fg_dbg(chip, FG_STATUS, "sleep: %d eoc: %d\n", sleep, end_of_charge);

	/* ESR discharging timer configuration */
	cycles_init = sleep ? chip->dt.esr_timer_asleep[TIMER_RETRY] :
			chip->dt.esr_timer_awake[TIMER_RETRY];
	if (end_of_charge)
		cycles_init = 0;

	cycles_max = sleep ? chip->dt.esr_timer_asleep[TIMER_MAX] :
			chip->dt.esr_timer_awake[TIMER_MAX];

	rc = fg_set_esr_timer(chip, cycles_init, cycles_max, false,
		sleep ? FG_IMA_NO_WLOCK : FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in setting ESR timer, rc=%d\n", rc);
		return rc;
	}

	/* ESR charging timer configuration */
	cycles_init = cycles_max = -EINVAL;
	if (end_of_charge || sleep) {
		cycles_init = chip->dt.esr_timer_charging[TIMER_RETRY];
		cycles_max = chip->dt.esr_timer_charging[TIMER_MAX];
	} else if (is_input_present(chip)) {
		cycles_init = chip->esr_timer_charging_default[TIMER_RETRY];
		cycles_max = chip->esr_timer_charging_default[TIMER_MAX];
	}

	rc = fg_set_esr_timer(chip, cycles_init, cycles_max, true,
		sleep ? FG_IMA_NO_WLOCK : FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in setting ESR timer, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int fg_update_maint_soc(struct fg_chip *chip); //ASUS_BSP

static void fg_ttf_update(struct fg_chip *chip)
{
	int rc;
	int delay_ms;
	union power_supply_propval prop = {0, };
	int online = 0;

	if (usb_psy_initialized(chip)) {
		rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_ONLINE, &prop);
		if (rc < 0) {
			pr_err("Couldn't read usb ONLINE prop rc=%d\n", rc);
			return;
		}

		online = online || prop.intval;
	}

	if (pc_port_psy_initialized(chip)) {
		rc = power_supply_get_property(chip->pc_port_psy,
			POWER_SUPPLY_PROP_ONLINE, &prop);
		if (rc < 0) {
			pr_err("Couldn't read pc_port ONLINE prop rc=%d\n", rc);
			return;
		}

		online = online || prop.intval;
	}

	if (dc_psy_initialized(chip)) {
		rc = power_supply_get_property(chip->dc_psy,
			POWER_SUPPLY_PROP_ONLINE, &prop);
		if (rc < 0) {
			pr_err("Couldn't read dc ONLINE prop rc=%d\n", rc);
			return;
		}

		online = online || prop.intval;
	}


	if (chip->online_status == online)
		return;

	chip->online_status = online;
	if (online)
		/* wait 35 seconds for the input to settle */
		delay_ms = 35000;
	else
		/* wait 5 seconds for current to settle during discharge */
		delay_ms = 5000;

	vote(chip->awake_votable, TTF_PRIMING, true, 0);
	cancel_delayed_work_sync(&chip->ttf_work);
	mutex_lock(&chip->ttf.lock);
	fg_circ_buf_clr(&chip->ttf.ibatt);
	fg_circ_buf_clr(&chip->ttf.vbatt);
	chip->ttf.last_ttf = 0;
	chip->ttf.last_ms = 0;
	mutex_unlock(&chip->ttf.lock);
	queue_delayed_work(system_power_efficient_wq,
		&chip->ttf_work, msecs_to_jiffies(delay_ms));
}

static void restore_cycle_counter(struct fg_chip *chip)
{
	int rc = 0, i;
	u8 data[2];

	if (!chip->cyc_ctr.en)
		return;

	mutex_lock(&chip->cyc_ctr.lock);
	for (i = 0; i < BUCKET_COUNT; i++) {
		rc = fg_sram_read(chip, CYCLE_COUNT_WORD + (i / 2),
				CYCLE_COUNT_OFFSET + (i % 2) * 2, data, 2,
				FG_IMA_DEFAULT);
		if (rc < 0)
			pr_err("failed to read bucket %d rc=%d\n", i, rc);
		else
			chip->cyc_ctr.count[i] = data[0] | data[1] << 8;
	}
	mutex_unlock(&chip->cyc_ctr.lock);
}

static void clear_cycle_counter(struct fg_chip *chip)
{
	int rc = 0, i;

	if (!chip->cyc_ctr.en)
		return;

	mutex_lock(&chip->cyc_ctr.lock);
	memset(chip->cyc_ctr.count, 0, sizeof(chip->cyc_ctr.count));
	for (i = 0; i < BUCKET_COUNT; i++) {
		chip->cyc_ctr.started[i] = false;
		chip->cyc_ctr.last_soc[i] = 0;
	}
	rc = fg_sram_write(chip, CYCLE_COUNT_WORD, CYCLE_COUNT_OFFSET,
			(u8 *)&chip->cyc_ctr.count,
			sizeof(chip->cyc_ctr.count) / sizeof(u8 *),
			FG_IMA_DEFAULT);
	if (rc < 0)
		pr_err("failed to clear cycle counter rc=%d\n", rc);

	mutex_unlock(&chip->cyc_ctr.lock);
}

static int fg_inc_store_cycle_ctr(struct fg_chip *chip, int bucket)
{
	int rc = 0;
	u16 cyc_count;
	u8 data[2];

	if (bucket < 0 || (bucket > BUCKET_COUNT - 1))
		return 0;

	cyc_count = chip->cyc_ctr.count[bucket];
	cyc_count++;
	data[0] = cyc_count & 0xFF;
	data[1] = cyc_count >> 8;

	rc = fg_sram_write(chip, CYCLE_COUNT_WORD + (bucket / 2),
			CYCLE_COUNT_OFFSET + (bucket % 2) * 2, data, 2,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("failed to write BATT_CYCLE[%d] rc=%d\n",
			bucket, rc);
		return rc;
	}

	chip->cyc_ctr.count[bucket] = cyc_count;
	fg_dbg(chip, FG_STATUS, "Stored count %d in bucket %d\n", cyc_count,
		bucket);

	return rc;
}

static void fg_cycle_counter_update(struct fg_chip *chip)
{
	int rc = 0, bucket, i, batt_soc;

	if (!chip->cyc_ctr.en)
		return;

	mutex_lock(&chip->cyc_ctr.lock);
	rc = fg_get_sram_prop(chip, FG_SRAM_BATT_SOC, &batt_soc);
	if (rc < 0) {
		pr_err("Failed to read battery soc rc: %d\n", rc);
		goto out;
	}

	/* We need only the most significant byte here */
	batt_soc = (u32)batt_soc >> 24;

	/* Find out which bucket the SOC falls in */
	bucket = batt_soc / BUCKET_SOC_PCT;

	if (chip->charge_status == POWER_SUPPLY_STATUS_CHARGING || chip->charge_status == POWER_SUPPLY_STATUS_QUICK_CHARGING 
		|| chip->charge_status == POWER_SUPPLY_STATUS_NOT_QUICK_CHARGING) { //ASUS_BSP
		if (!chip->cyc_ctr.started[bucket]) {
			chip->cyc_ctr.started[bucket] = true;
			chip->cyc_ctr.last_soc[bucket] = batt_soc;
		}
	} else if (chip->charge_done || !is_input_present(chip)) {
		for (i = 0; i < BUCKET_COUNT; i++) {
			if (chip->cyc_ctr.started[i] &&
				batt_soc > chip->cyc_ctr.last_soc[i] + 2) {
				//ASUS_BSP battery safety upgrade +++
				if(i == (BUCKET_COUNT-1)){
					g_cycle_count_data.cycle_count++; //95%~100% cycle count
					//BAT_DBG("%s cycle_count = %d\n", __FUNCTION__, g_cycle_count_data.cycle_count);
				}
				//ASUS_BSP battery safety upgrade ---
				rc = fg_inc_store_cycle_ctr(chip, i);
				if (rc < 0)
					pr_err("Error in storing cycle_ctr rc: %d\n",
						rc);
				chip->cyc_ctr.last_soc[i] = 0;
				chip->cyc_ctr.started[i] = false;
			}
		}
	}

	fg_dbg(chip, FG_STATUS, "batt_soc: %d bucket: %d chg_status: %d\n",
		batt_soc, bucket, chip->charge_status);
out:
	mutex_unlock(&chip->cyc_ctr.lock);
}

static int fg_get_cycle_count(struct fg_chip *chip)
{
	int count;

	if (!chip->cyc_ctr.en)
		return 0;

	if ((chip->cyc_ctr.id <= 0) || (chip->cyc_ctr.id > BUCKET_COUNT))
		return -EINVAL;

	mutex_lock(&chip->cyc_ctr.lock);
	count = chip->cyc_ctr.count[chip->cyc_ctr.id - 1];
	mutex_unlock(&chip->cyc_ctr.lock);
	return count;
}

static void status_change_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
			struct fg_chip, status_change_work);
	union power_supply_propval prop = {0, };
	int rc, batt_temp;

	if (!batt_psy_initialized(chip)) {
		fg_dbg(chip, FG_STATUS, "Charger not available?!\n");
		goto out;
	}

//ASUS_BSP +++
	mutex_lock(&chip->charge_status_lock);
	gauge_get_prop = 1;
	mutex_unlock(&chip->charge_status_lock);
//ASUS_BSP ---

	rc = power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_STATUS,
			&prop);
	if (rc < 0) {
		pr_err("Error in getting charging status, rc=%d\n", rc);
		goto out;
	}

	chip->charge_status = prop.intval;
	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CHARGE_TYPE, &prop);
	if (rc < 0) {
		pr_err("Error in getting charge type, rc=%d\n", rc);
		goto out;
	}

	chip->charge_type = prop.intval;
	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CHARGE_DONE, &prop);
	if (rc < 0) {
		pr_err("Error in getting charge_done, rc=%d\n", rc);
		goto out;
	}

	chip->charge_done = prop.intval;
	fg_cycle_counter_update(chip);  // bucket_count parse: soc percent is divided to bucket_count, such as bucket_count = 8, so bucket[0]-[7]: 0-12.5  12.5-25.... 87.5-100;   every pick = [100/bucket_count]
	fg_cap_learning_update(chip);

	rc = fg_charge_full_update(chip);
	if (rc < 0)
		pr_err("Error in charge_full_update, rc=%d\n", rc);

	rc = fg_adjust_recharge_soc(chip);
	if (rc < 0)
		pr_err("Error in adjusting recharge_soc, rc=%d\n", rc);

	rc = fg_adjust_recharge_voltage(chip);
	if (rc < 0)
		pr_err("Error in adjusting recharge_voltage, rc=%d\n", rc);

	rc = fg_adjust_ki_coeff_dischg(chip);
	if (rc < 0)
		pr_err("Error in adjusting ki_coeff_dischg, rc=%d\n", rc);

	rc = fg_esr_fcc_config(chip);
	if (rc < 0)
		pr_err("Error in adjusting FCC for ESR, rc=%d\n", rc);

	rc = fg_get_battery_temp(chip, &batt_temp);
	if (!rc) {
		rc = fg_slope_limit_config(chip, batt_temp);
		if (rc < 0)
			pr_err("Error in configuring slope limiter rc:%d\n",
				rc);

		rc = fg_adjust_ki_coeff_full_soc(chip, batt_temp);
		if (rc < 0)
			pr_err("Error in configuring ki_coeff_full_soc rc:%d\n",
				rc);
	}

	fg_ttf_update(chip);
	chip->prev_charge_status = chip->charge_status;
out:
	fg_dbg(chip, FG_POWER_SUPPLY, "charge_status:%d charge_type:%d charge_done:%d\n",
		chip->charge_status, chip->charge_type, chip->charge_done);
	pm_relax(chip->dev);
}

static int fg_bp_params_config(struct fg_chip *chip)
{
	int rc = 0;
	u8 buf;

	/* This SRAM register is only present in v2.0 and above */
	if (!(chip->wa_flags & PMI8998_V1_REV_WA) &&
					chip->bp.float_volt_uv > 0) {
		fg_encode(chip->sp, FG_SRAM_FLOAT_VOLT,
			chip->bp.float_volt_uv / 1000, &buf);
		rc = fg_sram_write(chip, chip->sp[FG_SRAM_FLOAT_VOLT].addr_word,
			chip->sp[FG_SRAM_FLOAT_VOLT].addr_byte, &buf,
			chip->sp[FG_SRAM_FLOAT_VOLT].len, FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing float_volt, rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->bp.vbatt_full_mv > 0) {
		rc = fg_set_constant_chg_voltage(chip,
				chip->bp.vbatt_full_mv * 1000);
		if (rc < 0)
			return rc;
	}

	return rc;
}

#define PROFILE_LOAD_BIT	BIT(0)
#define BOOTLOADER_LOAD_BIT	BIT(1)
#define BOOTLOADER_RESTART_BIT	BIT(2)
#define HLOS_RESTART_BIT	BIT(3)
static bool is_profile_load_required(struct fg_chip *chip)
{
	u8 buf[PROFILE_COMP_LEN], val;
	bool profiles_same = false;
	int rc;

	rc = fg_sram_read(chip, PROFILE_INTEGRITY_WORD,
			PROFILE_INTEGRITY_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("failed to read profile integrity rc=%d\n", rc);
		return false;
	}

	/* Check if integrity bit is set */
	if (val & PROFILE_LOAD_BIT) {
		fg_dbg(chip, FG_STATUS, "Battery profile integrity bit is set\n");

		/* Whitelist the values */
		val &= ~PROFILE_LOAD_BIT;
		if (val != HLOS_RESTART_BIT && val != BOOTLOADER_LOAD_BIT &&
			val != (BOOTLOADER_LOAD_BIT | BOOTLOADER_RESTART_BIT)) {
			val |= PROFILE_LOAD_BIT;
			pr_warn("Garbage value in profile integrity word: 0x%x\n",
				val);
			return true;
		}

		rc = fg_sram_read(chip, PROFILE_LOAD_WORD, PROFILE_LOAD_OFFSET,
				buf, PROFILE_COMP_LEN, FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in reading battery profile, rc:%d\n", rc);
			return false;
		}
		profiles_same = memcmp(chip->batt_profile, buf,
					PROFILE_COMP_LEN) == 0;
		if (profiles_same) {
			fg_dbg(chip, FG_STATUS, "Battery profile is same, not loading it\n");
			return false;
		}

		if (!chip->dt.force_load_profile) {
			pr_warn("Profiles doesn't match, skipping loading it since force_load_profile is disabled\n");
			if (fg_profile_dump) {
				pr_info("FG: loaded profile:\n");
				dump_sram(buf, PROFILE_LOAD_WORD,
					PROFILE_COMP_LEN);
				pr_info("FG: available profile:\n");
				dump_sram(chip->batt_profile, PROFILE_LOAD_WORD,
					PROFILE_LEN);
			}
			return false;
		}

		fg_dbg(chip, FG_STATUS, "Profiles are different, loading the correct one\n");
	} else {
		fg_dbg(chip, FG_STATUS, "Profile integrity bit is not set\n");
		if (fg_profile_dump) {
			pr_info("FG: profile to be loaded:\n");
			dump_sram(chip->batt_profile, PROFILE_LOAD_WORD,
				PROFILE_LEN);
		}
	}
	return true;
}

static void fg_update_batt_profile(struct fg_chip *chip)
{
	int rc, offset;
	u8 val;

	rc = fg_sram_read(chip, PROFILE_INTEGRITY_WORD,
			SW_CONFIG_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading SW_CONFIG_OFFSET, rc=%d\n", rc);
		return;
	}

	/*
	 * If the RCONN had not been updated, no need to update battery
	 * profile. Else, update the battery profile so that the profile
	 * modified by bootloader or HLOS matches with the profile read
	 * from device tree.
	 */

	if (!(val & RCONN_CONFIG_BIT))
		return;

	rc = fg_sram_read(chip, ESR_RSLOW_CHG_WORD,
			ESR_RSLOW_CHG_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading ESR_RSLOW_CHG_OFFSET, rc=%d\n", rc);
		return;
	}
	offset = (ESR_RSLOW_CHG_WORD - PROFILE_LOAD_WORD) * 4
			+ ESR_RSLOW_CHG_OFFSET;
	chip->batt_profile[offset] = val;

	rc = fg_sram_read(chip, ESR_RSLOW_DISCHG_WORD,
			ESR_RSLOW_DISCHG_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading ESR_RSLOW_DISCHG_OFFSET, rc=%d\n", rc);
		return;
	}
	offset = (ESR_RSLOW_DISCHG_WORD - PROFILE_LOAD_WORD) * 4
			+ ESR_RSLOW_DISCHG_OFFSET;
	chip->batt_profile[offset] = val;
}

static void clear_battery_profile(struct fg_chip *chip)
{
	u8 val = 0;
	int rc;

	rc = fg_sram_write(chip, PROFILE_INTEGRITY_WORD,
			PROFILE_INTEGRITY_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0)
		pr_err("failed to write profile integrity rc=%d\n", rc);
}

#define SOC_READY_WAIT_MS		2000
static int __fg_restart(struct fg_chip *chip)
{
	int rc, msoc;
	bool tried_again = false;

	rc = fg_get_prop_capacity(chip, &msoc);
	if (rc < 0) {
		pr_err("Error in getting capacity, rc=%d\n", rc);
		return rc;
	}

	chip->last_soc = msoc;
	chip->fg_restarting = true;
	reinit_completion(&chip->soc_ready);
	rc = fg_masked_write(chip, BATT_SOC_RESTART(chip), RESTART_GO_BIT,
			RESTART_GO_BIT);
	if (rc < 0) {
		pr_err("Error in writing to %04x, rc=%d\n",
			BATT_SOC_RESTART(chip), rc);
		goto out;
	}

wait:
	rc = wait_for_completion_interruptible_timeout(&chip->soc_ready,
		msecs_to_jiffies(SOC_READY_WAIT_MS));

	/* If we were interrupted wait again one more time. */
	if (rc == -ERESTARTSYS && !tried_again) {
		tried_again = true;
		goto wait;
	} else if (rc <= 0) {
		pr_err("wait for soc_ready timed out rc=%d\n", rc);
	}

	rc = fg_masked_write(chip, BATT_SOC_RESTART(chip), RESTART_GO_BIT, 0);
	if (rc < 0) {
		pr_err("Error in writing to %04x, rc=%d\n",
			BATT_SOC_RESTART(chip), rc);
		goto out;
	}
out:
	chip->fg_restarting = false;
	return rc;
}

extern int g_fv_setting; //ASUS_BSP battery safety upgrade
//ASUS_BSP battery safety upgrade +++
static void set_full_charging_voltage(void)
{
	if(0 == g_cycle_count_data.reload_condition){
		g_fv_setting = 0x74; //4.35V
		fg_set_constant_chg_voltage(g_fgChip, 4340 * 1000);
	}else if(1 == g_cycle_count_data.reload_condition){
		g_fv_setting = 0x6D; //4.30V
		fg_set_constant_chg_voltage(g_fgChip, 4290 * 1000);
	}else if(2 == g_cycle_count_data.reload_condition){
		g_fv_setting = 0x66; //4.25V
		fg_set_constant_chg_voltage(g_fgChip, 4240 * 1000);
	}
	BAT_DBG("%s g_fv_setting=%x \n",__FUNCTION__,g_fv_setting);
}
//ASUS_BSP battery safety upgrade ---

static void profile_load_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work,
				struct fg_chip,
				profile_load_work.work);
	u8 buf[2], val;
	int rc;

	vote(chip->awake_votable, PROFILE_LOAD, true, 0);

	rc = fg_get_batt_id(chip);
	if (rc < 0) {
		pr_err("Error in getting battery id, rc:%d\n", rc);
		goto out;
	}

	rc = fg_get_batt_profile(chip);
	if (rc < 0) {
		pr_warn("profile for batt_id=%dKOhms not found..using OTP, rc:%d\n",
			chip->batt_id_ohms / 1000, rc);
		goto out;
	}

	if (!chip->profile_available)
		goto out;

	fg_update_batt_profile(chip);

	if (!is_profile_load_required(chip))
		goto done;

	clear_cycle_counter(chip);
	mutex_lock(&chip->cl.lock);
	chip->cl.learned_cc_uah = 0;
	chip->cl.active = false;
	mutex_unlock(&chip->cl.lock);

	fg_dbg(chip, FG_STATUS, "profile loading started\n");
	rc = fg_masked_write(chip, BATT_SOC_RESTART(chip), RESTART_GO_BIT, 0);
	if (rc < 0) {
		pr_err("Error in writing to %04x, rc=%d\n",
			BATT_SOC_RESTART(chip), rc);
		goto out;
	}

	/* load battery profile */
	rc = fg_sram_write(chip, PROFILE_LOAD_WORD, PROFILE_LOAD_OFFSET,
			chip->batt_profile, PROFILE_LEN, FG_IMA_ATOMIC);
	if (rc < 0) {
		pr_err("Error in writing battery profile, rc:%d\n", rc);
		goto out;
	}

	rc = __fg_restart(chip);
	if (rc < 0) {
		pr_err("Error in restarting FG, rc=%d\n", rc);
		goto out;
	}

	fg_dbg(chip, FG_STATUS, "SOC is ready\n");

	/* Set the profile integrity bit */
	val = HLOS_RESTART_BIT | PROFILE_LOAD_BIT;
	rc = fg_sram_write(chip, PROFILE_INTEGRITY_WORD,
			PROFILE_INTEGRITY_OFFSET, &val, 1, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("failed to write profile integrity rc=%d\n", rc);
		goto out;
	}

done:
	rc = fg_bp_params_config(chip);
	if (rc < 0)
		pr_err("Error in configuring battery profile params, rc:%d\n",
			rc);

	rc = fg_sram_read(chip, NOM_CAP_WORD, NOM_CAP_OFFSET, buf, 2,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading %04x[%d] rc=%d\n", NOM_CAP_WORD,
			NOM_CAP_OFFSET, rc);
	} else {
		chip->cl.nom_cap_uah = (int)(buf[0] | buf[1] << 8) * 1000;
		rc = fg_load_learned_cap_from_sram(chip);
		if (rc < 0)
			pr_err("Error in loading capacity learning data, rc:%d\n",
				rc);
	}

	rc = fg_rconn_config(chip);
	if (rc < 0)
		pr_err("Error in configuring Rconn, rc=%d\n", rc);

	batt_psy_initialized(chip);
	fg_notify_charger(chip);
	chip->profile_loaded = true;
	fg_dbg(chip, FG_STATUS, "profile loaded successfully");
out:
	chip->soc_reporting_ready = true;
	vote(chip->awake_votable, PROFILE_LOAD, false, 0);
}

static void sram_dump_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work, struct fg_chip,
					    sram_dump_work.work);
	u8 buf[FG_SRAM_LEN];
	int rc;
	s64 timestamp_ms, quotient;
	s32 remainder;

	rc = fg_sram_read(chip, 0, 0, buf, FG_SRAM_LEN, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in reading FG SRAM, rc:%d\n", rc);
		goto resched;
	}

	timestamp_ms = ktime_to_ms(ktime_get_boottime());
	quotient = div_s64_rem(timestamp_ms, 1000, &remainder);
	fg_dbg(chip, FG_STATUS, "SRAM Dump Started at %lld.%d\n",
		quotient, remainder);
	dump_sram(buf, 0, FG_SRAM_LEN);
	timestamp_ms = ktime_to_ms(ktime_get_boottime());
	quotient = div_s64_rem(timestamp_ms, 1000, &remainder);
	fg_dbg(chip, FG_STATUS, "SRAM Dump done at %lld.%d\n",
		quotient, remainder);
resched:
	queue_delayed_work(system_power_efficient_wq,
		&chip->sram_dump_work,
			msecs_to_jiffies(fg_sram_dump_period_ms));
}

static int fg_sram_dump_sysfs(const char *val, const struct kernel_param *kp)
{
	int rc;
	struct power_supply *bms_psy;
	struct fg_chip *chip;
	bool old_val = fg_sram_dump;

	rc = param_set_bool(val, kp);
	if (rc) {
		pr_err("Unable to set fg_sram_dump: %d\n", rc);
		return rc;
	}

	if (fg_sram_dump == old_val)
		return 0;

	bms_psy = power_supply_get_by_name("bms");
	if (!bms_psy) {
		pr_err("bms psy not found\n");
		return -ENODEV;
	}

	chip = power_supply_get_drvdata(bms_psy);
	if (fg_sram_dump)
		queue_delayed_work(system_power_efficient_wq,
			&chip->sram_dump_work,
				msecs_to_jiffies(fg_sram_dump_period_ms));
	else
		cancel_delayed_work_sync(&chip->sram_dump_work);

	return 0;
}

static struct kernel_param_ops fg_sram_dump_ops = {
	.set = fg_sram_dump_sysfs,
	.get = param_get_bool,
};

module_param_cb(sram_dump_en, &fg_sram_dump_ops, &fg_sram_dump, 0644);

static int fg_restart_sysfs(const char *val, const struct kernel_param *kp)
{
	int rc;
	struct power_supply *bms_psy;
	struct fg_chip *chip;

	rc = param_set_int(val, kp);
	if (rc) {
		pr_err("Unable to set fg_restart: %d\n", rc);
		return rc;
	}

	if (fg_restart != 1) {
		pr_err("Bad value %d\n", fg_restart);
		return -EINVAL;
	}

	bms_psy = power_supply_get_by_name("bms");
	if (!bms_psy) {
		pr_err("bms psy not found\n");
		return 0;
	}

	chip = power_supply_get_drvdata(bms_psy);
	rc = __fg_restart(chip);
	if (rc < 0) {
		pr_err("Error in restarting FG, rc=%d\n", rc);
		return rc;
	}

	pr_info("FG restart done\n");
	return rc;
}

static struct kernel_param_ops fg_restart_ops = {
	.set = fg_restart_sysfs,
	.get = param_get_int,
};

module_param_cb(restart, &fg_restart_ops, &fg_restart, 0644);

#define HOURS_TO_SECONDS	3600
#define OCV_SLOPE_UV		10869
#define MILLI_UNIT		1000
#define MICRO_UNIT		1000000
#define NANO_UNIT		1000000000
static int fg_get_time_to_full_locked(struct fg_chip *chip, int *val)
{
	int rc, ibatt_avg, vbatt_avg, rbatt, msoc, full_soc, act_cap_mah,
		i_cc2cv, soc_cc2cv, tau, divisor, iterm, ttf_mode,
		i, soc_per_step, msoc_this_step, msoc_next_step,
		ibatt_this_step, t_predicted_this_step, ttf_slope,
		t_predicted_cv, t_predicted = 0;
	s64 delta_ms;

	if (chip->bp.float_volt_uv <= 0) {
		pr_err("battery profile is not loaded\n");
		return -ENODATA;
	}

	if (!batt_psy_initialized(chip)) {
		fg_dbg(chip, FG_TTF, "charger is not available\n");
		return -ENODATA;
	}

	rc = fg_get_prop_capacity(chip, &msoc);
	if (rc < 0) {
		pr_err("failed to get msoc rc=%d\n", rc);
		return rc;
	}
	fg_dbg(chip, FG_TTF, "msoc=%d\n", msoc);

	/* the battery is considered full if the SOC is 100% */
	if (msoc >= 100) {
		*val = 0;
		return 0;
	}

	if (is_qnovo_en(chip))
		ttf_mode = TTF_MODE_QNOVO;
	else
		ttf_mode = TTF_MODE_NORMAL;

	/* when switching TTF algorithms the TTF needs to be reset */
	if (chip->ttf.mode != ttf_mode) {
		fg_circ_buf_clr(&chip->ttf.ibatt);
		fg_circ_buf_clr(&chip->ttf.vbatt);
		chip->ttf.last_ttf = 0;
		chip->ttf.last_ms = 0;
		chip->ttf.mode = ttf_mode;
	}

	/* at least 10 samples are required to produce a stable IBATT */
	if (chip->ttf.ibatt.size < 10) {
		*val = -1;
		return 0;
	}

	rc = fg_circ_buf_median(&chip->ttf.ibatt, &ibatt_avg);
	if (rc < 0) {
		pr_err("failed to get IBATT AVG rc=%d\n", rc);
		return rc;
	}

	rc = fg_circ_buf_median(&chip->ttf.vbatt, &vbatt_avg);
	if (rc < 0) {
		pr_err("failed to get VBATT AVG rc=%d\n", rc);
		return rc;
	}

	ibatt_avg = -ibatt_avg / MILLI_UNIT;
	vbatt_avg /= MILLI_UNIT;

	/* clamp ibatt_avg to iterm */
	if (ibatt_avg < abs(chip->dt.sys_term_curr_ma))
		ibatt_avg = abs(chip->dt.sys_term_curr_ma);

	fg_dbg(chip, FG_TTF, "ibatt_avg=%d\n", ibatt_avg);
	fg_dbg(chip, FG_TTF, "vbatt_avg=%d\n", vbatt_avg);

	rc = fg_get_battery_resistance(chip, &rbatt);
	if (rc < 0) {
		pr_err("failed to get battery resistance rc=%d\n", rc);
		return rc;
	}

	rbatt /= MILLI_UNIT;
	fg_dbg(chip, FG_TTF, "rbatt=%d\n", rbatt);

	rc = fg_get_sram_prop(chip, FG_SRAM_ACT_BATT_CAP, &act_cap_mah);
	if (rc < 0) {
		pr_err("failed to get ACT_BATT_CAP rc=%d\n", rc);
		return rc;
	}

	rc = fg_get_sram_prop(chip, FG_SRAM_FULL_SOC, &full_soc);
	if (rc < 0) {
		pr_err("failed to get full soc rc=%d\n", rc);
		return rc;
	}
	full_soc = DIV_ROUND_CLOSEST(((u16)full_soc >> 8) * FULL_CAPACITY,
								FULL_SOC_RAW);
	act_cap_mah = full_soc * act_cap_mah / 100;
	fg_dbg(chip, FG_TTF, "act_cap_mah=%d\n", act_cap_mah);

	/* estimated battery current at the CC to CV transition */
	switch (chip->ttf.mode) {
	case TTF_MODE_NORMAL:
		i_cc2cv = ibatt_avg * vbatt_avg /
			max(MILLI_UNIT, chip->bp.float_volt_uv / MILLI_UNIT);
		break;
	case TTF_MODE_QNOVO:
		i_cc2cv = min(
			chip->ttf.cc_step.arr[MAX_CC_STEPS - 1] / MILLI_UNIT,
			ibatt_avg * vbatt_avg /
			max(MILLI_UNIT, chip->bp.float_volt_uv / MILLI_UNIT));
		break;
	default:
		pr_err("TTF mode %d is not supported\n", chip->ttf.mode);
		break;
	}
	fg_dbg(chip, FG_TTF, "i_cc2cv=%d\n", i_cc2cv);

	/* if we are already in CV state then we can skip estimating CC */
	if (chip->charge_type == POWER_SUPPLY_CHARGE_TYPE_TAPER)
		goto cv_estimate;

	/* estimated SOC at the CC to CV transition */
	soc_cc2cv = DIV_ROUND_CLOSEST(rbatt * i_cc2cv, OCV_SLOPE_UV);
	soc_cc2cv = 100 - soc_cc2cv;
	fg_dbg(chip, FG_TTF, "soc_cc2cv=%d\n", soc_cc2cv);

	switch (chip->ttf.mode) {
	case TTF_MODE_NORMAL:
		if (soc_cc2cv - msoc <= 0)
			goto cv_estimate;

		divisor = max(100, (ibatt_avg + i_cc2cv) / 2 * 100);
		t_predicted = div_s64((s64)act_cap_mah * (soc_cc2cv - msoc) *
						HOURS_TO_SECONDS, divisor);
		break;
	case TTF_MODE_QNOVO:
		soc_per_step = 100 / MAX_CC_STEPS;
		for (i = msoc / soc_per_step; i < MAX_CC_STEPS - 1; ++i) {
			msoc_next_step = (i + 1) * soc_per_step;
			if (i == msoc / soc_per_step)
				msoc_this_step = msoc;
			else
				msoc_this_step = i * soc_per_step;

			/* scale ibatt by 85% to account for discharge pulses */
			ibatt_this_step = min(
					chip->ttf.cc_step.arr[i] / MILLI_UNIT,
					ibatt_avg) * 85 / 100;
			divisor = max(100, ibatt_this_step * 100);
			t_predicted_this_step = div_s64((s64)act_cap_mah *
					(msoc_next_step - msoc_this_step) *
					HOURS_TO_SECONDS, divisor);
			t_predicted += t_predicted_this_step;
			fg_dbg(chip, FG_TTF, "[%d, %d] ma=%d t=%d\n",
				msoc_this_step, msoc_next_step,
				ibatt_this_step, t_predicted_this_step);
		}
		break;
	default:
		pr_err("TTF mode %d is not supported\n", chip->ttf.mode);
		break;
	}

cv_estimate:
	fg_dbg(chip, FG_TTF, "t_predicted_cc=%d\n", t_predicted);

	iterm = max(100, abs(chip->dt.sys_term_curr_ma) + 200);
	fg_dbg(chip, FG_TTF, "iterm=%d\n", iterm);

	if (chip->charge_type == POWER_SUPPLY_CHARGE_TYPE_TAPER)
		tau = max(MILLI_UNIT, ibatt_avg * MILLI_UNIT / iterm);
	else
		tau = max(MILLI_UNIT, i_cc2cv * MILLI_UNIT / iterm);

	rc = fg_lerp(fg_ln_table, ARRAY_SIZE(fg_ln_table), tau, &tau);
	if (rc < 0) {
		pr_err("failed to interpolate tau rc=%d\n", rc);
		return rc;
	}

	/* tau is scaled linearly from 95% to 100% SOC */
	if (msoc >= 95)
		tau = tau * 2 * (100 - msoc) / 10;

	fg_dbg(chip, FG_TTF, "tau=%d\n", tau);
	t_predicted_cv = div_s64((s64)act_cap_mah * rbatt * tau *
						HOURS_TO_SECONDS, NANO_UNIT);
	fg_dbg(chip, FG_TTF, "t_predicted_cv=%d\n", t_predicted_cv);
	t_predicted += t_predicted_cv;

	fg_dbg(chip, FG_TTF, "t_predicted_prefilter=%d\n", t_predicted);
	if (chip->ttf.last_ms != 0) {
		delta_ms = ktime_ms_delta(ktime_get_boottime(),
					  ms_to_ktime(chip->ttf.last_ms));
		if (delta_ms > 10000) {
			ttf_slope = div64_s64(
				(s64)(t_predicted - chip->ttf.last_ttf) *
				MICRO_UNIT, delta_ms);
			if (ttf_slope > -100)
				ttf_slope = -100;
			else if (ttf_slope < -2000)
				ttf_slope = -2000;

			t_predicted = div_s64(
				(s64)ttf_slope * delta_ms, MICRO_UNIT) +
				chip->ttf.last_ttf;
			fg_dbg(chip, FG_TTF, "ttf_slope=%d\n", ttf_slope);
		} else {
			t_predicted = chip->ttf.last_ttf;
		}
	}

	/* clamp the ttf to 0 */
	if (t_predicted < 0)
		t_predicted = 0;

	fg_dbg(chip, FG_TTF, "t_predicted_postfilter=%d\n", t_predicted);
	*val = t_predicted;
	return 0;
}

static int fg_get_time_to_full(struct fg_chip *chip, int *val)
{
	int rc;

	mutex_lock(&chip->ttf.lock);
	rc = fg_get_time_to_full_locked(chip, val);
	mutex_unlock(&chip->ttf.lock);
	return rc;
}

#define CENTI_ICORRECT_C0	105
#define CENTI_ICORRECT_C1	20
static int fg_get_time_to_empty(struct fg_chip *chip, int *val)
{
	int rc, ibatt_avg, msoc, full_soc, act_cap_mah, divisor;

	rc = fg_circ_buf_median(&chip->ttf.ibatt, &ibatt_avg);
	if (rc < 0) {
		/* try to get instantaneous current */
		rc = fg_get_battery_current(chip, &ibatt_avg);
		if (rc < 0) {
			pr_err("failed to get battery current, rc=%d\n", rc);
			return rc;
		}
	}

	ibatt_avg /= MILLI_UNIT;
	/* clamp ibatt_avg to 100mA */
	if (ibatt_avg < 100)
		ibatt_avg = 100;

	rc = fg_get_prop_capacity(chip, &msoc);
	if (rc < 0) {
		pr_err("Error in getting capacity, rc=%d\n", rc);
		return rc;
	}

	rc = fg_get_sram_prop(chip, FG_SRAM_ACT_BATT_CAP, &act_cap_mah);
	if (rc < 0) {
		pr_err("Error in getting ACT_BATT_CAP, rc=%d\n", rc);
		return rc;
	}

	rc = fg_get_sram_prop(chip, FG_SRAM_FULL_SOC, &full_soc);
	if (rc < 0) {
		pr_err("failed to get full soc rc=%d\n", rc);
		return rc;
	}
	full_soc = DIV_ROUND_CLOSEST(((u16)full_soc >> 8) * FULL_CAPACITY,
								FULL_SOC_RAW);
	act_cap_mah = full_soc * act_cap_mah / 100;

	divisor = CENTI_ICORRECT_C0 * 100 + CENTI_ICORRECT_C1 * msoc;
	divisor = ibatt_avg * divisor / 100;
	divisor = max(100, divisor);
	*val = act_cap_mah * msoc * HOURS_TO_SECONDS / divisor;
	return 0;
}

//ASUS_BSP +++
static int get_bat_charging_status(struct fg_chip *chip);
#define	COMPENSATION_COUNT_THD	25
#define DELTA_SOC_BACKUP_FILE  		"/asdf/gaugeMappingBackup"
static int init_asus_capacity(void)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8] = "";
	int l_result = -1;
	//int readlen = 0;

	fp = filp_open(DELTA_SOC_BACKUP_FILE, O_RDWR, 0);
	if (IS_ERR_OR_NULL(fp)) {
		BAT_DBG_E("%s: open (%s) fail\n", __func__, DELTA_SOC_BACKUP_FILE);
		return -ENOENT;	/*No such file or directory*/
	}

	/*For purpose that can use read/write system call*/
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	/*
	if (fp->f_op != NULL ) 
		BAT_DBG_E("F_OP\n");
	if (fp->f_op->read != NULL) 
		BAT_DBG_E("read\n");
	*/
	vfs_read(fp,buf,6,&pos_lsts);

	//use vfs_write due to null f_op->read	
	/*
	if (fp->f_op != NULL && fp->f_op->read != NULL) {
		pos_lsts = 0;
		readlen = fp->f_op->read(fp, buf, 6, &pos_lsts);
		buf[readlen] = '\0';		
	} else {
		BAT_DBG_E("%s: f_op=NULL or op->read=NULL\n", __func__);
		return -ENXIO;	
	}
	*/
	set_fs(old_fs);
	filp_close(fp, NULL);

	sscanf(buf, "%d", &l_result);
	if(l_result < 0) {
		BAT_DBG_E("%s: FAIL. (%d)\n", __func__, l_result);
		return 0;	/*Invalid argument*/
	} else {
		BAT_DBG("%s: %d\n", __func__, l_result);
	}
	
	return l_result;
}
void backup_asus_capacity(int input)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t pos_lsts = 0;
	char buf[8] = "";

	sprintf(buf, "%d", input);
	
	fp = filp_open(DELTA_SOC_BACKUP_FILE, O_RDWR | O_CREAT , 0666);
	if (IS_ERR_OR_NULL(fp)) {
		BAT_DBG_E("%s: open (%s) fail\n", __func__, DELTA_SOC_BACKUP_FILE);
		return;
	}

	/*For purpose that can use read/write system call*/
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	/*
	if (fp->f_op != NULL ) 
		BAT_DBG_E("F_OP\n");
	if (fp->f_op->write != NULL) 
		BAT_DBG_E("write\n");
	*/
	vfs_write(fp,buf,6,&pos_lsts);

	//use vfs_write due to null f_op->write
	/*
	if (fp->f_op != NULL && fp->f_op->write != NULL) {
		pos_lsts = 0;
		fp->f_op->write(fp, buf, strlen(buf), &fp->f_pos);				
	} else {
		BAT_DBG_E("%s: f_op=NULL or op->write=NULL\n", __func__);
		return;
	}
	*/
	set_fs(old_fs);
	filp_close(fp, NULL);

	BAT_DBG("%s : %s\n", __func__, buf);
}


/* 
[bsp] for safty, delta_soc should less than msoc/10*2
*/

void init_asus_capacity_worker(struct work_struct *work)
{

	int rc,msoc, soc255,raw_delta;
	bool is_valid = false;
	static int cnt=0;
	
	g_asusCapacityShift = init_asus_capacity();
	raw_delta = g_asusCapacityShift;
	if(raw_delta == -ENOENT ){
		if(cnt++ < 5)
			schedule_delayed_work(&init_asus_capacity_work, 5* HZ);
		goto out;
	}

	if(g_fgChip){
	rc = fg_get_msoc(g_fgChip, &msoc);
	if(rc<0)	
		msoc =50;
	msoc /= 10;

	fg_get_msoc_raw(g_fgChip, &soc255);

	if(raw_delta > 0 && raw_delta < msoc*2){
		is_valid = true;
		g_fgChip->last_msoc = soc255;		
		g_fgChip->delta_soc = raw_delta;
		g_fgChip->maint_soc = raw_delta+soc255;		
	}
	else
		g_asusCapacityShift=0;

	if(is_valid && raw_delta > FULL_SOC_RAW-soc255){
		g_fgChip->delta_soc = FULL_SOC_RAW-soc255;
		g_fgChip->maint_soc = FULL_SOC_RAW;
		backup_delta_soc(FULL_SOC_RAW-soc255);
	}
	

	if (batt_psy_initialized(g_fgChip))
		power_supply_changed(g_fgChip->batt_psy);

	BAT_DBG("%s: retrieve delta_soc %d, valid %d, result %d, msoc %d\n",
		EVT_TAG, raw_delta,is_valid, g_asusCapacityShift, soc255);
	}

out:	
	//reset	
	g_asusCapacityShift = 0;
}

void backup_asus_capacity_worker(struct work_struct *work)
{
	backup_asus_capacity(g_asusCapacityShift);
}
/*
regular_check_soc_worker:
Due to delta-soc be 2% sometimes, check wether calling power supply change regularly.
It temps to detect 1% soc change before receiving delta-soc irq.
However, this work just calc. soc instead of running fg_update_maint_soc() or fg_charge_full_update()
*/

static struct timespec g_prev_check_soc_time;
void regular_check_soc_worker(struct work_struct *work){

	struct fg_chip *chip = g_fgChip;
	int maint_msoc,msoc,soc255,rc=0;
	union power_supply_propval prop = {0, };
	bool online,call_change=false;

	if(!rc)
	rc = fg_get_msoc(chip, &msoc);

	if(!rc)	
	rc = fg_get_msoc_raw(chip, &soc255);	

	if(!rc)
	rc = !usb_psy_initialized(chip);

	if(!rc)	
	rc = power_supply_get_property(chip->usb_psy, POWER_SUPPLY_PROP_ONLINE,&prop);

	if(rc)
		goto do_nothing;

	online = (prop.intval != POWER_SUPPLY_PL_NONE);

	if(chip->delta_soc > 0){
		maint_msoc = DIV_ROUND_CLOSEST((soc255+chip->delta_soc) * FULL_CAPACITY, FULL_SOC_RAW); //ASUS_BSP

		if(!online && (msoc < g_reporting_soc))
			call_change = true;

		if(!online && (maint_msoc < g_maint_msoc))
			call_change = true;

	} else {
	
		if(online && (msoc > g_reporting_soc))
			call_change = true;
		else if(!online && (msoc < g_reporting_soc))
			call_change = true;
	}


	if(call_change && chip->delta_soc){
		BAT_DBG("detect soc changed, msoc %d, soc255 %d, g_maint_msoc %d, online %d, delta_soc %d\n",
			 msoc, soc255, g_maint_msoc, online, chip->delta_soc);		
		g_maint_msoc = maint_msoc;
		power_supply_changed(chip->batt_psy);		
	}
	else if(call_change){
		BAT_DBG("detect soc changed, msoc %d, soc255 %d, g_reporting_soc %d, online %d, delta_soc %d\n",
			 msoc, soc255, g_reporting_soc, online, chip->delta_soc);		
		g_reporting_soc = msoc;
		power_supply_changed(chip->batt_psy);		
	}
	
	do_nothing:

	if(rc < 0)
		BAT_DBG("%s: do nothing due to error, rc %d\n",__func__,rc);	

	g_prev_check_soc_time = current_kernel_time();
		
	schedule_delayed_work(&regular_check_soc_work, SOC_CHECK_INTERVAL * HZ);


}

void fix_maint_soc_worker(struct work_struct *work){

	struct fg_chip *chip = g_fgChip;
	union power_supply_propval prop = {0, };
	int rc=0;
	
	if(usb_psy_initialized(chip)){
		rc = power_supply_get_property(chip->usb_psy, POWER_SUPPLY_PROP_ONLINE,
			&prop);
		if (rc < 0) 
			pr_err("Error in getting usb online, rc=%d\n", rc);	
	}
	else
		pr_err("Error in getting usb_psy\n");
		

	if(prop.intval == POWER_SUPPLY_PL_NONE){
		chip->maint_soc -= 1;
		chip->delta_soc -= 1;
		backup_delta_soc(chip->delta_soc);
		BAT_DBG("%s delay work fix maint_soc by 1%% for compensation\n",EVT_TAG);
		if (batt_psy_initialized(chip))
			power_supply_changed(chip->batt_psy);
	}
	else
		BAT_DBG("%s cable online, abort fix maint_soc\n",EVT_TAG);
	
}

//ASUS_BSP battery safety upgrade +++
static void init_battery_safety(struct fg_chip *chip)
{
	chip->condition1_battery_time = BATTERY_USE_TIME_CONDITION1;
	chip->condition2_battery_time = BATTERY_USE_TIME_CONDITION2;
	chip->condition1_cycle_count = CYCLE_COUNT_CONDITION1;
	chip->condition2_cycle_count = CYCLE_COUNT_CONDITION2;
	chip->condition1_temp_vol_time = HIGH_TEMP_VOL_TIME_CONDITION1;
	chip->condition2_temp_vol_time = HIGH_TEMP_VOL_TIME_CONDITION2;
	chip->condition1_temp_time = HIGH_TEMP_TIME_CONDITION1;
	chip->condition2_temp_time = HIGH_TEMP_TIME_CONDITION2;
	chip->condition1_vol_time = HIGH_VOL_TIME_CONDITION1;
	chip->condition2_vol_time = HIGH_VOL_TIME_CONDITION2;
}

static int file_op(const char *filename, loff_t offset, char *buf, int length, int operation)
{
	int filep;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if(FILE_OP_READ == operation)
		filep= sys_open(filename, O_RDONLY | O_CREAT, 0666);
	else if(FILE_OP_WRITE == operation)
		filep= sys_open(filename, O_RDWR | O_CREAT, 0666);
	else {
		pr_err("Unknown partition op err!\n");
		return -1;
	}
	if(filep < 0) {
		pr_err("open %s err! error code:%d\n", filename, filep);
		return -1;
	}
   else
        fg_dbg(g_fgChip, FG_STATUS, "open %s success!\n", filename);

	sys_lseek(filep, offset, SEEK_SET);
	if(FILE_OP_READ == operation)
		sys_read(filep, buf, length);
	else if(FILE_OP_WRITE == operation) {
		sys_write(filep, buf, length);
		sys_fsync(filep);
	}
	set_fs(old_fs);
	sys_close(filep);
	return length;
}

static int backup_bat_percentage(void)
{
	char buf[1]={0};
	int bat_percent, rc;

	if(0 == g_cycle_count_data.reload_condition){
		bat_percent = 0;
	}else if(1 == g_cycle_count_data.reload_condition){
		bat_percent = 95;
	}else if(2 == g_cycle_count_data.reload_condition){
		bat_percent = 90;
	}
	sprintf(buf, "%d\n", bat_percent);
	BAT_DBG("bat_percent=%d;reload_condition=%d\n", bat_percent, g_cycle_count_data.reload_condition);	

	rc = file_op(BAT_PERCENT_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
		(char *)&buf, sizeof(char), FILE_OP_WRITE);
	if(rc<0)
		pr_err("%s:Write file:%s err!\n", __FUNCTION__, BAT_PERCENT_FILE_NAME);

	return rc;
}

static int backup_bat_cyclecount(void)
{
	char buf[30]={0};
	int rc;

	sprintf(buf, "%d\n", g_cycle_count_data.cycle_count);
	BAT_DBG("cycle_count=%d\n", g_cycle_count_data.cycle_count);	

	rc = file_op(BAT_CYCLE_SD_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
		(char *)&buf, sizeof(char)*30, FILE_OP_WRITE);
	if(rc<0)
		pr_err("%s:Write file:%s err!\n", __FUNCTION__, BAT_CYCLE_SD_FILE_NAME);

	return rc;
}

static int backup_bat_safety(void)
{
	char buf[70]={0};
	int rc;

	sprintf(buf, "%lu,%d,%lu,%lu,%lu\n", 
		g_cycle_count_data.battery_total_time, 
		g_cycle_count_data.cycle_count, 
		g_cycle_count_data.high_temp_total_time, 
		g_cycle_count_data.high_vol_total_time,
		g_cycle_count_data.high_temp_vol_time);		

	rc = file_op(BAT_SAFETY_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
		(char *)&buf, sizeof(char)*70, FILE_OP_WRITE);
	if(rc<0)
		pr_err("%s:Write file:%s err!\n", __FUNCTION__, BAT_SAFETY_FILE_NAME);

	return rc;
}

static int init_batt_cycle_count_data(void)
{
	int rc = 0;
	struct CYCLE_COUNT_DATA buf;

	/* Read cycle count data from emmc */
	rc = file_op(CYCLE_COUNT_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
		(char*)&buf, sizeof(struct CYCLE_COUNT_DATA), FILE_OP_READ);
	if(rc < 0) {
		pr_err("Read cycle count file failed!\n");
		return rc;
	}

	/* Check data validation */
	if(buf.magic != CYCLE_COUNT_DATA_MAGIC) {
		pr_err("data validation!\n");
		file_op(CYCLE_COUNT_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
		(char*)&g_cycle_count_data, sizeof(struct CYCLE_COUNT_DATA), FILE_OP_WRITE);
		return -1;
	}else {
		/* Update current value */
		BAT_DBG("Update current value!\n");
		g_cycle_count_data.cycle_count = buf.cycle_count;
		g_cycle_count_data.high_temp_total_time = buf.high_temp_total_time;
		g_cycle_count_data.high_temp_vol_time = buf.high_temp_vol_time;
		g_cycle_count_data.high_vol_total_time = buf.high_vol_total_time;
		g_cycle_count_data.reload_condition = buf.reload_condition;
		g_cycle_count_data.battery_total_time = buf.battery_total_time;

		rc = backup_bat_percentage();
		if(rc < 0){
			pr_err("backup_bat_percentage failed!\n");
			return -1;
		}

		rc = backup_bat_cyclecount();
		if(rc < 0){
			pr_err("backup_bat_cyclecount failed!\n");
			return -1;
		}
		
		rc = backup_bat_safety();
		if(rc < 0){
			pr_err("backup_bat_cyclecount failed!\n");
			return -1;
		}

		BAT_DBG("reload_condition=%d;high_temp_total_time=%lu;high_temp_vol_time=%lu;high_vol_total_time=%lu;battery_total_time=%lu\n",
			buf.reload_condition, buf.high_temp_total_time,buf.high_temp_vol_time,buf.high_vol_total_time,buf.battery_total_time);
	}
	BAT_DBG("Cycle count data initialize success!\n");
	g_cyclecount_initialized = true;
	set_full_charging_voltage();
	return 0;
}

extern int batt_safety_csc_backup(void);
static void write_back_cycle_count_data(void)
{
	int rc;

	backup_bat_percentage();
	backup_bat_cyclecount();
	backup_bat_safety();
	batt_safety_csc_backup();
	
	rc = file_op(CYCLE_COUNT_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
		(char *)&g_cycle_count_data, sizeof(struct CYCLE_COUNT_DATA), FILE_OP_WRITE);
	if(rc<0)
		pr_err("%s:Write file:%s err!\n", __FUNCTION__, CYCLE_COUNT_FILE_NAME);
}

#if 0
static void reload_battery_profile(struct fg_chip *chip){
	int rc;
	
	rc = fg_get_batt_profile(g_fgChip);
	if (rc < 0) {
		g_fgChip->soc_reporting_ready = true;
		pr_err("Error in getting battery profile, rc:%d\n", rc);
		//return IRQ_HANDLED;
	}

	clear_battery_profile(g_fgChip);
	schedule_delayed_work(&g_fgChip->profile_load_work, 0);
}
#endif

static void asus_reload_battery_profile(struct fg_chip *chip, int value){

	//save current status
	write_back_cycle_count_data();

	//reloade battery
	//reload_battery_profile(chip);
	set_full_charging_voltage();

	BAT_DBG("new profile is value=%d\n",value);
}

static void asus_judge_reload_condition(struct fg_chip *chip)
{
	int temp_condition = 0;
	int cycle_count = 0;
	bool full_charge;
	unsigned long local_high_vol_time = g_cycle_count_data.high_vol_total_time;
	unsigned long local_high_temp_time = g_cycle_count_data.high_temp_total_time;
	//unsigned long local_high_temp_vol_time = g_cycle_count_data.high_temp_vol_time;
	unsigned long local_battery_total_time = g_cycle_count_data.battery_total_time;

	temp_condition = g_cycle_count_data.reload_condition;
	if(temp_condition >= 2){ //if condition=2 will return
		return ;
	}

	//only full charger can load new profile
	full_charge = chip->charge_done;
	if(!full_charge)
		return ;

	//1.judge battery using total time
	if(local_battery_total_time >= chip->condition2_battery_time){
		g_cycle_count_data.reload_condition = 2;
		goto DONE;
	}else if(local_battery_total_time >= chip->condition1_battery_time &&
		local_battery_total_time < chip->condition2_battery_time){
		g_cycle_count_data.reload_condition = 1;
	}

	//2. judge battery cycle count
	cycle_count = g_cycle_count_data.cycle_count;
#if 0 //disable reloade condition with cycle_count
	if(cycle_count >= chip->condition2_cycle_count){
		g_cycle_count_data.reload_condition = 2;
		goto DONE;
	}else if(cycle_count >= chip->condition1_cycle_count &&
		cycle_count < chip->condition2_cycle_count){
		g_cycle_count_data.reload_condition = 1;
	}
#endif

	//3. judge high temp and voltage condition
#if 0 //disable reloade condition with high_temp_vol
	if(local_high_temp_vol_time >= chip->condition2_temp_vol_time){
		g_cycle_count_data.reload_condition = 2;
		goto DONE;
	}else if(local_high_temp_vol_time >= chip->condition1_temp_vol_time &&
		local_high_temp_vol_time < chip->condition2_temp_vol_time){
		g_cycle_count_data.reload_condition = 1;
	}
#endif

	//4. judge high temp condition
	if(local_high_temp_time >= chip->condition2_temp_time){
		g_cycle_count_data.reload_condition = 2;
		goto DONE;
	}else if(local_high_temp_time >= chip->condition1_temp_time &&
		local_high_temp_time < chip->condition2_temp_time){
		g_cycle_count_data.reload_condition = 1;
	}

	//5. judge high voltage condition
	if(local_high_vol_time >= chip->condition2_vol_time){
		g_cycle_count_data.reload_condition = 2;
		goto DONE;
	}else if(local_high_vol_time >= chip->condition1_vol_time &&
		local_high_vol_time < chip->condition2_vol_time){
		g_cycle_count_data.reload_condition = 1;
	}

DONE:
	if(temp_condition != g_cycle_count_data.reload_condition)
		asus_reload_battery_profile(chip, g_cycle_count_data.reload_condition);

}

unsigned long last_battery_total_time = 0;
unsigned long last_high_temp_time = 0;
unsigned long last_high_vol_time = 0;
unsigned long last_high_temp_vol_time = 0;
extern unsigned long asus_qpnp_rtc_read_time(void);

static void calculation_time_fun(int type)
{
	unsigned long now_time;
	unsigned long temp_time = 0;

	now_time = asus_qpnp_rtc_read_time();
	if(now_time < 0){
		pr_err("asus read rtc time failed!\n");
		return ;
	}

	switch(type){
		case TOTOL_TIME_CAL_TYPE:
			if(0 == last_battery_total_time){
				last_battery_total_time = now_time;
				BAT_DBG("now_time=%lu;last_battery_total_time=%lu\n", now_time, g_cycle_count_data.battery_total_time);
			}else{
				temp_time = now_time - last_battery_total_time;
				if(temp_time > 0)
					g_cycle_count_data.battery_total_time += temp_time;
				last_battery_total_time = now_time;
			}
		break;

		case HIGH_VOL_CAL_TYPE:
			if(0 == last_high_vol_time){
				last_high_vol_time = now_time;
				BAT_DBG("now_time=%lu;high_vol_total_time=%lu\n", now_time, g_cycle_count_data.high_vol_total_time);
			}else{
				temp_time = now_time - last_high_vol_time;
				if(temp_time > 0)
					g_cycle_count_data.high_vol_total_time += temp_time;
				last_high_vol_time = now_time;
			}
		break;

		case HIGH_TEMP_CAL_TYPE:
			if(0 == last_high_temp_time){
				last_high_temp_time = now_time;
				BAT_DBG("now_time=%lu;high_temp_total_time=%lu\n", now_time, g_cycle_count_data.high_temp_total_time);
			}else{
				temp_time = now_time - last_high_temp_time;
				if(temp_time > 0)
					g_cycle_count_data.high_temp_total_time += temp_time;
				last_high_temp_time = now_time;
			}
		break;

		case HIGH_TEMP_VOL_CAL_TYPE:
			if(0 == last_high_temp_vol_time){
				last_high_temp_vol_time = now_time;
				BAT_DBG("now_time=%lu;high_temp_vol_time=%lu\n", now_time, g_cycle_count_data.high_temp_vol_time);
			}else{
				temp_time = now_time - last_high_temp_vol_time;
				if(temp_time > 0)
					g_cycle_count_data.high_temp_vol_time += temp_time;
				last_high_temp_vol_time = now_time;
			}
		break;
	}
}

#if 0
static void calculation_battery_time_fun(unsigned long time)
{
	static unsigned long last_time = 0;
	unsigned long count_time = 0;

	if(last_time > 0){
		count_time = ((time-last_time)>0) ? (time-last_time):0;
		g_cycle_count_data.battery_total_time += count_time;
		last_time = time;
		return ;
	}

	if(g_cycle_count_data.battery_total_time <= time){
		 g_cycle_count_data.battery_total_time = time;
	}else{//add up time when pull out the battery
		g_cycle_count_data.battery_total_time += time;
	}
	BAT_DBG("time=%lu;battery_total_time=%lu\n", time,g_cycle_count_data.battery_total_time);
	last_time = time;
}
#endif 
static int write_test_value = 0;
static void update_battery_safe(struct fg_chip *chip)
{
	int rc;
	int temp;
	int capacity;
	unsigned long now_time;

	//BAT_DBG("%s +", __func__);

	if(rtc_probe_done != true){
		pr_err("rtc probe is not ready");
		return;
	}

	if(g_cyclecount_initialized != true){
		rc = init_batt_cycle_count_data();
		if(rc < 0){
			pr_err("cyclecount is not initialized");
			return;
		}
	}
	
	rc = fg_get_battery_temp(chip, &temp);
	if (rc < 0) {
		pr_err("Error in getting battery temp, rc=%d\n", rc);
		return;
	}

	rc = fg_get_prop_capacity(chip, &capacity);
	if (rc < 0) {
		pr_err("Error in getting capacity, rc=%d\n", rc);
		return;
	}

	now_time = asus_qpnp_rtc_read_time();

	if(now_time < 0){
		pr_err("asus read rtc time failed!\n");
		return ;
	}

#if 0
	if(write_test_value != 1){ //skip battery time test
		calculation_battery_time_fun(now_time);
	}
#endif
	calculation_time_fun(TOTOL_TIME_CAL_TYPE);

	if(capacity == FULL_CAPACITY_VALUE){
		calculation_time_fun(HIGH_VOL_CAL_TYPE);	
	}else{
		last_high_vol_time = 0; //exit high vol
	}

	if(temp >= HIGHER_TEMP){
		calculation_time_fun(HIGH_TEMP_CAL_TYPE);
	}else{
		last_high_temp_time = 0; //exit high temp
	}

	if(temp >= HIGH_TEMP && capacity == FULL_CAPACITY_VALUE){
		calculation_time_fun(HIGH_TEMP_VOL_CAL_TYPE);
	}else{
		last_high_temp_vol_time = 0; //exit high temp and vol
	}

	asus_judge_reload_condition(chip);
	write_back_cycle_count_data();
	//BAT_DBG("%s -", __func__);
}

void battery_safety_upgrade_data_polling(int time) {
	cancel_delayed_work(&battery_safety_work);
	schedule_delayed_work(&battery_safety_work, time * HZ);
}

void battery_safety_worker(struct work_struct *work)
{
	update_battery_safe(g_fgChip);
	battery_safety_upgrade_data_polling(BATTERY_SAFETY_UPGRADE_TIME); // update each hour
}
//ASUS_BSP battery safety upgrade ---

//ASUS_BS battery health upgrade +++
static void battery_health_data_reset(void){
	g_bat_health_data.bat_current = 0;
	g_bat_health_data.bat_current_avg = 0;
	g_bat_health_data.accumulate_time = 0;
	g_bat_health_data.accumulate_current = 0;
	g_bat_health_data.start_time = 0;
	g_bat_health_data.end_time = 0;
	g_bathealth_trigger = false;
	g_last_bathealth_trigger = false;
	wake_unlock(&bat_health_lock);
}

extern int batt_health_csc_backup(void);
static int resotre_bat_health(void)
{
	int i=0, rc = 0;

	memset(&g_bat_health_data_backup,0,sizeof(struct BAT_HEALTH_DATA_BACKUP)*BAT_HEALTH_NUMBER_MAX);
	
	/* Read cycle count data from emmc */
	rc = file_op(BAT_HEALTH_DATA_FILE_NAME, BAT_HEALTH_DATA_OFFSET,
		(char*)&g_bat_health_data_backup, sizeof(struct BAT_HEALTH_DATA_BACKUP)*BAT_HEALTH_NUMBER_MAX, FILE_OP_READ);
	if(rc < 0) {
		pr_err("Read bat health file failed!\n");
		return -1;
	}

	BAT_DBG("%s: index(%d)\n",__FUNCTION__, g_bat_health_data_backup[0].health);
	for(i=1; i<BAT_HEALTH_NUMBER_MAX;i++){
		BAT_DBG("%s %d",g_bat_health_data_backup[i].date, g_bat_health_data_backup[i].health);
	}

	g_health_upgrade_index = g_bat_health_data_backup[0].health;
	g_bathealth_initialized = true;

	batt_health_csc_backup();
	batt_safety_csc_backup();
	return 0;
}

static int backup_bat_health(void)
{
	int bat_health, rc;
	struct timespec ts;
	struct rtc_time tm; 
	int health_t;
	int count=0, i=0;
	unsigned long long bat_health_accumulate=0;

	getnstimeofday(&ts); 
	rtc_time_to_tm(ts.tv_sec,&tm); 

	bat_health = g_bat_health_data.bat_health;

	if(g_health_upgrade_index == BAT_HEALTH_NUMBER_MAX-1){
		g_health_upgrade_index = 1;
	}else{
		g_health_upgrade_index++;
	}

	sprintf(g_bat_health_data_backup[g_health_upgrade_index].date, "%d-%02d-%02d %02d:%02d:%02d", tm.tm_year+1900,tm.tm_mon+1, tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);
	g_bat_health_data_backup[g_health_upgrade_index].health = bat_health;
	g_bat_health_data_backup[0].health = g_health_upgrade_index;

	BAT_DBG("%s ===== Health history ====\n",__FUNCTION__);
	for(i=1;i<BAT_HEALTH_NUMBER_MAX;i++){
		if(g_bat_health_data_backup[i].health!=0){
			count++;
			bat_health_accumulate += g_bat_health_data_backup[i].health;
			BAT_DBG("%s %02d:%d\n",__FUNCTION__,i,g_bat_health_data_backup[i].health);
		}
	}
	BAT_DBG("%s ========================\n",__FUNCTION__);

	if(count==0){
		BAT_DBG("%s battery health value is empty\n",__FUNCTION__);
		return -1;
	}
	health_t = bat_health_accumulate*10/count;
	g_bat_health_avg = (int)(health_t + 5)/10;
	g_bat_health_data_backup[g_health_upgrade_index].health = g_bat_health_avg;

	rc = file_op(BAT_HEALTH_DATA_FILE_NAME, BAT_HEALTH_DATA_OFFSET,
		(char *)&g_bat_health_data_backup, sizeof(struct BAT_HEALTH_DATA_BACKUP)*BAT_HEALTH_NUMBER_MAX, FILE_OP_WRITE);
	if(rc<0){
		pr_err("%s:Write file:%s err!\n", __FUNCTION__, BAT_HEALTH_DATA_FILE_NAME);
	}

	return rc;
}

int batt_health_csc_backup(void){
	int rc=0, i=0;
	struct BAT_HEALTH_DATA_BACKUP buf[BAT_HEALTH_NUMBER_MAX];
	char buf2[BAT_HEALTH_NUMBER_MAX][30];

	memset(&buf,0,sizeof(struct BAT_HEALTH_DATA_BACKUP)*BAT_HEALTH_NUMBER_MAX);
	memset(&buf2,0,sizeof(char)*BAT_HEALTH_NUMBER_MAX*30);
	
	rc = file_op(BAT_HEALTH_DATA_FILE_NAME, BAT_HEALTH_DATA_OFFSET,
		(char*)&buf, sizeof(struct BAT_HEALTH_DATA)*BAT_HEALTH_NUMBER_MAX, FILE_OP_READ);
	if(rc < 0) {
		BAT_DBG_E("Read bat health file failed!\n");
		//return rc;
	}

	for(i=1;i<BAT_HEALTH_NUMBER_MAX;i++){
		if(buf[i].health!=0){
			sprintf(&buf2[i-1][0], "%s [%d]\n", buf[i].date, buf[i].health);
		}
	}

	rc = file_op(BAT_HEALTH_DATA_SD_FILE_NAME, BAT_HEALTH_DATA_OFFSET,
	(char *)&buf2, sizeof(char)*BAT_HEALTH_NUMBER_MAX*30, FILE_OP_WRITE);
	if(rc < 0 ) 
		BAT_DBG_E("Write bat health file failed!\n");


	BAT_DBG("%s Done! \n",__FUNCTION__);
	return rc;
}

static void update_battery_health(struct fg_chip *chip){
	int bat_capacity, bat_current, delta_p;
	unsigned long T;
	int health_t;

	if(g_health_upgrade_enable != true){
		return;
	}
	
	if(g_bathealth_initialized != true){
		resotre_bat_health();
		return;
	}

	if(!chip->online_status){
		if(g_last_bathealth_trigger == true){
			battery_health_data_reset();
		}
		return;
	}
	
	fg_get_prop_capacity(chip, &bat_capacity);

	if(bat_capacity == g_health_upgrade_start_level && g_bat_health_data.start_time == 0){
		wake_lock(&bat_health_lock);
		g_bathealth_trigger = true;
		g_bat_health_data.start_time = asus_qpnp_rtc_read_time();
	}
	if(bat_capacity > g_health_upgrade_end_level){
		g_bathealth_trigger = false;
	}
	if(g_last_bathealth_trigger == false && g_bathealth_trigger == false){
		return;
	}
	
	if( g_bathealth_trigger ){		
//		if(g_screen_on == true){
//			return;
//		}
		fg_get_battery_current(chip, &bat_current);

		g_bat_health_data.accumulate_time += g_health_upgrade_upgrade_time;
		g_bat_health_data.bat_current = -bat_current;
		g_bat_health_data.accumulate_current += g_bat_health_data.bat_current;
		g_bat_health_data.bat_current_avg = g_bat_health_data.accumulate_current/g_bat_health_data.accumulate_time;
			
		if(g_health_debug_enable)
			BAT_DBG("%s accumulate_time(%llu), accumulate_current(%llu), bat_current(%d), bat_current_avg(%llu), bat_capacity(%d)",__FUNCTION__, g_bat_health_data.accumulate_time, g_bat_health_data.accumulate_current/1000, g_bat_health_data.bat_current/1000, g_bat_health_data.bat_current_avg/1000, bat_capacity);

		if(bat_capacity >= g_health_upgrade_end_level){			
			g_bat_health_data.end_time = asus_qpnp_rtc_read_time();
			delta_p = g_health_upgrade_end_level - g_health_upgrade_start_level;
			T = g_bat_health_data.end_time - g_bat_health_data.start_time;
			health_t = (g_bat_health_data.bat_current_avg*T)*10/(unsigned long long)(ZE620KL_DESIGNED_CAPACITY*delta_p)/(unsigned long long)360;
			g_bat_health_data.bat_health = (int)((health_t + 5)/10);
	
			backup_bat_health();
			batt_health_csc_backup();
			BAT_DBG("%s battery health = (%d,%d), T(%lu), bat_current_avg(%llu)",__FUNCTION__, g_bat_health_data.bat_health, g_bat_health_avg, T, g_bat_health_data.bat_current_avg/1000);
			
			battery_health_data_reset();
		}else{
				//do nothing
		}
	}else{
		battery_health_data_reset();
	}
	g_last_bathealth_trigger = g_bathealth_trigger;
}

void battery_health_upgrade_data_polling(int time) {
	cancel_delayed_work(&battery_health_work);
	schedule_delayed_work(&battery_health_work, time * HZ);
}

void battery_health_worker(struct work_struct *work)
{
	update_battery_health(g_fgChip);
	battery_health_upgrade_data_polling(g_health_upgrade_upgrade_time);
}

#if 0
static void update_battery_metadata(struct fg_chip *chip){
	//copy health data to sdcard
	batt_health_csc_backup();
}

void battery_metadata_upgrade_data_polling(int time) {
	cancel_delayed_work(&battery_metadata_work);
	schedule_delayed_work(&battery_metadata_work, time * HZ);
}

void battery_metadata_worker(struct work_struct *work)
{
	update_battery_metadata(g_fgChip);
	battery_metadata_upgrade_data_polling(BATTERY_METADATA_UPGRADE_TIME); // update each hour
}
#endif
//ASUS_BS battery health upgrade ---

//ASUS_BSP display callback function +++
static int fb_event_callback(struct notifier_block *self,
              unsigned long event, void *data)
{
    struct fb_event *evdata = data;

    if (!evdata) {
       pr_err("%s: event data not available\n", __func__);
       return NOTIFY_BAD;
    }

    /* handle only mdss fb device */
    if (strncmp("mdssfb", evdata->info->fix.id, 6))
       return NOTIFY_DONE;

    if (event == FB_EVENT_BLANK) {
       int *blank = evdata->data;

       switch (*blank) {
       case FB_BLANK_UNBLANK:
           g_screen_on = true;
           break;
       case FB_BLANK_POWERDOWN:
       case FB_BLANK_HSYNC_SUSPEND:
       case FB_BLANK_VSYNC_SUSPEND:
       case FB_BLANK_NORMAL:
           g_screen_on = false;
           break;
       default:
           pr_err("Unknown case in FB_EVENT_BLANK event\n");
           break;
       }
    }
	BAT_DBG("%s: g_screen_on:%d", __func__, g_screen_on);
    return 0;
}
//ASUS_BSP display callback function ---
//ASUS_BSP ---

static int fg_update_maint_soc(struct fg_chip *chip)
{
	int rc = 0, msoc, delta=0; //ASUS_BSP
	static int msoc_counter=0,count=0; //ASUS_BSP

	if (!chip->dt.linearize_soc)
		return 0;

	mutex_lock(&chip->charge_full_lock);
	if (chip->delta_soc <= 0)
		goto out;

	rc = fg_get_msoc_raw(chip, &msoc); //ASUS_BSP
	if (rc < 0) {
		pr_err("Error in getting msoc, rc=%d\n", rc);
		goto out;
	}

//ASUS_BSP +++
	g_reporting_soc = DIV_ROUND_CLOSEST(msoc * FULL_CAPACITY, FULL_SOC_RAW);

	if(chip->delta_soc){
		g_maint_msoc = DIV_ROUND_CLOSEST((msoc+ chip->delta_soc)* FULL_CAPACITY
			, FULL_SOC_RAW);
		
	}
	
	if (chip->delta_soc <= 0)
		goto out;


	if (msoc >= chip->maint_soc) {
		/*
		 * When the monotonic SOC goes above maintenance SOC, we should
		 * stop showing the maintenance SOC.
		 */
		 
		 //for resume and msoc exceed maint.
		if(!report_maint && chip->delta_soc != 0){
			//with cable?;
			if(chip->delta_soc + msoc >= FULL_SOC_RAW)
				chip->delta_soc = FULL_SOC_RAW - msoc;
			chip->maint_soc = chip->delta_soc + msoc;			
			report_maint = true;
			BAT_DBG("%s reporting maint_soc\n",EVT_TAG);
		}
		else{		
		chip->delta_soc = 0;
		chip->maint_soc = 0;
			msoc_counter=0;
			count=0;
			report_maint = false;
			BAT_DBG("%s soc catch maint_soc\n",EVT_TAG);
		}
		
		if(g_asusCapacityShift != chip->delta_soc)
			backup_delta_soc(chip->delta_soc);
		
	} 
	else if (msoc <= chip->last_msoc) {

		// "=" means charging, need not modify report_maint
		if(msoc < chip->last_msoc)
			report_maint = false;
		
		delta= chip->last_msoc-msoc;
		/* MSOC is decreasing. Decrease maintenance SOC as well */
		chip->maint_soc -= delta;
		msoc_counter+= delta;
		if (msoc_counter >=COMPENSATION_COUNT_THD) {
			/*
			 * Reduce the maintenance SOC additionally by 1 whenever
			 * it crosses a SOC multiple of 10.
			 */

			if(DIV_ROUND_CLOSEST(chip->maint_soc * FULL_CAPACITY, FULL_SOC_RAW)
				> DIV_ROUND_CLOSEST((chip->maint_soc-1) * FULL_CAPACITY, FULL_SOC_RAW)){

				schedule_delayed_work(&fix_maint_soc_work, 60*HZ);
			}
			else{			
			chip->maint_soc -= 1;
			chip->delta_soc -= 1;
				backup_delta_soc(chip->delta_soc);
				BAT_DBG("%s counter exceed %d, fix maint_soc by 1%% for compensation\n"
					,EVT_TAG,COMPENSATION_COUNT_THD);			
		}
			msoc_counter=0;
	}
	}else{
		count += msoc - chip->last_msoc;
		//try
		chip->delta_soc = chip->maint_soc - msoc;
		if(count >= 2
			&&97 >= DIV_ROUND_CLOSEST(chip->maint_soc * FULL_CAPACITY, FULL_SOC_RAW)){
			count=0;
			chip->maint_soc += 1;
			chip->delta_soc = chip->maint_soc - msoc;
			BAT_DBG("%s counter exceed %d, fix maint_soc by 1%% for compensation\n",EVT_TAG,2);
		}
		else if(count >= 3
			&&97 < DIV_ROUND_CLOSEST(chip->maint_soc * FULL_CAPACITY, FULL_SOC_RAW)){
			count=0;
			chip->maint_soc += 1;
			if(chip->maint_soc >= FULL_SOC_RAW)
				chip->maint_soc = FULL_SOC_RAW;
			chip->delta_soc = chip->maint_soc - msoc;
			BAT_DBG("%s counter exceed %d, fix maint_soc by 1%% for compensation\n",EVT_TAG,3);
		}
		if(g_asusCapacityShift != chip->delta_soc)
			backup_delta_soc(chip->delta_soc);
	}	


	BAT_DBG("msoc: %d last_msoc: %d maint_soc: %d delta_soc: %d cnt: %d\n",
		msoc, chip->last_msoc, chip->maint_soc, chip->delta_soc, msoc_counter);
//ASUS_BSP ---
	chip->last_msoc = msoc;
out:
	mutex_unlock(&chip->charge_full_lock);
	return rc;
}

static int fg_esr_validate(struct fg_chip *chip)
{
	int rc, esr_uohms;
	u8 buf[2];

	if (chip->dt.esr_clamp_mohms <= 0)
		return 0;

	rc = fg_get_sram_prop(chip, FG_SRAM_ESR, &esr_uohms);
	if (rc < 0) {
		pr_err("failed to get ESR, rc=%d\n", rc);
		return rc;
	}

	if (esr_uohms >= chip->dt.esr_clamp_mohms * 1000) {
		pr_debug("ESR %d is > ESR_clamp\n", esr_uohms);
		return 0;
	}

	esr_uohms = chip->dt.esr_clamp_mohms * 1000;
	fg_encode(chip->sp, FG_SRAM_ESR, esr_uohms, buf);
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_ESR].addr_word,
			chip->sp[FG_SRAM_ESR].addr_byte, buf,
			chip->sp[FG_SRAM_ESR].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR, rc=%d\n", rc);
		return rc;
	}

	fg_dbg(chip, FG_STATUS, "ESR clamped to %duOhms\n", esr_uohms);
	return 0;
}

static int fg_force_esr_meas(struct fg_chip *chip)
{
	int rc;
	int esr_uohms;

	mutex_lock(&chip->qnovo_esr_ctrl_lock);
	/* force esr extraction enable */
	rc = fg_sram_masked_write(chip, ESR_EXTRACTION_ENABLE_WORD,
			ESR_EXTRACTION_ENABLE_OFFSET, BIT(0), BIT(0),
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("failed to enable esr extn rc=%d\n", rc);
		goto out;
	}

	rc = fg_masked_write(chip, BATT_INFO_QNOVO_CFG(chip),
			LD_REG_CTRL_BIT, 0);
	if (rc < 0) {
		pr_err("Error in configuring qnovo_cfg rc=%d\n", rc);
		goto out;
	}

	rc = fg_masked_write(chip, BATT_INFO_TM_MISC1(chip),
			ESR_REQ_CTL_BIT | ESR_REQ_CTL_EN_BIT,
			ESR_REQ_CTL_BIT | ESR_REQ_CTL_EN_BIT);
	if (rc < 0) {
		pr_err("Error in configuring force ESR rc=%d\n", rc);
		goto out;
	}

	/*
	 * Release and grab the lock again after 1.5 seconds so that prepare
	 * callback can succeed if the request comes in between.
	 */
	mutex_unlock(&chip->qnovo_esr_ctrl_lock);

	/* wait 1.5 seconds for hw to measure ESR */
	msleep(1500);

	mutex_lock(&chip->qnovo_esr_ctrl_lock);
	rc = fg_masked_write(chip, BATT_INFO_TM_MISC1(chip),
			ESR_REQ_CTL_BIT | ESR_REQ_CTL_EN_BIT,
			0);
	if (rc < 0) {
		pr_err("Error in restoring force ESR rc=%d\n", rc);
		goto out;
	}

	/* If qnovo is disabled, then leave ESR extraction enabled */
	if (!chip->qnovo_enable)
		goto done;

	rc = fg_masked_write(chip, BATT_INFO_QNOVO_CFG(chip),
			LD_REG_CTRL_BIT, LD_REG_CTRL_BIT);
	if (rc < 0) {
		pr_err("Error in restoring qnovo_cfg rc=%d\n", rc);
		goto out;
	}

	/* force esr extraction disable */
	rc = fg_sram_masked_write(chip, ESR_EXTRACTION_ENABLE_WORD,
			ESR_EXTRACTION_ENABLE_OFFSET, BIT(0), 0,
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("failed to disable esr extn rc=%d\n", rc);
		goto out;
	}

done:
	fg_get_battery_resistance(chip, &esr_uohms);
	fg_dbg(chip, FG_STATUS, "ESR uohms = %d\n", esr_uohms);
out:
	mutex_unlock(&chip->qnovo_esr_ctrl_lock);
	return rc;
}

static int fg_prepare_for_qnovo(struct fg_chip *chip, int qnovo_enable)
{
	int rc = 0;

	mutex_lock(&chip->qnovo_esr_ctrl_lock);
	/* force esr extraction disable when qnovo enables */
	rc = fg_sram_masked_write(chip, ESR_EXTRACTION_ENABLE_WORD,
			ESR_EXTRACTION_ENABLE_OFFSET,
			BIT(0), qnovo_enable ? 0 : BIT(0),
			FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in configuring esr extraction rc=%d\n", rc);
		goto out;
	}

	rc = fg_masked_write(chip, BATT_INFO_QNOVO_CFG(chip),
			LD_REG_CTRL_BIT,
			qnovo_enable ? LD_REG_CTRL_BIT : 0);
	if (rc < 0) {
		pr_err("Error in configuring qnovo_cfg rc=%d\n", rc);
		goto out;
	}

	fg_dbg(chip, FG_STATUS, "%s for Qnovo\n",
		qnovo_enable ? "Prepared" : "Unprepared");
	chip->qnovo_enable = qnovo_enable;
out:
	mutex_unlock(&chip->qnovo_esr_ctrl_lock);
	return rc;
}

static void ttf_work(struct work_struct *work)
{
	struct fg_chip *chip = container_of(work, struct fg_chip,
					    ttf_work.work);
	int rc, ibatt_now, vbatt_now, ttf;
	ktime_t ktime_now;

	mutex_lock(&chip->ttf.lock);
	if (chip->charge_status != POWER_SUPPLY_STATUS_CHARGING &&
			chip->charge_status != POWER_SUPPLY_STATUS_DISCHARGING)
		goto end_work;

	rc = fg_get_battery_current(chip, &ibatt_now);
	if (rc < 0) {
		pr_err("failed to get battery current, rc=%d\n", rc);
		goto end_work;
	}

	rc = fg_get_battery_voltage(chip, &vbatt_now);
	if (rc < 0) {
		pr_err("failed to get battery voltage, rc=%d\n", rc);
		goto end_work;
	}

	fg_circ_buf_add(&chip->ttf.ibatt, ibatt_now);
	fg_circ_buf_add(&chip->ttf.vbatt, vbatt_now);

	if (chip->charge_status == POWER_SUPPLY_STATUS_CHARGING) {
		rc = fg_get_time_to_full_locked(chip, &ttf);
		if (rc < 0) {
			pr_err("failed to get ttf, rc=%d\n", rc);
			goto end_work;
		}

		/* keep the wake lock and prime the IBATT and VBATT buffers */
		if (ttf < 0) {
			/* delay for one FG cycle */
			queue_delayed_work(system_power_efficient_wq,
				&chip->ttf_work, msecs_to_jiffies(1500));
			mutex_unlock(&chip->ttf.lock);
			return;
		}

		/* update the TTF reference point every minute */
		ktime_now = ktime_get_boottime();
		if (ktime_ms_delta(ktime_now,
				   ms_to_ktime(chip->ttf.last_ms)) > 60000 ||
				   chip->ttf.last_ms == 0) {
			chip->ttf.last_ttf = ttf;
			chip->ttf.last_ms = ktime_to_ms(ktime_now);
		}
	}

	/* recurse every 10 seconds */
	queue_delayed_work(system_power_efficient_wq,
		&chip->ttf_work, msecs_to_jiffies(10000));
end_work:
	vote(chip->awake_votable, TTF_PRIMING, false, 0);
	mutex_unlock(&chip->ttf.lock);
}

/* PSY CALLBACKS STAY HERE */

static int fg_psy_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *pval)
{
	struct fg_chip *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = fg_get_prop_capacity(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_RAW:
		rc = fg_get_msoc_raw(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (chip->battery_missing)
			pval->intval = 3700000;
		else
			rc = fg_get_battery_voltage(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = fg_get_battery_current(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP:
//ASUS_BSP +++
		if(fake_temp == FAKE_TEMP_INIT)
			rc = fg_get_battery_temp(chip, &pval->intval);
		else
			pval->intval = fake_temp;
//ASUS_BSP ---
		break;
	case POWER_SUPPLY_PROP_COLD_TEMP:
		rc = fg_get_jeita_threshold(chip, JEITA_COLD, &pval->intval);
		if (rc < 0) {
			pr_err("Error in reading jeita_cold, rc=%d\n", rc);
			return rc;
		}
		break;
	case POWER_SUPPLY_PROP_COOL_TEMP:
		rc = fg_get_jeita_threshold(chip, JEITA_COOL, &pval->intval);
		if (rc < 0) {
			pr_err("Error in reading jeita_cool, rc=%d\n", rc);
			return rc;
		}
		break;
	case POWER_SUPPLY_PROP_WARM_TEMP:
		rc = fg_get_jeita_threshold(chip, JEITA_WARM, &pval->intval);
		if (rc < 0) {
			pr_err("Error in reading jeita_warm, rc=%d\n", rc);
			return rc;
		}
		break;
	case POWER_SUPPLY_PROP_HOT_TEMP:
		rc = fg_get_jeita_threshold(chip, JEITA_HOT, &pval->intval);
		if (rc < 0) {
			pr_err("Error in reading jeita_hot, rc=%d\n", rc);
			return rc;
		}
		break;
	case POWER_SUPPLY_PROP_RESISTANCE:
		rc = fg_get_battery_resistance(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		rc = fg_get_sram_prop(chip, FG_SRAM_OCV, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		pval->intval = chip->cl.nom_cap_uah;
		break;
	case POWER_SUPPLY_PROP_RESISTANCE_ID:
		pval->intval = chip->batt_id_ohms;
		break;
	case POWER_SUPPLY_PROP_BATTERY_TYPE:
		pval->strval = fg_get_battery_type(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		pval->intval = chip->bp.float_volt_uv;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		pval->intval = fg_get_cycle_count(chip);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT_ID:
		pval->intval = chip->cyc_ctr.id;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW_RAW:
		rc = fg_get_charge_raw(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		pval->intval = chip->cl.init_cc_uah;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		pval->intval = chip->cl.learned_cc_uah;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		rc = fg_get_charge_counter(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW:
		rc = fg_get_charge_counter_shadow(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
		rc = fg_get_time_to_full(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		rc = fg_get_time_to_empty(chip, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_SOC_REPORTING_READY:
		pval->intval = chip->soc_reporting_ready;
		break;
	case POWER_SUPPLY_PROP_DEBUG_BATTERY:
		pval->intval = is_debug_batt_id(chip);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		rc = fg_get_sram_prop(chip, FG_SRAM_VBATT_FULL, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CC_STEP:
		if ((chip->ttf.cc_step.sel >= 0) &&
				(chip->ttf.cc_step.sel < MAX_CC_STEPS)) {
			pval->intval =
				chip->ttf.cc_step.arr[chip->ttf.cc_step.sel];
		} else {
			pr_err("cc_step_sel is out of bounds [0, %d]\n",
				chip->ttf.cc_step.sel);
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CC_STEP_SEL:
		pval->intval = chip->ttf.cc_step.sel;
		break;
	default:
		pr_err("unsupported property %d\n", psp);
		rc = -EINVAL;
		break;
	}

	if (rc < 0)
		return -ENODATA;

	return 0;
}

static int fg_psy_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *pval)
{
	struct fg_chip *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CYCLE_COUNT_ID:
		if ((pval->intval > 0) && (pval->intval <= BUCKET_COUNT)) {
			chip->cyc_ctr.id = pval->intval;
		} else {
			pr_err("rejecting invalid cycle_count_id = %d\n",
				pval->intval);
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		rc = fg_set_constant_chg_voltage(chip, pval->intval);
		break;
	case POWER_SUPPLY_PROP_RESISTANCE:
		rc = fg_force_esr_meas(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_QNOVO_ENABLE:
		rc = fg_prepare_for_qnovo(chip, pval->intval);
		break;
	case POWER_SUPPLY_PROP_CC_STEP:
		if ((chip->ttf.cc_step.sel >= 0) &&
				(chip->ttf.cc_step.sel < MAX_CC_STEPS)) {
			chip->ttf.cc_step.arr[chip->ttf.cc_step.sel] =
								pval->intval;
		} else {
			pr_err("cc_step_sel is out of bounds [0, %d]\n",
				chip->ttf.cc_step.sel);
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CC_STEP_SEL:
		if ((pval->intval >= 0) && (pval->intval < MAX_CC_STEPS)) {
			chip->ttf.cc_step.sel = pval->intval;
		} else {
			pr_err("cc_step_sel is out of bounds [0, %d]\n",
				pval->intval);
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (chip->cl.active) {
			pr_warn("Capacity learning active!\n");
			return 0;
		}
		if (pval->intval <= 0 || pval->intval > chip->cl.nom_cap_uah) {
			pr_err("charge_full is out of bounds\n");
			return -EINVAL;
		}
		chip->cl.learned_cc_uah = pval->intval;
		rc = fg_save_learned_cap_to_sram(chip);
		if (rc < 0)
			pr_err("Error in saving learned_cc_uah, rc=%d\n", rc);
		break;
	case POWER_SUPPLY_PROP_COLD_TEMP:
		rc = fg_set_jeita_threshold(chip, JEITA_COLD, pval->intval);
		if (rc < 0) {
			pr_err("Error in writing jeita_cold, rc=%d\n", rc);
			return rc;
		}
		break;
	case POWER_SUPPLY_PROP_COOL_TEMP:
		rc = fg_set_jeita_threshold(chip, JEITA_COOL, pval->intval);
		if (rc < 0) {
			pr_err("Error in writing jeita_cool, rc=%d\n", rc);
			return rc;
		}
		break;
	case POWER_SUPPLY_PROP_WARM_TEMP:
		rc = fg_set_jeita_threshold(chip, JEITA_WARM, pval->intval);
		if (rc < 0) {
			pr_err("Error in writing jeita_warm, rc=%d\n", rc);
			return rc;
		}
		break;
	case POWER_SUPPLY_PROP_HOT_TEMP:
		rc = fg_set_jeita_threshold(chip, JEITA_HOT, pval->intval);
		if (rc < 0) {
			pr_err("Error in writing jeita_hot, rc=%d\n", rc);
			return rc;
		}
		break;
	default:
		break;
	}

	return rc;
}

static int fg_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CYCLE_COUNT_ID:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_CC_STEP:
	case POWER_SUPPLY_PROP_CC_STEP_SEL:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_COLD_TEMP:
	case POWER_SUPPLY_PROP_COOL_TEMP:
	case POWER_SUPPLY_PROP_WARM_TEMP:
	case POWER_SUPPLY_PROP_HOT_TEMP:
		return 1;
	default:
		break;
	}

	return 0;
}

static void fg_external_power_changed(struct power_supply *psy)
{
	pr_debug("power supply changed\n");
}

static int fg_notifier_cb(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct fg_chip *chip = container_of(nb, struct fg_chip, nb);

	spin_lock(&chip->suspend_lock);
	if (chip->suspended) {
		/* Return if we are still suspended */
		spin_unlock(&chip->suspend_lock);
		return NOTIFY_OK;
	}
	spin_unlock(&chip->suspend_lock);

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (work_pending(&chip->status_change_work))
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "battery") == 0)
		|| (strcmp(psy->desc->name, "parallel") == 0)
		|| (strcmp(psy->desc->name, "usb") == 0)) {
		/*
		 * We cannot vote for awake votable here as that takes
		 * a mutex lock and this is executed in an atomic context.
		 */
		pm_stay_awake(chip->dev);
		schedule_work(&chip->status_change_work);
	}

	return NOTIFY_OK;
}

static enum power_supply_property fg_psy_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_RAW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_COLD_TEMP,
	POWER_SUPPLY_PROP_COOL_TEMP,
	POWER_SUPPLY_PROP_WARM_TEMP,
	POWER_SUPPLY_PROP_HOT_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_RESISTANCE_ID,
	POWER_SUPPLY_PROP_RESISTANCE,
	POWER_SUPPLY_PROP_BATTERY_TYPE,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CYCLE_COUNT_ID,
	POWER_SUPPLY_PROP_CHARGE_NOW_RAW,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_SOC_REPORTING_READY,
	POWER_SUPPLY_PROP_DEBUG_BATTERY,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CC_STEP,
	POWER_SUPPLY_PROP_CC_STEP_SEL,
};

static const struct power_supply_desc fg_psy_desc = {
	.name = "bms",
	.type = POWER_SUPPLY_TYPE_BMS,
	.properties = fg_psy_props,
	.num_properties = ARRAY_SIZE(fg_psy_props),
	.get_property = fg_psy_get_property,
	.set_property = fg_psy_set_property,
	.external_power_changed = fg_external_power_changed,
	.property_is_writeable = fg_property_is_writeable,
};

/* INIT FUNCTIONS STAY HERE */

#define DEFAULT_ESR_CHG_TIMER_RETRY	8
#define DEFAULT_ESR_CHG_TIMER_MAX	16
static int fg_hw_init(struct fg_chip *chip)
{
	int rc;
	u8 buf[4], val;

	fg_encode(chip->sp, FG_SRAM_CUTOFF_VOLT, chip->dt.cutoff_volt_mv, buf);
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_CUTOFF_VOLT].addr_word,
			chip->sp[FG_SRAM_CUTOFF_VOLT].addr_byte, buf,
			chip->sp[FG_SRAM_CUTOFF_VOLT].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing cutoff_volt, rc=%d\n", rc);
		return rc;
	}

	fg_encode(chip->sp, FG_SRAM_EMPTY_VOLT, chip->dt.empty_volt_mv, buf);
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_EMPTY_VOLT].addr_word,
			chip->sp[FG_SRAM_EMPTY_VOLT].addr_byte, buf,
			chip->sp[FG_SRAM_EMPTY_VOLT].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing empty_volt, rc=%d\n", rc);
		return rc;
	}

	fg_encode(chip->sp, FG_SRAM_CHG_TERM_CURR, chip->dt.chg_term_curr_ma,
		buf);
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_CHG_TERM_CURR].addr_word,
			chip->sp[FG_SRAM_CHG_TERM_CURR].addr_byte, buf,
			chip->sp[FG_SRAM_CHG_TERM_CURR].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing chg_term_curr, rc=%d\n", rc);
		return rc;
	}

	fg_encode(chip->sp, FG_SRAM_SYS_TERM_CURR, chip->dt.sys_term_curr_ma,
		buf);
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_SYS_TERM_CURR].addr_word,
			chip->sp[FG_SRAM_SYS_TERM_CURR].addr_byte, buf,
			chip->sp[FG_SRAM_SYS_TERM_CURR].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing sys_term_curr, rc=%d\n", rc);
		return rc;
	}

	fg_encode(chip->sp, FG_SRAM_CUTOFF_CURR, chip->dt.cutoff_curr_ma,
		buf);
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_CUTOFF_CURR].addr_word,
			chip->sp[FG_SRAM_CUTOFF_CURR].addr_byte, buf,
			chip->sp[FG_SRAM_CUTOFF_CURR].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing cutoff_curr, rc=%d\n", rc);
		return rc;
	}

	if (!(chip->wa_flags & PMI8998_V1_REV_WA)) {
		fg_encode(chip->sp, FG_SRAM_CHG_TERM_BASE_CURR,
			chip->dt.chg_term_base_curr_ma, buf);
		rc = fg_sram_write(chip,
				chip->sp[FG_SRAM_CHG_TERM_BASE_CURR].addr_word,
				chip->sp[FG_SRAM_CHG_TERM_BASE_CURR].addr_byte,
				buf, chip->sp[FG_SRAM_CHG_TERM_BASE_CURR].len,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing chg_term_base_curr, rc=%d\n",
				rc);
			return rc;
		}
	}

	if (chip->dt.vbatt_low_thr_mv > 0) {
		fg_encode(chip->sp, FG_SRAM_VBATT_LOW,
			chip->dt.vbatt_low_thr_mv, buf);
		rc = fg_sram_write(chip, chip->sp[FG_SRAM_VBATT_LOW].addr_word,
				chip->sp[FG_SRAM_VBATT_LOW].addr_byte, buf,
				chip->sp[FG_SRAM_VBATT_LOW].len,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing vbatt_low_thr, rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.delta_soc_thr > 0 && chip->dt.delta_soc_thr < 100) {
		fg_encode(chip->sp, FG_SRAM_DELTA_MSOC_THR,
			chip->dt.delta_soc_thr, buf);
		rc = fg_sram_write(chip,
				chip->sp[FG_SRAM_DELTA_MSOC_THR].addr_word,
				chip->sp[FG_SRAM_DELTA_MSOC_THR].addr_byte,
				buf, chip->sp[FG_SRAM_DELTA_MSOC_THR].len,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing delta_msoc_thr, rc=%d\n", rc);
			return rc;
		}

		fg_encode(chip->sp, FG_SRAM_DELTA_BSOC_THR,
			chip->dt.delta_soc_thr, buf);
		rc = fg_sram_write(chip,
				chip->sp[FG_SRAM_DELTA_BSOC_THR].addr_word,
				chip->sp[FG_SRAM_DELTA_BSOC_THR].addr_byte,
				buf, chip->sp[FG_SRAM_DELTA_BSOC_THR].len,
				FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing delta_bsoc_thr, rc=%d\n", rc);
			return rc;
		}
	}

	/*
	 * configure battery thermal coefficients c1,c2,c3
	 * if its value is not zero.
	 */
	if (chip->dt.batt_therm_coeffs[0] > 0) {
		rc = fg_write(chip, BATT_INFO_THERM_C1(chip),
			chip->dt.batt_therm_coeffs, BATT_THERM_NUM_COEFFS);
		if (rc < 0) {
			pr_err("Error in writing battery thermal coefficients, rc=%d\n",
				rc);
			return rc;
		}
	}


	if (chip->dt.recharge_soc_thr > 0 && chip->dt.recharge_soc_thr < 100) {
		rc = fg_set_recharge_soc(chip, chip->dt.recharge_soc_thr);
		if (rc < 0) {
			pr_err("Error in setting recharge_soc, rc=%d\n", rc);
			return rc;
		}
	}

	if (chip->dt.recharge_volt_thr_mv > 0) {
		rc = fg_set_recharge_voltage(chip,
			chip->dt.recharge_volt_thr_mv);
		if (rc < 0) {
			pr_err("Error in setting recharge_voltage, rc=%d\n",
				rc);
			return rc;
		}
	}

	if (chip->dt.rsense_sel >= SRC_SEL_BATFET &&
			chip->dt.rsense_sel < SRC_SEL_RESERVED) {
		rc = fg_masked_write(chip, BATT_INFO_IBATT_SENSING_CFG(chip),
				SOURCE_SELECT_MASK, chip->dt.rsense_sel);
		if (rc < 0) {
			pr_err("Error in writing rsense_sel, rc=%d\n", rc);
			return rc;
		}
	}

	rc = fg_set_jeita_threshold(chip, JEITA_COLD,
		chip->dt.jeita_thresholds[JEITA_COLD] * 10);
	if (rc < 0) {
		pr_err("Error in writing jeita_cold, rc=%d\n", rc);
		return rc;
	}

	rc = fg_set_jeita_threshold(chip, JEITA_COOL,
		chip->dt.jeita_thresholds[JEITA_COOL] * 10);
	if (rc < 0) {
		pr_err("Error in writing jeita_cool, rc=%d\n", rc);
		return rc;
	}

	rc = fg_set_jeita_threshold(chip, JEITA_WARM,
		chip->dt.jeita_thresholds[JEITA_WARM] * 10);
	if (rc < 0) {
		pr_err("Error in writing jeita_warm, rc=%d\n", rc);
		return rc;
	}

	rc = fg_set_jeita_threshold(chip, JEITA_HOT,
		chip->dt.jeita_thresholds[JEITA_HOT] * 10);
	if (rc < 0) {
		pr_err("Error in writing jeita_hot, rc=%d\n", rc);
		return rc;
	}

	if (chip->pmic_rev_id->pmic_subtype == PMI8998_SUBTYPE) {
		chip->esr_timer_charging_default[TIMER_RETRY] =
			DEFAULT_ESR_CHG_TIMER_RETRY;
		chip->esr_timer_charging_default[TIMER_MAX] =
			DEFAULT_ESR_CHG_TIMER_MAX;
	} else {
		/* We don't need this for pm660 at present */
		chip->esr_timer_charging_default[TIMER_RETRY] = -EINVAL;
		chip->esr_timer_charging_default[TIMER_MAX] = -EINVAL;
	}

	rc = fg_set_esr_timer(chip, chip->dt.esr_timer_charging[TIMER_RETRY],
		chip->dt.esr_timer_charging[TIMER_MAX], true, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in setting ESR timer, rc=%d\n", rc);
		return rc;
	}

	rc = fg_set_esr_timer(chip, chip->dt.esr_timer_awake[TIMER_RETRY],
		chip->dt.esr_timer_awake[TIMER_MAX], false, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in setting ESR timer, rc=%d\n", rc);
		return rc;
	}

	restore_cycle_counter(chip);

	if (chip->dt.jeita_hyst_temp >= 0) {
		val = chip->dt.jeita_hyst_temp << JEITA_TEMP_HYST_SHIFT;
		rc = fg_masked_write(chip, BATT_INFO_BATT_TEMP_CFG(chip),
			JEITA_TEMP_HYST_MASK, val);
		if (rc < 0) {
			pr_err("Error in writing batt_temp_cfg, rc=%d\n", rc);
			return rc;
		}
	}

	get_batt_temp_delta(chip->dt.batt_temp_delta, &val);
	rc = fg_masked_write(chip, BATT_INFO_BATT_TMPR_INTR(chip),
			CHANGE_THOLD_MASK, val);
	if (rc < 0) {
		pr_err("Error in writing batt_temp_delta, rc=%d\n", rc);
		return rc;
	}

	fg_encode(chip->sp, FG_SRAM_ESR_TIGHT_FILTER,
		chip->dt.esr_tight_flt_upct, buf);
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_ESR_TIGHT_FILTER].addr_word,
			chip->sp[FG_SRAM_ESR_TIGHT_FILTER].addr_byte, buf,
			chip->sp[FG_SRAM_ESR_TIGHT_FILTER].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR tight filter, rc=%d\n", rc);
		return rc;
	}

	fg_encode(chip->sp, FG_SRAM_ESR_BROAD_FILTER,
		chip->dt.esr_broad_flt_upct, buf);
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_ESR_BROAD_FILTER].addr_word,
			chip->sp[FG_SRAM_ESR_BROAD_FILTER].addr_byte, buf,
			chip->sp[FG_SRAM_ESR_BROAD_FILTER].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing ESR broad filter, rc=%d\n", rc);
		return rc;
	}

	fg_encode(chip->sp, FG_SRAM_ESR_PULSE_THRESH,
		chip->dt.esr_pulse_thresh_ma, buf);
	rc = fg_sram_write(chip, chip->sp[FG_SRAM_ESR_PULSE_THRESH].addr_word,
			chip->sp[FG_SRAM_ESR_PULSE_THRESH].addr_byte, buf,
			chip->sp[FG_SRAM_ESR_PULSE_THRESH].len, FG_IMA_DEFAULT);
	if (rc < 0) {
		pr_err("Error in writing esr_pulse_thresh_ma, rc=%d\n", rc);
		return rc;
	}

	get_esr_meas_current(chip->dt.esr_meas_curr_ma, &val);
	rc = fg_masked_write(chip, BATT_INFO_ESR_PULL_DN_CFG(chip),
			ESR_PULL_DOWN_IVAL_MASK, val);
	if (rc < 0) {
		pr_err("Error in writing esr_meas_curr_ma, rc=%d\n", rc);
		return rc;
	}

	if (is_debug_batt_id(chip)) {
		val = ESR_NO_PULL_DOWN;
		rc = fg_masked_write(chip, BATT_INFO_ESR_PULL_DN_CFG(chip),
			ESR_PULL_DOWN_MODE_MASK, val);
		if (rc < 0) {
			pr_err("Error in writing esr_pull_down, rc=%d\n", rc);
			return rc;
		}
	}

	return 0;
}

static int fg_memif_init(struct fg_chip *chip)
{
	return fg_ima_init(chip);
}

static int fg_adjust_timebase(struct fg_chip *chip)
{
	int rc = 0, die_temp;
	s32 time_base = 0;
	u8 buf[2] = {0};

	if ((chip->wa_flags & PM660_TSMC_OSC_WA) && chip->die_temp_chan) {
		rc = iio_read_channel_processed(chip->die_temp_chan, &die_temp);
		if (rc < 0) {
			pr_err("Error in reading die_temp, rc:%d\n", rc);
			return rc;
		}

		rc = fg_lerp(fg_tsmc_osc_table, ARRAY_SIZE(fg_tsmc_osc_table),
					die_temp / 1000, &time_base);
		if (rc < 0) {
			pr_err("Error to lookup fg_tsmc_osc_table rc=%d\n", rc);
			return rc;
		}

		fg_encode(chip->sp, FG_SRAM_TIMEBASE, time_base, buf);
		rc = fg_sram_write(chip,
			chip->sp[FG_SRAM_TIMEBASE].addr_word,
			chip->sp[FG_SRAM_TIMEBASE].addr_byte, buf,
			chip->sp[FG_SRAM_TIMEBASE].len, FG_IMA_DEFAULT);
		if (rc < 0) {
			pr_err("Error in writing timebase, rc=%d\n", rc);
			return rc;
		}
	}

	return 0;
}

/* INTERRUPT HANDLERS STAY HERE */

static irqreturn_t fg_mem_xcp_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;
	u8 status;
	int rc;

	rc = fg_read(chip, MEM_IF_INT_RT_STS(chip), &status, 1);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			MEM_IF_INT_RT_STS(chip), rc);
		return IRQ_HANDLED;
	}

	fg_dbg(chip, FG_IRQ, "irq %d triggered, status:%d\n", irq, status);

	mutex_lock(&chip->sram_rw_lock);
	rc = fg_clear_dma_errors_if_any(chip);
	if (rc < 0)
		pr_err("Error in clearing DMA error, rc=%d\n", rc);

	if (status & MEM_XCP_BIT) {
		rc = fg_clear_ima_errors_if_any(chip, true);
		if (rc < 0 && rc != -EAGAIN)
			pr_err("Error in checking IMA errors rc:%d\n", rc);
	}

	mutex_unlock(&chip->sram_rw_lock);
	return IRQ_HANDLED;
}

static irqreturn_t fg_vbatt_low_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;

	fg_dbg(chip, FG_IRQ, "irq %d triggered\n", irq);
	return IRQ_HANDLED;
}

static irqreturn_t fg_batt_missing_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;
	u8 status;
	int rc;

	rc = fg_read(chip, BATT_INFO_INT_RT_STS(chip), &status, 1);
	if (rc < 0) {
		pr_err("failed to read addr=0x%04x, rc=%d\n",
			BATT_INFO_INT_RT_STS(chip), rc);
		return IRQ_HANDLED;
	}

	fg_dbg(chip, FG_IRQ, "irq %d triggered sts:%d\n", irq, status);
	chip->battery_missing = (status & BT_MISS_BIT);

	if (chip->battery_missing) {
		chip->profile_available = false;
		chip->profile_loaded = false;
		chip->soc_reporting_ready = false;
		return IRQ_HANDLED;
	}

	clear_battery_profile(chip);
	queue_delayed_work(system_power_efficient_wq,
		&chip->profile_load_work, 0);

	if (chip->fg_psy)
		power_supply_changed(chip->fg_psy);

	return IRQ_HANDLED;
}

static irqreturn_t fg_delta_batt_temp_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;
	union power_supply_propval prop = {0, };
	int rc, batt_temp;

	rc = fg_get_battery_temp(chip, &batt_temp);
	if (rc < 0) {
		pr_err("Error in getting batt_temp\n");
		return IRQ_HANDLED;
	}
	fg_dbg(chip, FG_IRQ, "irq %d triggered bat_temp: %d\n", irq, batt_temp);

	rc = fg_esr_filter_config(chip, batt_temp, false);
	if (rc < 0)
		pr_err("Error in configuring ESR filter rc:%d\n", rc);

	rc = fg_slope_limit_config(chip, batt_temp);
	if (rc < 0)
		pr_err("Error in configuring slope limiter rc:%d\n", rc);

	rc = fg_adjust_ki_coeff_full_soc(chip, batt_temp);
	if (rc < 0)
		pr_err("Error in configuring ki_coeff_full_soc rc:%d\n", rc);

	if (!batt_psy_initialized(chip)) {
		chip->last_batt_temp = batt_temp;
		return IRQ_HANDLED;
	}

	power_supply_get_property(chip->batt_psy, POWER_SUPPLY_PROP_HEALTH,
		&prop);
	chip->health = prop.intval;

	if (chip->last_batt_temp != batt_temp) {
		rc = fg_adjust_timebase(chip);
		if (rc < 0)
			pr_err("Error in adjusting timebase, rc=%d\n", rc);

		rc = fg_adjust_recharge_voltage(chip);
		if (rc < 0)
			pr_err("Error in adjusting recharge_voltage, rc=%d\n",
				rc);

		chip->last_batt_temp = batt_temp;
//ASUS_BSP +++
		if(chip->delta_soc >0)
			fg_update_maint_soc(chip);
//ASUS_BSP ---			
		power_supply_changed(chip->batt_psy);
	}

	if (abs(chip->last_batt_temp - batt_temp) > 30)
		pr_warn("Battery temperature last:%d current: %d\n",
			chip->last_batt_temp, batt_temp);
	return IRQ_HANDLED;
}

static irqreturn_t fg_first_est_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;

	fg_dbg(chip, FG_IRQ, "irq %d triggered\n", irq);
	complete_all(&chip->soc_ready);
	return IRQ_HANDLED;
}

static irqreturn_t fg_soc_update_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;

	fg_dbg(chip, FG_IRQ, "irq %d triggered\n", irq);
	complete_all(&chip->soc_update);
	return IRQ_HANDLED;
}

static irqreturn_t fg_delta_bsoc_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;
	int rc;

	fg_dbg(chip, FG_IRQ, "irq %d triggered\n", irq);
	rc = fg_charge_full_update(chip);
	if (rc < 0)
		pr_err("Error in charge_full_update, rc=%d\n", rc);

	return IRQ_HANDLED;
}

static irqreturn_t fg_delta_msoc_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;
	int rc;

	fg_dbg(chip, FG_IRQ, "irq %d triggered\n", irq);
	fg_cycle_counter_update(chip);

	if (chip->cl.active)
		fg_cap_learning_update(chip);

	rc = fg_charge_full_update(chip);
	if (rc < 0)
		pr_err("Error in charge_full_update, rc=%d\n", rc);

	rc = fg_adjust_ki_coeff_dischg(chip);
	if (rc < 0)
		pr_err("Error in adjusting ki_coeff_dischg, rc=%d\n", rc);

	rc = fg_update_maint_soc(chip);
	if (rc < 0)
		pr_err("Error in updating maint_soc, rc=%d\n", rc);

	rc = fg_esr_validate(chip);
	if (rc < 0)
		pr_err("Error in validating ESR, rc=%d\n", rc);

	rc = fg_adjust_timebase(chip);
	if (rc < 0)
		pr_err("Error in adjusting timebase, rc=%d\n", rc);

	if (batt_psy_initialized(chip))
		power_supply_changed(chip->batt_psy);

	return IRQ_HANDLED;
}

static irqreturn_t fg_empty_soc_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;

	fg_dbg(chip, FG_IRQ, "irq %d triggered\n", irq);
	if (batt_psy_initialized(chip))
		power_supply_changed(chip->batt_psy);

	return IRQ_HANDLED;
}

static irqreturn_t fg_soc_irq_handler(int irq, void *data)
{
	struct fg_chip *chip = data;

	fg_dbg(chip, FG_IRQ, "irq %d triggered\n", irq);
	return IRQ_HANDLED;
}

static irqreturn_t fg_dummy_irq_handler(int irq, void *data)
{
	pr_debug("irq %d triggered\n", irq);
	return IRQ_HANDLED;
}

static struct fg_irq_info fg_irqs[FG_IRQ_MAX] = {
	/* BATT_SOC irqs */
	[MSOC_FULL_IRQ] = {
		.name		= "msoc-full",
		.handler	= fg_soc_irq_handler,
	},
	[MSOC_HIGH_IRQ] = {
		.name		= "msoc-high",
		.handler	= fg_soc_irq_handler,
		.wakeable	= true,
	},
	[MSOC_EMPTY_IRQ] = {
		.name		= "msoc-empty",
		.handler	= fg_empty_soc_irq_handler,
		.wakeable	= true,
	},
	[MSOC_LOW_IRQ] = {
		.name		= "msoc-low",
		.handler	= fg_soc_irq_handler,
		.wakeable	= true,
	},
	[MSOC_DELTA_IRQ] = {
		.name		= "msoc-delta",
		.handler	= fg_delta_msoc_irq_handler,
		.wakeable	= true,
	},
	[BSOC_DELTA_IRQ] = {
		.name		= "bsoc-delta",
		.handler	= fg_delta_bsoc_irq_handler,
		.wakeable	= true,
	},
	[SOC_READY_IRQ] = {
		.name		= "soc-ready",
		.handler	= fg_first_est_irq_handler,
		.wakeable	= true,
	},
	[SOC_UPDATE_IRQ] = {
		.name		= "soc-update",
		.handler	= fg_soc_update_irq_handler,
	},
	/* BATT_INFO irqs */
	[BATT_TEMP_DELTA_IRQ] = {
		.name		= "batt-temp-delta",
		.handler	= fg_delta_batt_temp_irq_handler,
		.wakeable	= true,
	},
	[BATT_MISSING_IRQ] = {
		.name		= "batt-missing",
		.handler	= fg_batt_missing_irq_handler,
		.wakeable	= true,
	},
	[ESR_DELTA_IRQ] = {
		.name		= "esr-delta",
		.handler	= fg_dummy_irq_handler,
	},
	[VBATT_LOW_IRQ] = {
		.name		= "vbatt-low",
		.handler	= fg_vbatt_low_irq_handler,
		.wakeable	= true,
	},
	[VBATT_PRED_DELTA_IRQ] = {
		.name		= "vbatt-pred-delta",
		.handler	= fg_dummy_irq_handler,
	},
	/* MEM_IF irqs */
	[DMA_GRANT_IRQ] = {
		.name		= "dma-grant",
		.handler	= fg_dummy_irq_handler,
	},
	[MEM_XCP_IRQ] = {
		.name		= "mem-xcp",
		.handler	= fg_mem_xcp_irq_handler,
	},
	[IMA_RDY_IRQ] = {
		.name		= "ima-rdy",
		.handler	= fg_dummy_irq_handler,
	},
};

static int fg_get_irq_index_byname(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fg_irqs); i++) {
		if (strcmp(fg_irqs[i].name, name) == 0)
			return i;
	}

	pr_err("%s is not in irq list\n", name);
	return -ENOENT;
}

static int fg_register_interrupts(struct fg_chip *chip)
{
	struct device_node *child, *node = chip->dev->of_node;
	struct property *prop;
	const char *name;
	int rc, irq, irq_index;

	for_each_available_child_of_node(node, child) {
		of_property_for_each_string(child, "interrupt-names", prop,
						name) {
			irq = of_irq_get_byname(child, name);
			if (irq < 0) {
				dev_err(chip->dev, "failed to get irq %s irq:%d\n",
					name, irq);
				return irq;
			}

			irq_index = fg_get_irq_index_byname(name);
			if (irq_index < 0)
				return irq_index;

			rc = devm_request_threaded_irq(chip->dev, irq, NULL,
					fg_irqs[irq_index].handler,
					IRQF_ONESHOT, name, chip);
			if (rc < 0) {
				dev_err(chip->dev, "failed to register irq handler for %s rc:%d\n",
					name, rc);
				return rc;
			}

			fg_irqs[irq_index].irq = irq;
			if (fg_irqs[irq_index].wakeable)
				enable_irq_wake(fg_irqs[irq_index].irq);
		}
	}

	return 0;
}

static int fg_parse_dt_property_u32_array(struct device_node *node,
				const char *prop_name, int *buf, int len)
{
	int rc;

	rc = of_property_count_elems_of_size(node, prop_name, sizeof(u32));
	if (rc < 0) {
		if (rc == -EINVAL)
			return 0;
		else
			return rc;
	} else if (rc != len) {
		pr_err("Incorrect length %d for %s, rc=%d\n", len, prop_name,
			rc);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(node, prop_name, buf, len);
	if (rc < 0) {
		pr_err("Error in reading %s, rc=%d\n", prop_name, rc);
		return rc;
	}

	return 0;
}

static int fg_parse_slope_limit_coefficients(struct fg_chip *chip)
{
	struct device_node *node = chip->dev->of_node;
	int rc, i;

	rc = of_property_read_u32(node, "qcom,slope-limit-temp-threshold",
			&chip->dt.slope_limit_temp);
	if (rc < 0)
		return 0;

	rc = fg_parse_dt_property_u32_array(node, "qcom,slope-limit-coeffs",
		chip->dt.slope_limit_coeffs, SLOPE_LIMIT_NUM_COEFFS);
	if (rc < 0)
		return rc;

	for (i = 0; i < SLOPE_LIMIT_NUM_COEFFS; i++) {
		if (chip->dt.slope_limit_coeffs[i] > SLOPE_LIMIT_COEFF_MAX ||
			chip->dt.slope_limit_coeffs[i] < 0) {
			pr_err("Incorrect slope limit coefficient\n");
			return -EINVAL;
		}
	}

	chip->slope_limit_en = true;
	return 0;
}

static int fg_parse_ki_coefficients(struct fg_chip *chip)
{
	struct device_node *node = chip->dev->of_node;
	int rc, i, temp;

	rc = of_property_read_u32(node, "qcom,ki-coeff-full-dischg", &temp);
	if (!rc)
		chip->dt.ki_coeff_full_soc_dischg = temp;

	rc = fg_parse_dt_property_u32_array(node, "qcom,ki-coeff-soc-dischg",
		chip->dt.ki_coeff_soc, KI_COEFF_SOC_LEVELS);
	if (rc < 0)
		return rc;

	rc = fg_parse_dt_property_u32_array(node, "qcom,ki-coeff-med-dischg",
		chip->dt.ki_coeff_med_dischg, KI_COEFF_SOC_LEVELS);
	if (rc < 0)
		return rc;

	rc = fg_parse_dt_property_u32_array(node, "qcom,ki-coeff-hi-dischg",
		chip->dt.ki_coeff_hi_dischg, KI_COEFF_SOC_LEVELS);
	if (rc < 0)
		return rc;

	for (i = 0; i < KI_COEFF_SOC_LEVELS; i++) {
		if (chip->dt.ki_coeff_soc[i] < 0 ||
			chip->dt.ki_coeff_soc[i] > FULL_CAPACITY) {
			pr_err("Error in ki_coeff_soc_dischg values\n");
			return -EINVAL;
		}

		if (chip->dt.ki_coeff_med_dischg[i] < 0 ||
			chip->dt.ki_coeff_med_dischg[i] > KI_COEFF_MAX) {
			pr_err("Error in ki_coeff_med_dischg values\n");
			return -EINVAL;
		}

		if (chip->dt.ki_coeff_med_dischg[i] < 0 ||
			chip->dt.ki_coeff_med_dischg[i] > KI_COEFF_MAX) {
			pr_err("Error in ki_coeff_med_dischg values\n");
			return -EINVAL;
		}
	}
	chip->ki_coeff_dischg_en = true;
	return 0;
}

#define DEFAULT_CUTOFF_VOLT_MV		3200
#define DEFAULT_EMPTY_VOLT_MV		2800 //ASUS_BSP
#define DEFAULT_RECHARGE_VOLT_MV	4250
#define DEFAULT_CHG_TERM_CURR_MA	100
#define DEFAULT_CHG_TERM_BASE_CURR_MA	75
#define DEFAULT_SYS_TERM_CURR_MA	-125
#define DEFAULT_CUTOFF_CURR_MA		500
#define DEFAULT_DELTA_SOC_THR		1
#define DEFAULT_RECHARGE_SOC_THR	98 //ASUS_BSP LiJen
#define DEFAULT_BATT_TEMP_COLD		0
#define DEFAULT_BATT_TEMP_COOL		5
#define DEFAULT_BATT_TEMP_WARM		45
#define DEFAULT_BATT_TEMP_HOT		50
#define DEFAULT_CL_START_SOC		15
#define DEFAULT_CL_MIN_TEMP_DECIDEGC	150
#define DEFAULT_CL_MAX_TEMP_DECIDEGC	450
#define DEFAULT_CL_MAX_INC_DECIPERC	5
#define DEFAULT_CL_MAX_DEC_DECIPERC	100
#define DEFAULT_CL_MIN_LIM_DECIPERC	0
#define DEFAULT_CL_MAX_LIM_DECIPERC	0
#define BTEMP_DELTA_LOW			2
#define BTEMP_DELTA_HIGH		10
#define DEFAULT_ESR_FLT_TEMP_DECIDEGC	100
#define DEFAULT_ESR_TIGHT_FLT_UPCT	3907
#define DEFAULT_ESR_BROAD_FLT_UPCT	99610
#define DEFAULT_ESR_TIGHT_LT_FLT_UPCT	30000
#define DEFAULT_ESR_BROAD_LT_FLT_UPCT	30000
#define DEFAULT_ESR_FLT_RT_DECIDEGC	60
#define DEFAULT_ESR_TIGHT_RT_FLT_UPCT	5860
#define DEFAULT_ESR_BROAD_RT_FLT_UPCT	156250
#define DEFAULT_ESR_CLAMP_MOHMS		20
#define DEFAULT_ESR_PULSE_THRESH_MA	110
#define DEFAULT_ESR_MEAS_CURR_MA	120
static int fg_parse_dt(struct fg_chip *chip)
{
	struct device_node *child, *revid_node, *node = chip->dev->of_node;
	u32 base, temp;
	u8 subtype;
	int rc;

	if (!node)  {
		dev_err(chip->dev, "device tree node missing\n");
		return -ENXIO;
	}

	revid_node = of_parse_phandle(node, "qcom,pmic-revid", 0);
	if (!revid_node) {
		pr_err("Missing qcom,pmic-revid property - driver failed\n");
		return -EINVAL;
	}

	chip->pmic_rev_id = get_revid_data(revid_node);
	if (IS_ERR_OR_NULL(chip->pmic_rev_id)) {
		pr_err("Unable to get pmic_revid rc=%ld\n",
			PTR_ERR(chip->pmic_rev_id));
		/*
		 * the revid peripheral must be registered, any failure
		 * here only indicates that the rev-id module has not
		 * probed yet.
		 */
		return -EPROBE_DEFER;
	}

	pr_debug("PMIC subtype %d Digital major %d\n",
		chip->pmic_rev_id->pmic_subtype, chip->pmic_rev_id->rev4);

	switch (chip->pmic_rev_id->pmic_subtype) {
	case PMI8998_SUBTYPE:
		if (chip->pmic_rev_id->rev4 < PMI8998_V2P0_REV4) {
			chip->sp = pmi8998_v1_sram_params;
			chip->alg_flags = pmi8998_v1_alg_flags;
			chip->wa_flags |= PMI8998_V1_REV_WA;
		} else if (chip->pmic_rev_id->rev4 == PMI8998_V2P0_REV4) {
			chip->sp = pmi8998_v2_sram_params;
			chip->alg_flags = pmi8998_v2_alg_flags;
		} else {
			return -EINVAL;
		}
		break;
	case PM660_SUBTYPE:
		chip->sp = pmi8998_v2_sram_params;
		chip->alg_flags = pmi8998_v2_alg_flags;
		chip->use_ima_single_mode = true;
		if (chip->pmic_rev_id->fab_id == PM660_FAB_ID_TSMC)
			chip->wa_flags |= PM660_TSMC_OSC_WA;
		break;
	default:
		return -EINVAL;
	}

	if (of_get_available_child_count(node) == 0) {
		dev_err(chip->dev, "No child nodes specified!\n");
		return -ENXIO;
	}

	for_each_available_child_of_node(node, child) {
		rc = of_property_read_u32(child, "reg", &base);
		if (rc < 0) {
			dev_err(chip->dev, "reg not specified in node %s, rc=%d\n",
				child->full_name, rc);
			return rc;
		}

		rc = fg_read(chip, base + PERPH_SUBTYPE_REG, &subtype, 1);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't read subtype for base %d, rc=%d\n",
				base, rc);
			return rc;
		}

		switch (subtype) {
		case FG_BATT_SOC_PMI8998:
			chip->batt_soc_base = base;
			break;
		case FG_BATT_INFO_PMI8998:
			chip->batt_info_base = base;
			break;
		case FG_MEM_INFO_PMI8998:
			chip->mem_if_base = base;
			break;
		default:
			dev_err(chip->dev, "Invalid peripheral subtype 0x%x\n",
				subtype);
			return -ENXIO;
		}
	}

	rc = of_property_read_u32(node, "qcom,rradc-base", &base);
	if (rc < 0) {
		dev_err(chip->dev, "rradc-base not specified, rc=%d\n", rc);
		return rc;
	}
	chip->rradc_base = base;

	/* Read all the optional properties below */
	rc = of_property_read_u32(node, "qcom,fg-cutoff-voltage", &temp);
	if (rc < 0)
		chip->dt.cutoff_volt_mv = DEFAULT_CUTOFF_VOLT_MV;
	else
		chip->dt.cutoff_volt_mv = temp;

	rc = of_property_read_u32(node, "qcom,fg-empty-voltage", &temp);
	if (rc < 0)
		chip->dt.empty_volt_mv = DEFAULT_EMPTY_VOLT_MV;
	else
		chip->dt.empty_volt_mv = temp;

	rc = of_property_read_u32(node, "qcom,fg-vbatt-low-thr", &temp);
	if (rc < 0)
		chip->dt.vbatt_low_thr_mv = -EINVAL;
	else
		chip->dt.vbatt_low_thr_mv = temp;

	rc = of_property_read_u32(node, "qcom,fg-chg-term-current", &temp);
	if (rc < 0)
		chip->dt.chg_term_curr_ma = DEFAULT_CHG_TERM_CURR_MA;
	else
		chip->dt.chg_term_curr_ma = temp;

	rc = of_property_read_u32(node, "qcom,fg-sys-term-current", &temp);
	if (rc < 0)
		chip->dt.sys_term_curr_ma = DEFAULT_SYS_TERM_CURR_MA;
	else
		chip->dt.sys_term_curr_ma = temp;

	rc = of_property_read_u32(node, "qcom,fg-chg-term-base-current", &temp);
	if (rc < 0)
		chip->dt.chg_term_base_curr_ma = DEFAULT_CHG_TERM_BASE_CURR_MA;
	else
		chip->dt.chg_term_base_curr_ma = temp;

	rc = of_property_read_u32(node, "qcom,fg-cutoff-current", &temp);
	if (rc < 0)
		chip->dt.cutoff_curr_ma = DEFAULT_CUTOFF_CURR_MA;
	else
		chip->dt.cutoff_curr_ma = temp;

	rc = of_property_read_u32(node, "qcom,fg-delta-soc-thr", &temp);
	if (rc < 0)
		chip->dt.delta_soc_thr = DEFAULT_DELTA_SOC_THR;
	else
		chip->dt.delta_soc_thr = temp;

	rc = of_property_read_u32(node, "qcom,fg-recharge-soc-thr", &temp);
	if (rc < 0)
		chip->dt.recharge_soc_thr = DEFAULT_RECHARGE_SOC_THR;
	else
		chip->dt.recharge_soc_thr = temp;

	rc = of_property_read_u32(node, "qcom,fg-recharge-voltage", &temp);
	if (rc < 0)
		chip->dt.recharge_volt_thr_mv = DEFAULT_RECHARGE_VOLT_MV;
	else
		chip->dt.recharge_volt_thr_mv = temp;

	BAT_DBG("recharge_volt_thr_mv %d\n",chip->dt.recharge_volt_thr_mv); //ASUS_BSP

	chip->dt.auto_recharge_soc = of_property_read_bool(node,
					"qcom,fg-auto-recharge-soc");

	rc = of_property_read_u32(node, "qcom,fg-rsense-sel", &temp);
	if (rc < 0)
		chip->dt.rsense_sel = SRC_SEL_BATFET_SMB;
	else
		chip->dt.rsense_sel = (u8)temp & SOURCE_SELECT_MASK;

	chip->dt.jeita_thresholds[JEITA_COLD] = DEFAULT_BATT_TEMP_COLD;
	chip->dt.jeita_thresholds[JEITA_COOL] = DEFAULT_BATT_TEMP_COOL;
	chip->dt.jeita_thresholds[JEITA_WARM] = DEFAULT_BATT_TEMP_WARM;
	chip->dt.jeita_thresholds[JEITA_HOT] = DEFAULT_BATT_TEMP_HOT;
	if (of_property_count_elems_of_size(node, "qcom,fg-jeita-thresholds",
		sizeof(u32)) == NUM_JEITA_LEVELS) {
		rc = of_property_read_u32_array(node,
				"qcom,fg-jeita-thresholds",
				chip->dt.jeita_thresholds, NUM_JEITA_LEVELS);
		if (rc < 0)
			pr_warn("Error reading Jeita thresholds, default values will be used rc:%d\n",
				rc);
	}

	BAT_DBG("HW jeita warm:%d hot:%d\n", chip->dt.jeita_thresholds[JEITA_WARM],chip->dt.jeita_thresholds[JEITA_HOT]); //ASUS_BSP
	if (of_property_count_elems_of_size(node,
		"qcom,battery-thermal-coefficients",
		sizeof(u8)) == BATT_THERM_NUM_COEFFS) {
		rc = of_property_read_u8_array(node,
				"qcom,battery-thermal-coefficients",
				chip->dt.batt_therm_coeffs,
				BATT_THERM_NUM_COEFFS);
		if (rc < 0)
			pr_warn("Error reading battery thermal coefficients, rc:%d\n",
				rc);
	}

	rc = fg_parse_dt_property_u32_array(node, "qcom,fg-esr-timer-charging",
		chip->dt.esr_timer_charging, NUM_ESR_TIMERS);
	if (rc < 0) {
		chip->dt.esr_timer_charging[TIMER_RETRY] = -EINVAL;
		chip->dt.esr_timer_charging[TIMER_MAX] = -EINVAL;
	}

	rc = fg_parse_dt_property_u32_array(node, "qcom,fg-esr-timer-awake",
		chip->dt.esr_timer_awake, NUM_ESR_TIMERS);
	if (rc < 0) {
		chip->dt.esr_timer_awake[TIMER_RETRY] = -EINVAL;
		chip->dt.esr_timer_awake[TIMER_MAX] = -EINVAL;
	}

	rc = fg_parse_dt_property_u32_array(node, "qcom,fg-esr-timer-asleep",
		chip->dt.esr_timer_asleep, NUM_ESR_TIMERS);
	if (rc < 0) {
		chip->dt.esr_timer_asleep[TIMER_RETRY] = -EINVAL;
		chip->dt.esr_timer_asleep[TIMER_MAX] = -EINVAL;
	}

	chip->cyc_ctr.en = of_property_read_bool(node, "qcom,cycle-counter-en");
	if (chip->cyc_ctr.en)
		chip->cyc_ctr.id = 1;

	chip->dt.force_load_profile = of_property_read_bool(node,
					"qcom,fg-force-load-profile");

	BAT_DBG("force_load_profile: %d\n",chip->dt.force_load_profile); //ASUS_BSP
	rc = of_property_read_u32(node, "qcom,cl-start-capacity", &temp);
	if (rc < 0)
		chip->dt.cl_start_soc = DEFAULT_CL_START_SOC;
	else
		chip->dt.cl_start_soc = temp;

	rc = of_property_read_u32(node, "qcom,cl-min-temp", &temp);
	if (rc < 0)
		chip->dt.cl_min_temp = DEFAULT_CL_MIN_TEMP_DECIDEGC;
	else
		chip->dt.cl_min_temp = temp;

	rc = of_property_read_u32(node, "qcom,cl-max-temp", &temp);
	if (rc < 0)
		chip->dt.cl_max_temp = DEFAULT_CL_MAX_TEMP_DECIDEGC;
	else
		chip->dt.cl_max_temp = temp;

	rc = of_property_read_u32(node, "qcom,cl-max-increment", &temp);
	if (rc < 0)
		chip->dt.cl_max_cap_inc = DEFAULT_CL_MAX_INC_DECIPERC;
	else
		chip->dt.cl_max_cap_inc = temp;

	rc = of_property_read_u32(node, "qcom,cl-max-decrement", &temp);
	if (rc < 0)
		chip->dt.cl_max_cap_dec = DEFAULT_CL_MAX_DEC_DECIPERC;
	else
		chip->dt.cl_max_cap_dec = temp;

	rc = of_property_read_u32(node, "qcom,cl-min-limit", &temp);
	if (rc < 0)
		chip->dt.cl_min_cap_limit = DEFAULT_CL_MIN_LIM_DECIPERC;
	else
		chip->dt.cl_min_cap_limit = temp;

	rc = of_property_read_u32(node, "qcom,cl-max-limit", &temp);
	if (rc < 0)
		chip->dt.cl_max_cap_limit = DEFAULT_CL_MAX_LIM_DECIPERC;
	else
		chip->dt.cl_max_cap_limit = temp;

	rc = of_property_read_u32(node, "qcom,fg-jeita-hyst-temp", &temp);
	if (rc < 0)
		chip->dt.jeita_hyst_temp = -EINVAL;
	else
		chip->dt.jeita_hyst_temp = temp;

	rc = of_property_read_u32(node, "qcom,fg-batt-temp-delta", &temp);
	if (rc < 0)
		chip->dt.batt_temp_delta = -EINVAL;
	else if (temp > BTEMP_DELTA_LOW && temp <= BTEMP_DELTA_HIGH)
		chip->dt.batt_temp_delta = temp;

	chip->dt.hold_soc_while_full = of_property_read_bool(node,
					"qcom,hold-soc-while-full");

	chip->dt.linearize_soc = of_property_read_bool(node,
					"qcom,linearize-soc");

	rc = fg_parse_ki_coefficients(chip);
	if (rc < 0)
		pr_err("Error in parsing Ki coefficients, rc=%d\n", rc);

	rc = of_property_read_u32(node, "qcom,fg-rconn-mohms", &temp);
	if (!rc)
		chip->dt.rconn_mohms = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-filter-switch-temp",
			&temp);
	if (rc < 0)
		chip->dt.esr_flt_switch_temp = DEFAULT_ESR_FLT_TEMP_DECIDEGC;
	else
		chip->dt.esr_flt_switch_temp = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-tight-filter-micro-pct",
			&temp);
	if (rc < 0)
		chip->dt.esr_tight_flt_upct = DEFAULT_ESR_TIGHT_FLT_UPCT;
	else
		chip->dt.esr_tight_flt_upct = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-broad-filter-micro-pct",
			&temp);
	if (rc < 0)
		chip->dt.esr_broad_flt_upct = DEFAULT_ESR_BROAD_FLT_UPCT;
	else
		chip->dt.esr_broad_flt_upct = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-tight-lt-filter-micro-pct",
			&temp);
	if (rc < 0)
		chip->dt.esr_tight_lt_flt_upct = DEFAULT_ESR_TIGHT_LT_FLT_UPCT;
	else
		chip->dt.esr_tight_lt_flt_upct = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-broad-lt-filter-micro-pct",
			&temp);
	if (rc < 0)
		chip->dt.esr_broad_lt_flt_upct = DEFAULT_ESR_BROAD_LT_FLT_UPCT;
	else
		chip->dt.esr_broad_lt_flt_upct = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-rt-filter-switch-temp",
			&temp);
	if (rc < 0)
		chip->dt.esr_flt_rt_switch_temp = DEFAULT_ESR_FLT_RT_DECIDEGC;
	else
		chip->dt.esr_flt_rt_switch_temp = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-tight-rt-filter-micro-pct",
			&temp);
	if (rc < 0)
		chip->dt.esr_tight_rt_flt_upct = DEFAULT_ESR_TIGHT_RT_FLT_UPCT;
	else
		chip->dt.esr_tight_rt_flt_upct = temp;

	rc = of_property_read_u32(node, "qcom,fg-esr-broad-rt-filter-micro-pct",
			&temp);
	if (rc < 0)
		chip->dt.esr_broad_rt_flt_upct = DEFAULT_ESR_BROAD_RT_FLT_UPCT;
	else
		chip->dt.esr_broad_rt_flt_upct = temp;

	rc = fg_parse_slope_limit_coefficients(chip);
	if (rc < 0)
		pr_err("Error in parsing slope limit coeffs, rc=%d\n", rc);

	rc = of_property_read_u32(node, "qcom,fg-esr-clamp-mohms", &temp);
	if (rc < 0)
		chip->dt.esr_clamp_mohms = DEFAULT_ESR_CLAMP_MOHMS;
	else
		chip->dt.esr_clamp_mohms = temp;

	chip->dt.esr_pulse_thresh_ma = DEFAULT_ESR_PULSE_THRESH_MA;
	rc = of_property_read_u32(node, "qcom,fg-esr-pulse-thresh-ma", &temp);
	if (!rc) {
		/* ESR pulse qualification threshold range is 1-997 mA */
		if (temp > 0 && temp < 997)
			chip->dt.esr_pulse_thresh_ma = temp;
	}

	chip->dt.esr_meas_curr_ma = DEFAULT_ESR_MEAS_CURR_MA;
	rc = of_property_read_u32(node, "qcom,fg-esr-meas-curr-ma", &temp);
	if (!rc) {
		/* ESR measurement current range is 60-240 mA */
		if (temp >= 60 || temp <= 240)
			chip->dt.esr_meas_curr_ma = temp;
	}

	return 0;
}

static void fg_cleanup(struct fg_chip *chip)
{
	alarm_try_to_cancel(&chip->esr_filter_alarm);
	power_supply_unreg_notifier(&chip->nb);
	debugfs_remove_recursive(chip->dfs_root);
	if (chip->awake_votable)
		destroy_votable(chip->awake_votable);

	if (chip->delta_bsoc_irq_en_votable)
		destroy_votable(chip->delta_bsoc_irq_en_votable);

	if (chip->batt_miss_irq_en_votable)
		destroy_votable(chip->batt_miss_irq_en_votable);

	if (chip->batt_id_chan)
		iio_channel_release(chip->batt_id_chan);

	dev_set_drvdata(chip->dev, NULL);
}

//ASUS_BSP +++
static int batt_type_proc_read(struct seq_file *buf, void *v)
{
	int result = 0;
	char model[32] = "";

	result = g_fgChip->batt_id_ohms;
	if(!result){
		snprintf(model, sizeof(model), "%s", BATT_UNKNOWN);
		goto end;
	}	
	
	if (g_ASUS_hwID <= ZE620KL_SR) {
		if (g_fgChip->profile_available  && !strcmp(g_fgChip->bp.batt_type_str, BATT_TYPE_CSL))
			snprintf(model, sizeof(model), "%s", BATT_TYPE_51K);
		else
			snprintf(model, sizeof(model), "%s", BATT_UNKNOWN);

	} else {
		if (g_fgChip->profile_available  && !strcmp(g_fgChip->bp.batt_type_str, BATT_TYPE_CSL_ER))
			snprintf(model, sizeof(model), "%s", BATT_TYPE_51K_ER);
		else
			snprintf(model, sizeof(model), "%s", BATT_UNKNOWN);
	}
	
end:
	seq_printf(buf,"%s\n",model);
	BAT_DBG("%s: %dohms, %s\n", __func__, result, model);
	
	return 0;
}
static int batt_type_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, batt_type_proc_read, NULL);
}

static void create_batt_type_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  batt_type_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/batt_type", 0444, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}



static int battID_status_proc_read(struct seq_file *buf, void *v)
{
	int result = 0;

	result= g_fgChip->batt_id_ohms;
	
	if(!result)
		seq_printf(buf, "FAIL\n");
	else
		seq_printf(buf, "PASS\n");

	BAT_DBG("%s: %d\n", __func__, result);
	
	return 0;
}
static int battID_status_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, battID_status_proc_read, NULL);
}

static void create_battID_status_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  battID_status_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/battID_status", 0444, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}


/*+++ proc batt_current Interface+++*/
static int batt_mili_temp_proc_read(struct seq_file *buf, void *v)
{
	int result = 0;

	fg_get_battery_temp(g_fgChip, &result);
	//scale 0.1 to 0.001 
	result *=100;
	BAT_DBG("%s: %d\n", __func__, result);
	seq_printf(buf, "%d\n", result);
	return 0;
}
static int batt_mili_temp_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, batt_mili_temp_proc_read, NULL);
}

static void create_batt_mili_temp_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  batt_mili_temp_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/batt_miliTemp", 0444, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}
/*---proc batt_current Interface---*/


static ssize_t thermal_test_proc_write(struct file *filp, const char __user *buff, size_t len, loff_t *data){
	int val;
	char messages[8]="";

	
	len =(len > 8 ?8:len);
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);

	if(val < -200 ||val > 700)
		fake_temp = FAKE_TEMP_INIT;
	else 
		fake_temp = val;
	
	BAT_DBG("%s: set fake temperature as %d\n",__func__,fake_temp);

	return len;
}


static int thermal_proc_read(struct seq_file *buf, void *v)
{
	int result = fake_temp;
	
	BAT_DBG("%s: %d\n", __func__, result);
	seq_printf(buf, "%d\n", result);
	return 0;
}
static int thermal_test_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, thermal_proc_read, NULL);
}

static void create_thermal_test_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  thermal_test_proc_open,
		.write = thermal_test_proc_write,
		.read = seq_read,
		.llseek = seq_lseek,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/ThermalTemp", 0666, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}

/*+++ proc batt_current Interface+++*/
static int batt_current_proc_read(struct seq_file *buf, void *v)
{
	int result = 0;

	fg_get_battery_current(g_fgChip, &result);
	BAT_DBG("%s: %d\n", __func__, result);
	seq_printf(buf, "%d\n", result);
	return 0;
}
static int batt_current_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, batt_current_proc_read, NULL);
}

static void create_batt_current_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  batt_current_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/batt_current", 0444, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}
/*---proc batt_current Interface---*/
/*+++proc batt_voltage Interface+++*/
static int batt_voltage_proc_read(struct seq_file *buf, void *v)
{
	int result = 0;
	fg_get_battery_voltage(g_fgChip, &result);
	BAT_DBG("%s: %d\n", __func__, result);
	seq_printf(buf, "%d\n", result);
	return 0;
}
static int batt_voltage_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, batt_voltage_proc_read, NULL);
}

static void create_batt_voltage_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  batt_voltage_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/batt_voltage", 0444, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}
/*---proc batt_voltage Interface---*/
/*+++ proc gaugeIC_status Interface+++*/
static int gaugeIC_status_proc_read(struct seq_file *buf, void *v)
{
	int result = 0,val=0;
	if (fg_get_battery_current(g_fgChip, &val) == 0) {
		result = 1;
	}
	
	BAT_DBG("%s: %d\n", __func__, result);
	seq_printf(buf, "%d\n", result);
	return 0;
}
static int gaugeIC_status_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, gaugeIC_status_proc_read, NULL);
}

static void create_gaugeIC_status_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  gaugeIC_status_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/gaugeIC_status", 0444, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}
/*--- proc gaugeIC_status Interface---*/


//ASUS_BSP LiJen config PMIC GPIO2 to ADC channel +++
static int32_t get_pmgpio2_vadc_voltage(int channel){
	struct qpnp_vadc_chip *vadc_dev;
	struct qpnp_vadc_result adc_result;
	int32_t adc;
	
	vadc_dev = qpnp_get_vadc(g_fgChip->dev, "pm-gpio2");
	if (IS_ERR(vadc_dev)) {
		BAT_DBG("%s: qpnp_get_vadc failed\n", __func__);
		return -1;
	}else{
		qpnp_vadc_read(vadc_dev, channel, &adc_result); //Read the GPIO2 VADC channel with 1:1 scaling
		adc = (int) adc_result.physical;
		//adc = adc / 1000; /* uV to mV */
		BAT_DBG("%s: adc=%d\n", __func__, adc);
	}
	
	return adc;
}

static int pmic_gpio2_adc_wp_proc_file_proc_read(struct seq_file *buf, void *v)
{
	int32_t adc;
	
	adc = get_pmgpio2_vadc_voltage(VADC_AMUX1_GPIO_PU3);
	
	BAT_DBG("%s: %d\n", __func__, adc);
	seq_printf(buf, "%d\n", adc);
	return 0;
}
static int pmic_gpio2_adc_wp_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, pmic_gpio2_adc_wp_proc_file_proc_read, NULL);
}

static void create_pmic_gpio2_adc_wp_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  pmic_gpio2_adc_wp_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/pmic_gpio2_adc_wp", 0444, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}

static int wp_state_proc_file_proc_read(struct seq_file *buf, void *v)
{
	
	BAT_DBG("%s: %d\n", __func__, g_wp_state);
	seq_printf(buf, "%d\n", g_wp_state);
	return 0;
}
static int wp_state_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, wp_state_proc_file_proc_read, NULL);
}

static void create_wp_state_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  wp_state_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/wp_state", 0444, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}

static int pmic_gpio2_adc_tds_proc_file_proc_read(struct seq_file *buf, void *v)
{
	int32_t adc;
	
	adc = get_pmgpio2_vadc_voltage(VADC_AMUX1_GPIO_PU1);
	
	BAT_DBG("%s: %d\n", __func__, adc);
	seq_printf(buf, "%d\n", adc);
	return 0;
}
static int pmic_gpio2_adc_tds_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, pmic_gpio2_adc_tds_proc_file_proc_read, NULL);
}

static void create_pmic_gpio2_adc_tds_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  pmic_gpio2_adc_tds_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/pmic_gpio2_adc_tds", 0444, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}
//ASUS_BSP LiJen config PMIC GPIO2 to ADC channel ---

//ASUS_BSP battery safety upgrade +++
static int batt_safety_proc_show(struct seq_file *buf, void *data)
{
	int rc =0;

	rc = file_op(CYCLE_COUNT_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
	(char *)&g_cycle_count_data, sizeof(struct CYCLE_COUNT_DATA), FILE_OP_WRITE);
	if(rc < 0 )
		BAT_DBG_E("%s: write cycle count file error\n",__FUNCTION__);
	
	seq_printf(buf, "---show battery safety value---\n");
	seq_printf(buf, "cycle_count:%d\n", g_cycle_count_data.cycle_count);
	seq_printf(buf, "battery_total_time:%lu\n", g_cycle_count_data.battery_total_time);
	seq_printf(buf, "high_temp_total_time:%lu\n", g_cycle_count_data.high_temp_total_time);
	seq_printf(buf, "high_vol_total_time:%lu\n", g_cycle_count_data.high_vol_total_time);
	seq_printf(buf, "high_temp_vol_time:%lu\n", g_cycle_count_data.high_temp_vol_time);
	seq_printf(buf, "reload_condition:%d\n", g_cycle_count_data.reload_condition);

	return 0;
}
static int batt_safety_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, batt_safety_proc_show, NULL);
}

static void asus_judge_reload_condition(struct fg_chip *chip);
static ssize_t batt_safety_proc_write(struct file *file,const char __user *buffer,size_t count,loff_t *pos)
{
	int value=0;
	unsigned long time = 0;
	char buf[30] = {0};
	size_t buf_size;
	char *start = buf;

	buf_size = min(count, (size_t)(sizeof(buf)-1));
	if (copy_from_user(buf, buffer, buf_size)) {
		BAT_DBG_E("Failed to copy from user\n");
		return -EFAULT;
	}
	buf[buf_size] = 0;

	sscanf(start, "%d", &value);
	while (*start++ != ' ');
	sscanf(start, "%lu", &time);

	write_test_value = value;

	switch(value){
		case 1:
			g_cycle_count_data.battery_total_time = time;
		break;
		case 2:
			g_cycle_count_data.cycle_count = (int)time;
		break;
		case 3:
			g_cycle_count_data.high_temp_vol_time = time;
		break;
		case 4:
			g_cycle_count_data.high_temp_total_time = time;
		break;
		case 5:
			g_cycle_count_data.high_vol_total_time = time;
		break;
		default:
			BAT_DBG("input error!Now return\n");
			return count;
	}
	asus_judge_reload_condition(g_fgChip);
	BAT_DBG("value=%d;time=%lu\n", value, time);

	return count;
}

static const struct file_operations batt_safety_fops = {
	.owner = THIS_MODULE,
	.open = batt_safety_proc_open,
	.read = seq_read,
	.write = batt_safety_proc_write,
	.release = single_release,
};

static int batt_safety_csc_proc_show(struct seq_file *buf, void *data)
{
	int rc =0;

	rc = file_op(CYCLE_COUNT_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
	(char *)&g_cycle_count_data, sizeof(struct CYCLE_COUNT_DATA), FILE_OP_WRITE);
	if(rc < 0 )
		BAT_DBG_E("%s: write cycle count file error\n",__FUNCTION__);
	
	seq_printf(buf, "---show battery safety value---\n");
	seq_printf(buf, "cycle_count:%d\n", g_cycle_count_data.cycle_count);
	seq_printf(buf, "battery_total_time:%lu\n", g_cycle_count_data.battery_total_time);
	seq_printf(buf, "high_temp_total_time:%lu\n", g_cycle_count_data.high_temp_total_time);
	seq_printf(buf, "high_vol_total_time:%lu\n", g_cycle_count_data.high_vol_total_time);
	seq_printf(buf, "high_temp_vol_time:%lu\n", g_cycle_count_data.high_temp_vol_time);
	seq_printf(buf, "reload_condition:%d\n", g_cycle_count_data.reload_condition);

	return 0;
}
static int batt_safety_csc_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, batt_safety_csc_proc_show, NULL);
}

static int batt_safety_csc_erase(void){
	int rc =0;
	char buf[1]={0};

	g_cycle_count_data.battery_total_time = 0;
	g_cycle_count_data.cycle_count = 0;
	g_cycle_count_data.high_temp_total_time = 0;
	g_cycle_count_data.high_temp_vol_time = 0;
	g_cycle_count_data.high_vol_total_time = 0;
	g_cycle_count_data.reload_condition = 0;

	rc = file_op(CYCLE_COUNT_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
	(char *)&g_cycle_count_data, sizeof(struct CYCLE_COUNT_DATA), FILE_OP_WRITE);
	if(rc < 0 )
		BAT_DBG_E("%s:Write file:%s err!\n", __FUNCTION__, CYCLE_COUNT_FILE_NAME);

	rc = file_op(BAT_PERCENT_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
		(char *)&buf, sizeof(char), FILE_OP_WRITE);
	if(rc<0)
		BAT_DBG_E("%s:Write file:%s err!\n", __FUNCTION__, BAT_PERCENT_FILE_NAME);

	BAT_DBG("%s Done! rc(%d)\n",__FUNCTION__,rc);
	return rc;
}

int batt_safety_csc_backup(void){
	int rc = 0;
	struct CYCLE_COUNT_DATA buf;
//	char buf2[1]={0};

	rc = file_op(CYCLE_COUNT_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
		(char*)&buf, sizeof(struct CYCLE_COUNT_DATA), FILE_OP_READ);
	if(rc < 0) {
		BAT_DBG_E("Read cycle count file failed!\n");
		return rc;
	}

	rc = file_op(CYCLE_COUNT_SD_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
	(char *)&buf, sizeof(struct CYCLE_COUNT_DATA), FILE_OP_WRITE);
	if(rc < 0 ) 
		BAT_DBG_E("Write cycle count file failed!\n");

#if 0
	rc = file_op(BAT_PERCENT_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
		(char*)&buf2, sizeof(char), FILE_OP_READ);
	if(rc < 0) {
		BAT_DBG_E("Read cycle count percent file failed!\n");
		return rc;
	}

	rc = file_op(BAT_PERCENT_SD_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
	(char *)&buf2, sizeof(char), FILE_OP_WRITE);
	if(rc < 0 ) 
		BAT_DBG_E("Write cycle count percent file failed!\n");
#endif
	BAT_DBG("%s Done!\n",__FUNCTION__);
	return rc;
}

static int batt_safety_csc_restore(void){
	int rc = 0;
	struct CYCLE_COUNT_DATA buf;
	char buf2[1]={0};

	rc = file_op(CYCLE_COUNT_SD_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
		(char*)&buf, sizeof(struct CYCLE_COUNT_DATA), FILE_OP_READ);
	if(rc < 0) {
		BAT_DBG_E("Read cycle count file failed!\n");
		return rc;
	}

	rc = file_op(CYCLE_COUNT_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
	(char *)&buf, sizeof(struct CYCLE_COUNT_DATA), FILE_OP_WRITE);
	if(rc < 0 ) 
		BAT_DBG_E("Write cycle count file failed!\n");

	rc = file_op(BAT_PERCENT_SD_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
		(char*)&buf2, sizeof(char), FILE_OP_READ);
	if(rc < 0) {
		BAT_DBG_E("Read cycle count percent file failed!\n");
		return rc;
	}

	rc = file_op(BAT_PERCENT_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
	(char *)&buf2, sizeof(char), FILE_OP_WRITE);
	if(rc < 0 ) 
		BAT_DBG_E("Write cycle count percent file failed!\n");

	init_batt_cycle_count_data();
	BAT_DBG("%s Done! rc(%d)\n",__FUNCTION__,rc);
	return rc;
}

static int batt_safety_csc_getcyclecount(void){
	char buf[30]={0};
	int rc;

	sprintf(buf, "%d\n", g_cycle_count_data.cycle_count);
	BAT_DBG("cycle_count=%d\n", g_cycle_count_data.cycle_count);	

	rc = file_op(BAT_CYCLE_SD_FILE_NAME, CYCLE_COUNT_DATA_OFFSET,
		(char *)&buf, sizeof(char)*30, FILE_OP_WRITE);
	if(rc<0)
		pr_err("%s:Write file:%s err!\n", __FUNCTION__, BAT_CYCLE_SD_FILE_NAME);


	BAT_DBG("%s Done! rc(%d)\n",__FUNCTION__,rc);
	return rc;
}

//ASUS_BS battery health upgrade +++
static void batt_health_upgrade_debug_enable(bool enable){

	g_health_debug_enable = enable;
	BAT_DBG("%s: %d\n",__FUNCTION__,g_health_debug_enable);
}

static void batt_health_upgrade_enable(bool enable){

	g_health_upgrade_enable = enable;
	BAT_DBG("%s: %d\n",__FUNCTION__,g_health_upgrade_enable);
}

static int batt_health_config_proc_show(struct seq_file *buf, void *data)
{
	int count=0, i=0;
	unsigned long long bat_health_accumulate=0;

	seq_printf(buf, "start level:%d\n", g_health_upgrade_start_level);
	seq_printf(buf, "end level:%d\n", g_health_upgrade_end_level);
	seq_printf(buf, "upgrade time:%d\n", g_health_upgrade_upgrade_time);

	for(i=1;i<BAT_HEALTH_NUMBER_MAX;i++){
		if(g_bat_health_data_backup[i].health!=0){
			count++;
			bat_health_accumulate += g_bat_health_data_backup[i].health;
		}
	}
	g_bat_health_avg = bat_health_accumulate/count;	
	seq_printf(buf, "health_avg: %d\n", g_bat_health_avg);

	return 0;
}
static int batt_health_config_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, batt_health_config_proc_show, NULL);
}

static ssize_t batt_health_config_write(struct file *file,const char __user *buffer,size_t count,loff_t *pos)
{
	int command=0;
	int value = 0;
	char buf[30] = {0};
	size_t buf_size;
	char *start = buf;

	buf_size = min(count, (size_t)(sizeof(buf)-1));
	if (copy_from_user(buf, buffer, buf_size)) {
		BAT_DBG_E("Failed to copy from user\n");
		return -EFAULT;
	}
	buf[buf_size] = 0;

	sscanf(start, "%d", &command);
	while (*start++ != ' ');
	sscanf(start, "%d", &value);

	switch(command){
		case 1:
			g_health_upgrade_start_level = value;
			BAT_DBG("health upgrade start_level = %d;\n", value);
		break;
		case 2:
			g_health_upgrade_end_level = value;
			BAT_DBG("health upgrade end_level = %d;\n", value);
		break;
		case 3:
			g_health_upgrade_upgrade_time = value;
			BAT_DBG("health upgrade time = %d;\n", value);
		break;
		default:
			BAT_DBG("input error!Now return\n");
			return count;
	}

	return count;
}

static const struct file_operations batt_health_config_fops = {
	.owner = THIS_MODULE,
	.open = batt_health_config_proc_open,
	.read = seq_read,
	.write = batt_health_config_write,
	.release = single_release,
};

//ASUS_BS battery health upgrade ---

static ssize_t batt_safety_csc_proc_write(struct file *file,const char __user *buffer,size_t count,loff_t *pos)
{
	int value=0;
	char buf[2] = {0};
	size_t buf_size;
	char *start = buf;

	buf_size = min(count, (size_t)(sizeof(buf)-1));
	if (copy_from_user(buf, buffer, buf_size)) {
		BAT_DBG_E("Failed to copy from user\n");
		return -EFAULT;
	}
	buf[buf_size] = 0;

	sscanf(start, "%d", &value);

	switch(value){
		case 0: //erase
			batt_safety_csc_erase();
		break;
		case 1: //backup /persist to /sdcard
			batt_safety_csc_backup();
		break;
		case 2: //resotre /sdcard from /persist 
			batt_safety_csc_restore();
		break;
		case 3: //write cycle count to /sdcard
			batt_safety_csc_getcyclecount();
		break;
		case 4: //copy bat_health_data to /sdcard from /persist 
			batt_health_csc_backup();
			break;
		case 5: // disable battery health debug log
			batt_health_upgrade_debug_enable(false);
			break;
		case 6: // enable battery health debug log
			batt_health_upgrade_debug_enable(true);
			break;
		case 7: // disable battery health upgrade
			batt_health_upgrade_enable(false);
			break;
		case 8: // enable battery health upgrade
			batt_health_upgrade_enable(true);
			break;

		default:
			BAT_DBG_E("input error!Now return\n");
			return count;
	}

	return count;
}

static const struct file_operations batt_safety_csc_fops = {
	.owner = THIS_MODULE,
	.open = batt_safety_csc_proc_open,
	.read = seq_read,
	.write = batt_safety_csc_proc_write,
	.release = single_release,
};

static int cycle_count_proc_show(struct seq_file *buf, void *data)
{
	seq_printf(buf, "---show cycle count value---\n");
	seq_printf(buf, "cycle count:%d\n", g_cycle_count_data.cycle_count);

	return 0;
}

static int cycle_count_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, cycle_count_proc_show, NULL);
}

static const struct file_operations cycle_count_fops = {
	.owner = THIS_MODULE,
	.open = cycle_count_proc_open,
	.read = seq_read,
	.release = single_release,
};

static int condition_value_proc_show(struct seq_file *buf, void *data)
{
	if(!g_fgChip){
		BAT_DBG_E("chip oem is NULL!");
		return -1;
	}

	seq_printf(buf, "---show condition value---\n");
	seq_printf(buf, "condition1 battery time %lu\n", g_fgChip->condition1_battery_time);
	seq_printf(buf, "condition2 battery time %lu\n", g_fgChip->condition2_battery_time);
	seq_printf(buf, "condition1 cycle count %d\n", g_fgChip->condition1_cycle_count);
	seq_printf(buf, "condition2 cycle count %d\n", g_fgChip->condition2_cycle_count);
	seq_printf(buf, "condition1 temp time %lu\n", g_fgChip->condition1_temp_time);
	seq_printf(buf, "condition2 temp time %lu\n", g_fgChip->condition2_temp_time);
	seq_printf(buf, "condition1 temp&vol time %lu\n", g_fgChip->condition1_temp_vol_time);
	seq_printf(buf, "condition2 temp&vol time %lu\n", g_fgChip->condition2_temp_vol_time);
	seq_printf(buf, "condition1 vol time %lu\n", g_fgChip->condition1_vol_time);
	seq_printf(buf, "condition2 vol time %lu\n", g_fgChip->condition2_vol_time);

	return 0;
}

static int condition_value_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, condition_value_proc_show, NULL);
}

static ssize_t condition_value_proc_write(struct file *file,const char __user *buffer,size_t count,loff_t *pos)
{
	int value = 0;
	unsigned long condition1_time = 0;
	unsigned long condition2_time = 0;
	char buf[count];
	char *start = buf;

	if(!g_fgChip){
		BAT_DBG_E("g_fgChip is NULL!");
		return count;
	}

	if (copy_from_user(buf, buffer, count-1)) {
		BAT_DBG_E("Failed to copy from user\n");
		return -EFAULT;
	}
	buf[count] = 0;

	sscanf(start, "%d", &value);
	while (*start++ != ' ');
	sscanf(start, "%lu", &condition1_time);
	while (*start++ != ' ');
	sscanf(start, "%lu", &condition2_time);

	if(value && condition2_time <= condition1_time){
		BAT_DBG_E("input value error,please input correct value!\n");
		return count;
	}

	switch(value){
		case 0:
			init_battery_safety(g_fgChip);
			g_cycle_count_data.reload_condition = 0;
		break;
		case 1:
			g_fgChip->condition1_battery_time = condition1_time;
			g_fgChip->condition2_battery_time = condition2_time;
		break;
		case 2:
			g_fgChip->condition1_cycle_count = (int)condition1_time;
			g_fgChip->condition2_cycle_count = (int)condition2_time;
		break;
		case 3:
			g_fgChip->condition1_temp_vol_time = condition1_time;
			g_fgChip->condition2_temp_vol_time = condition2_time;
		break;
		case 4:
			g_fgChip->condition1_temp_time = condition1_time;
			g_fgChip->condition2_temp_time = condition2_time;
		break;
		case 5:
			g_fgChip->condition1_vol_time = condition1_time;
			g_fgChip->condition2_vol_time = condition2_time;
		break;
	}

	BAT_DBG("value=%d;condition1_time=%lu;condition2_time=%lu\n", value, condition1_time, condition2_time);
	return count;
}

static const struct file_operations condition_value_fops = {
	.owner = THIS_MODULE,
	.open = condition_value_proc_open,
	.read = seq_read,
	.write = condition_value_proc_write,
	.release = single_release,
};

static void create_batt_cycle_count_proc_file(void)
{
	struct proc_dir_entry *asus_batt_cycle_count_dir = proc_mkdir("Batt_Cycle_Count", NULL);
	struct proc_dir_entry *asus_batt_cycle_count_proc_file = proc_create("cycle_count", 0666,
		asus_batt_cycle_count_dir, &cycle_count_fops);
	struct proc_dir_entry *asus_batt_batt_safety_proc_file = proc_create("batt_safety", 0666,
		asus_batt_cycle_count_dir, &batt_safety_fops);
	struct proc_dir_entry *asus_batt_batt_safety_csc_proc_file = proc_create("batt_safety_csc", 0666,
		asus_batt_cycle_count_dir, &batt_safety_csc_fops);
	struct proc_dir_entry *asus_batt_safety_condition_proc_file = proc_create("condition_value", 0666,
		asus_batt_cycle_count_dir, &condition_value_fops);
}

//Write back batt_cyclecount data before restart/shutdown 
static int reboot_shutdown_prep(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	switch(event) {
	case SYS_RESTART:
	case SYS_POWER_OFF:
		/* Write data back to emmc */
		//write_back_cycle_count_data();
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}
/*  Call back function for reboot notifier chain  */
static struct notifier_block reboot_blk = {
	.notifier_call	= reboot_shutdown_prep,
};
//ASUS_BSP battery safety upgrade ---

//ASUS_BSP display callback function +++
static struct notifier_block fb_notifier = {
	.notifier_call	= fb_event_callback,
};
//ASUS_BSP display callback function ---
//ASUS_BSP ---

struct switch_dev batt_dev;
static ssize_t batt_switch_name_Titan(struct switch_dev *sdev, char *buf)
{
	char bat_modelName[16] = "";
	char bat_cellCode = 'X';
	int bat_ID = 0;
	int bat_profileVersion = 1;
	const char* bat_driverVersion = "14.1000.1706.8";
	if (g_fgChip) {
		if(g_ASUS_hwID <= ZE620KL_SR){
			if (g_fgChip->profile_available && !strcmp(g_fgChip->bp.batt_type_str, BATT_TYPE_CSL_V2_4P35)) {
				snprintf(bat_modelName, sizeof(bat_modelName), "%s", BATT_MODELNAME_TITAN);
				bat_cellCode = 'O';
				bat_ID = BATT_ID_51K_INDEX;
				bat_profileVersion = SECOND_PROFILE_VER;	
			}
		}else{
			if (g_fgChip->profile_available && !strcmp(g_fgChip->bp.batt_type_str, BATT_TYPE_CSL_ER_V2_4P35)) {
				snprintf(bat_modelName, sizeof(bat_modelName), "%s", BATT_MODELNAME_ARA);
				bat_cellCode = 'O';
				bat_ID = BATT_ID_51K_INDEX;
				bat_profileVersion = SECOND_PROFILE_VER;	
			}
		}
	}
	return sprintf(buf, "%s-%c-%02d-%04d-%s\n", bat_modelName, bat_cellCode, bat_ID, bat_profileVersion, bat_driverVersion);
}
/*
static ssize_t batt_switch_name(struct switch_dev *sdev, char *buf)
{
	char bat_modelName[16] = "";
	char bat_cellCode = 'X';
	int bat_ID = 0;
	int bat_profileVersion = 1;
	const char* bat_driverVersion = "13.16.0.8";
	if (the_chip) {
		if (g_ASUS_hwID >= ZE520KL_EVB && g_ASUS_hwID <= ZE520KL_MP) {
			snprintf(bat_modelName, sizeof(bat_modelName), "%s", BATT_MODELNAME_LEO);
		} else {
			snprintf(bat_modelName, sizeof(bat_modelName), "%s", BATT_MODELNAME_LIBRA);
		}
		if (!strcmp(the_chip->batt_type, BATT_GENERIC)) {
			if (g_ASUS_hwID >= ZE520KL_EVB && g_ASUS_hwID <= ZE520KL_MP) {
				bat_cellCode = 'T';
				bat_ID = 1;
				bat_profileVersion = 1;
			} else{
				bat_cellCode = 'O';
				bat_ID = 3;
				bat_profileVersion = 2;
			}
		} else if (!strcmp(the_chip->batt_type, BATT_LIBRA_SMP)) {
			bat_cellCode = 'G';
			bat_ID = 1;
			bat_profileVersion = 1;
		} else if (!strcmp(the_chip->batt_type, BATT_LIBRA_COSLIGHT)) {
			bat_cellCode = 'O';
			bat_ID = 3;
			bat_profileVersion = 2;
		} else if (!strcmp(the_chip->batt_type, BATT_LIBRA_LG)) {
			bat_cellCode = 'G';
			bat_ID = 2;
			bat_profileVersion = 1;
		} else if (!strcmp(the_chip->batt_type, BATT_LEO_SMP)) {
			bat_cellCode = 'T';
			bat_ID = 1;
			bat_profileVersion = 1;
		}
	}
	return sprintf(buf, "%s-%c-%02d-%04d-%s\n", bat_modelName, bat_cellCode, bat_ID, bat_profileVersion, bat_driverVersion);
}
*/
void register_battery_version(void)
{
	/* register switch device for battery information versions report */
	batt_dev.name = "battery";
	batt_dev.print_name = batt_switch_name_Titan;
	if (switch_dev_register(&batt_dev) < 0) {
		BAT_DBG_E("%s: fail to register battery switch\n", __func__);
	}
}

//ASUS_BSP LiJen add battery id switch dev +++
struct switch_dev batt_id_dev;
static ssize_t batt_id_switch_state(struct switch_dev *sdev, char *buf)
{
		int result = 0;
		char state[32] = "";
	
		result = g_fgChip->batt_id_ohms;
		if(!result){
			snprintf(state, sizeof(state), "FAIL");
			goto end;
		}	
		
		if (g_ASUS_hwID <= ZE620KL_SR) {
			if (g_fgChip->profile_available  && !strcmp(g_fgChip->bp.batt_type_str, BATT_TYPE_CSL))
				snprintf(state, sizeof(state), "PASS");
			else
				snprintf(state, sizeof(state), "FAIL");
	
		} else {
			if (g_fgChip->profile_available  && !strcmp(g_fgChip->bp.batt_type_str, BATT_TYPE_CSL_ER))
				snprintf(state, sizeof(state), "PASS");
			else
				snprintf(state, sizeof(state), "FAIL");
		}

	end:	
		BAT_DBG("%s: %dohms, %s\n", __func__, result, state);
		return sprintf(buf,"%s\n",state);		
}

void register_battery_id(void)
{
	/* register switch device for battery information versions report */
	batt_id_dev.name = "battery_id";
	batt_id_dev.print_state = batt_id_switch_state;
	if (switch_dev_register(&batt_id_dev) < 0) {
		BAT_DBG_E("%s: fail to register battery id switch\n", __func__);
	}
}
//ASUS_BSP LiJen add battery id switch dev ---

/*

update_gauge_status_work	log- report capacity
fix_maint_soc_work		smooth soc while delta_soc > 0
regular_check_soc_work		smooth soc reporting
init_asus_capacity_work	retrieve delta_soc
backup_asus_capacity_work	backup delta_soc

*/
void probe_init_asus_works(void)
{
	INIT_DELAYED_WORK(&fix_maint_soc_work, fix_maint_soc_worker);
	INIT_DELAYED_WORK(&regular_check_soc_work, regular_check_soc_worker);

	INIT_DELAYED_WORK(&init_asus_capacity_work, init_asus_capacity_worker);
	INIT_DELAYED_WORK(&backup_asus_capacity_work, backup_asus_capacity_worker);

	INIT_DELAYED_WORK(&battery_safety_work, battery_safety_worker); //ASUS_BSP battery safety upgrade
	INIT_DELAYED_WORK(&battery_health_work, battery_health_worker); //battery_health_work
	//INIT_DELAYED_WORK(&battery_metadata_work, battery_metadata_worker); //battery_metadata_work
}

void probe_create_procs(void)
{

	create_batt_current_proc_file();
	create_batt_voltage_proc_file();
	create_gaugeIC_status_proc_file();
	create_batt_mili_temp_proc_file();
	create_battID_status_proc_file();
	create_batt_type_proc_file();
	create_thermal_test_proc_file();
//ASUS_BSP LiJen config PMIC GPIO2 to ADC channel +++
	create_pmic_gpio2_adc_wp_proc_file();
	create_pmic_gpio2_adc_tds_proc_file();
	create_wp_state_proc_file();
//ASUS_BSP LiJen config PMIC GPIO2 to ADC channel ---

	register_battery_version();
	register_battery_id(); //ASUS_BSP LiJen add battery id switch dev 

}

//ASUS_BSP LiJen config PMIC GPIO2 VADC_BTM channel +++
//ASUS_BSP LiJen add water proof uevent +++
struct switch_dev wp_dev;
void register_water_proof_uenvent(void)
{
	int ret;
	// register switch device for water proof report
	wp_dev.name = "vbus_liquid";
	wp_dev.index = 0;

	ret = switch_dev_register(&wp_dev);
	if (ret < 0)
        BAT_DBG_E("%s: Fail to register switch water_proof uevent\n", __func__);
    else
        BAT_DBG("%s: Success to register switch water_proof uevent\n", __func__);
}
//ASUS_BSP LiJen add water proof uevent ---

extern int setup_vadc_monitor(struct fg_chip *chip);
static void vadc_notification(enum qpnp_tm_state state, void *ctx)
{
	//struct qpnp_chg_chip *chip = ctx;
	//int rc=0;
	
	if (state == ADC_TM_HIGH_STATE) { //In High state
		pr_err("%s: ADC_TM_HIGH_STATE\n", __func__);
	}
	else { //In low state
		pr_err("%s: ADC_TM_LOW_STATE\n", __func__);
	}

	setup_vadc_monitor(ctx);
}

bool double_check_is_liquid_mode(int32_t adc)
{
	int32_t adc1=-1, adc2=-1, adc3=-1, adc4=-1, adc5=-1;
	bool ret;

	adc1 = adc;
	msleep(300);
	adc2 = get_pmgpio2_vadc_voltage(VADC_AMUX1_GPIO_PU3);
	if(adc2 >= 200000 && adc2 <= 1300000){
		msleep(300);
		adc3 = get_pmgpio2_vadc_voltage(VADC_AMUX1_GPIO_PU3);
		if(adc3 >= 200000 && adc3 <= 1300000){
			msleep(300);
			adc4 = get_pmgpio2_vadc_voltage(VADC_AMUX1_GPIO_PU3);
			if(adc4 >= 200000 && adc4 <= 1300000){
				msleep(300);
				adc5 = get_pmgpio2_vadc_voltage(VADC_AMUX1_GPIO_PU3);	
				if(adc5 >= 200000 && adc5 <= 1300000){
					ret = true;
					BAT_DBG("%s: adc(%d, %d, %d, %d, %d) ret(%d)\n", __func__, adc1, adc2,adc3, adc4, adc5, ret);
					return ret;
				}
			}
		}
	}

	ret = false;
	BAT_DBG("%s: adc(%d, %d, %d, %d, %d) ret(%d)\n", __func__, adc1, adc2,adc3, adc4, adc5, ret);
	return ret;
}

int setup_vadc_monitor(struct fg_chip *chip)
{
	int rc=0;
	int32_t adc;

restart:
	adc = get_pmgpio2_vadc_voltage(VADC_AMUX1_GPIO_PU3);
	
	/* Get the ADC device instance (one time) */
	g_adc_tm_dev = qpnp_get_adc_tm(chip->dev, "pm-gpio2");
	if (IS_ERR(g_adc_tm_dev)) {
			rc = PTR_ERR(g_adc_tm_dev);
			BAT_DBG("%s qpnp_get_adc_tm fail(%d)\n", __func__, rc);
	}

	if(adc > 1300000){ //Normal
		g_adc_param.low_thr = 1100000; //uV
		g_adc_param.state_request = ADC_TM_LOW_THR_ENABLE;		
		g_wp_state = 0;
		//WeiYu ++ temply remove reporting uevent, HW issue
		//switch_set_state(&wp_dev, 0); //ASUS_BSP LiJen add water proof uevent
		//ASUSErclog(ASUS_USB_WATER_ALERT, "Water_alert is dismissed.state %d\n",g_wp_state);		
	}else if(adc < 200000){ //Cable in
		g_adc_param.high_thr = 300000; //uV
		g_adc_param.state_request = ADC_TM_HIGH_THR_ENABLE; 	
		g_wp_state = 2;
		//switch_set_state(&wp_dev, 0); //ASUS_BSP LiJen add water proof uevent
		//ASUSErclog(ASUS_USB_WATER_ALERT, "Water_alert is dismissed. state %d\n",g_wp_state);		
	}else{	//With Liquid
		if(g_wp_state != 1){
			//double check into Liquid mode
			if(double_check_is_liquid_mode(adc)){
				g_adc_param.high_thr = 1300000; //uV
				g_adc_param.state_request = ADC_TM_HIGH_THR_ENABLE; 
				g_wp_state = 1;
				//switch_set_state(&wp_dev, 1); //ASUS_BSP LiJen add water proof uevent
				//ASUSErclog(ASUS_USB_WATER_ALERT, "Water_alert is triggered\n");		
			}else{
				goto restart;
			}
		}else{
			//do nothing			
		}
	}
	pr_err("%s adc(%d), g_wp_state(%d), low_thr(%d), high_thr(%d), state_request(%d)\n", __func__, adc, g_wp_state, g_adc_param.low_thr, g_adc_param.high_thr, g_adc_param.state_request);
	//ASUSEvtlog("%s adc(%d), g_wp_state(%d), low_thr(%d), high_thr(%d), state_request(%d)\n", __func__, adc, g_wp_state, g_adc_param.low_thr, g_adc_param.high_thr, g_adc_param.state_request);
	g_adc_param.channel = 0x72;
	g_adc_param.timer_interval = ADC_MEAS2_INTERVAL_1S;
	g_adc_param.btm_ctx = chip;
	g_adc_param.threshold_notification = vadc_notification;
	rc = qpnp_adc_tm_channel_measure(g_adc_tm_dev, &g_adc_param);
	if (rc){
		BAT_DBG_E("%s: qpnp_adc_tm_channel_measure fail(%d) ---\n", __func__,rc);
	}

	return rc;
}
//ASUS_BSP LiJen config PMIC GPIO2 VADC_BTM channel ---

static int fg_gen3_probe(struct platform_device *pdev)
{
	struct fg_chip *chip;
	struct power_supply_config fg_psy_cfg;
	int rc, msoc, volt_uv, batt_temp;

	BAT_DBG("%s: +++\n", __func__);  //ASUS_BSP
	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip){
		BAT_DBG("%s: rc(%d) ---\n", __func__,rc); //ASUS_BSP
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;
	chip->debug_mask = &fg_gen3_debug_mask;
	chip->irqs = fg_irqs;
	chip->charge_status = -EINVAL;
	chip->prev_charge_status = -EINVAL;
	chip->ki_coeff_full_soc = -EINVAL;
	chip->online_status = -EINVAL;
	chip->regmap = dev_get_regmap(chip->dev->parent, NULL);
	if (!chip->regmap) {
		dev_err(chip->dev, "Parent regmap is unavailable\n");
		BAT_DBG("%s: rc(%d) ---\n", __func__,rc); //ASUS_BSP
		return -ENXIO;
	}

	chip->batt_id_chan = iio_channel_get(chip->dev, "rradc_batt_id");
	if (IS_ERR(chip->batt_id_chan)) {
		if (PTR_ERR(chip->batt_id_chan) != -EPROBE_DEFER)
			pr_err("batt_id_chan unavailable %ld\n",
				PTR_ERR(chip->batt_id_chan));
		rc = PTR_ERR(chip->batt_id_chan);
		chip->batt_id_chan = NULL;
		BAT_DBG("%s: rc(%d) ---\n", __func__,rc); //ASUS_BSP
		return rc;
	}

	rc = of_property_match_string(chip->dev->of_node,
				"io-channel-names", "rradc_die_temp");
	if (rc >= 0) {
		chip->die_temp_chan = iio_channel_get(chip->dev,
						"rradc_die_temp");
		if (IS_ERR(chip->die_temp_chan)) {
			if (PTR_ERR(chip->die_temp_chan) != -EPROBE_DEFER)
				pr_err("rradc_die_temp unavailable %ld\n",
					PTR_ERR(chip->die_temp_chan));
			rc = PTR_ERR(chip->die_temp_chan);
			chip->die_temp_chan = NULL;
			BAT_DBG("%s: rc(%d) ---\n", __func__,rc);
			return rc;
		}
	}

	chip->awake_votable = create_votable("FG_WS", VOTE_SET_ANY, fg_awake_cb,
					chip);
	if (IS_ERR(chip->awake_votable)) {
		rc = PTR_ERR(chip->awake_votable);
		chip->awake_votable = NULL;
		goto exit;
	}

	chip->delta_bsoc_irq_en_votable = create_votable("FG_DELTA_BSOC_IRQ",
						VOTE_SET_ANY,
						fg_delta_bsoc_irq_en_cb, chip);
	if (IS_ERR(chip->delta_bsoc_irq_en_votable)) {
		rc = PTR_ERR(chip->delta_bsoc_irq_en_votable);
		chip->delta_bsoc_irq_en_votable = NULL;
		goto exit;
	}

	chip->batt_miss_irq_en_votable = create_votable("FG_BATT_MISS_IRQ",
						VOTE_SET_ANY,
						fg_batt_miss_irq_en_cb, chip);
	if (IS_ERR(chip->batt_miss_irq_en_votable)) {
		rc = PTR_ERR(chip->batt_miss_irq_en_votable);
		chip->batt_miss_irq_en_votable = NULL;
		goto exit;
	}
	g_fgChip = chip; //ASUS_BSP
	rc = fg_parse_dt(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Error in reading DT parameters, rc:%d\n",
			rc);
		goto exit;
	}

	mutex_init(&chip->bus_lock);
	mutex_init(&chip->sram_rw_lock);
	mutex_init(&chip->cyc_ctr.lock);
	mutex_init(&chip->cl.lock);
	mutex_init(&chip->ttf.lock);
	mutex_init(&chip->charge_full_lock);
	mutex_init(&chip->charge_status_lock);	//ASUS_BSP
	mutex_init(&chip->qnovo_esr_ctrl_lock);
	spin_lock_init(&chip->suspend_lock);
	init_completion(&chip->soc_update);
	init_completion(&chip->soc_ready);
	INIT_DELAYED_WORK(&chip->profile_load_work, profile_load_work);
	INIT_WORK(&chip->status_change_work, status_change_work);
	INIT_DELAYED_WORK(&chip->ttf_work, ttf_work);
	INIT_DELAYED_WORK(&chip->sram_dump_work, sram_dump_work);
	INIT_WORK(&chip->esr_filter_work, esr_filter_work);
	alarm_init(&chip->esr_filter_alarm, ALARM_BOOTTIME,
			fg_esr_filter_alarm_cb);

	probe_init_asus_works(); //ASUS_BSP
	rc = fg_memif_init(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Error in initializing FG_MEMIF, rc:%d\n",
			rc);
		goto exit;
	}

	rc = fg_hw_init(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Error in initializing FG hardware, rc:%d\n",
			rc);
		goto exit;
	}

	platform_set_drvdata(pdev, chip);

	/* Register the power supply */
	fg_psy_cfg.drv_data = chip;
	fg_psy_cfg.of_node = NULL;
	fg_psy_cfg.supplied_to = NULL;
	fg_psy_cfg.num_supplicants = 0;
	chip->fg_psy = devm_power_supply_register(chip->dev, &fg_psy_desc,
			&fg_psy_cfg);
	if (IS_ERR(chip->fg_psy)) {
		pr_err("failed to register fg_psy rc = %ld\n",
				PTR_ERR(chip->fg_psy));
		goto exit;
	}

	chip->nb.notifier_call = fg_notifier_cb;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		goto exit;
	}

	rc = fg_register_interrupts(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Error in registering interrupts, rc:%d\n",
			rc);
		goto exit;
	}

	/* Keep SOC_UPDATE_IRQ disabled until we require it */
	if (fg_irqs[SOC_UPDATE_IRQ].irq)
		disable_irq_nosync(fg_irqs[SOC_UPDATE_IRQ].irq);

	/* Keep BSOC_DELTA_IRQ disabled until we require it */
	vote(chip->delta_bsoc_irq_en_votable, DELTA_BSOC_IRQ_VOTER, false, 0);

	/* Keep BATT_MISSING_IRQ disabled until we require it */
	vote(chip->batt_miss_irq_en_votable, BATT_MISS_IRQ_VOTER, false, 0);

#ifdef CONFIG_DEBUG_FS
	rc = fg_debugfs_create(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Error in creating debugfs entries, rc:%d\n",
			rc);
		goto exit;
	}
#endif

	rc = fg_get_battery_voltage(chip, &volt_uv);
	if (!rc)
		rc = fg_get_prop_capacity(chip, &msoc);

	if (!rc)
		rc = fg_get_battery_temp(chip, &batt_temp);

	if (!rc) {
		BAT_DBG("battery SOC:%d voltage: %duV temp: %d id: %dKOhms\n",
			msoc, volt_uv, batt_temp, chip->batt_id_ohms / 1000);
		rc = fg_esr_filter_config(chip, batt_temp, false);
		if (rc < 0)
			pr_err("Error in configuring ESR filter rc:%d\n", rc);
	}

	//ASUS_BSP battery safety upgrade +++
	init_battery_safety(chip);
	//init_batt_cycle_count_data();
	register_reboot_notifier(&reboot_blk);
	schedule_delayed_work(&battery_safety_work, 30 * HZ);
	//ASUS_BSP battery safety upgrade ---

	//ASUS_BSP display callback function +++
	rc = fb_register_client(&fb_notifier);
	if (rc < 0) {
		pr_err("%s: fb_register_client failed, returned with rc=%d\n",
							 __func__, rc);
	}
	//ASUS_BSP display callback function ---
	wake_lock_init(&bat_health_lock, WAKE_LOCK_SUSPEND, "bat_health_lock");
	battery_health_data_reset();
	schedule_delayed_work(&battery_health_work, 300 * HZ); //battery_health_work
	//schedule_delayed_work(&battery_metadata_work, BATTERY_METADATA_UPGRADE_TIME * HZ); //battery_metadata_work

	device_init_wakeup(chip->dev, true);
	queue_delayed_work(system_power_efficient_wq,
		&chip->profile_load_work, 0);

//ASUS_BSP +++
	probe_create_procs();		

	schedule_delayed_work(&regular_check_soc_work, SOC_CHECK_INTERVAL * HZ);
	schedule_delayed_work(&init_asus_capacity_work, 5* HZ);

	register_water_proof_uenvent(); //ASUS_BSP LiJen add water proof uevent

//ASUS_BSP LiJen config PMIC GPIO2 VADC_BTM channel +++
#if 0 //disable water proof monitor
	rc = setup_vadc_monitor(chip);
	if(rc){
		BAT_DBG_E("%s: setup_vadc_monitor fail(%d) ---\n", __func__,rc);
	}
#endif
//ASUS_BSP LiJen config PMIC GPIO2 VADC_BTM channel ---

#ifdef ASUS_ZE620KL_PROJECT	
	fg_batt_id_ready = 1;	
#endif

	BAT_DBG("FG GEN3 driver probed successfully\n");
	BAT_DBG("%s: ---\n", __func__);
//ASUS_BSP ---
	return 0;
exit:
	fg_cleanup(chip);
	BAT_DBG("%s: rc(%d) ---\n", __func__,rc); //ASUS_BSP
	return rc;
}

static int fg_gen3_suspend(struct device *dev)
{
	struct fg_chip *chip = dev_get_drvdata(dev);
	int rc;

	spin_lock(&chip->suspend_lock);
	chip->suspended = true;
	spin_unlock(&chip->suspend_lock);

	rc = fg_esr_timer_config(chip, true);
	if (rc < 0)
		pr_err("Error in configuring ESR timer, rc=%d\n", rc);

	cancel_delayed_work_sync(&chip->ttf_work);
	if (fg_sram_dump)
		cancel_delayed_work_sync(&chip->sram_dump_work);
	return 0;
}

static int fg_gen3_resume(struct device *dev)
{
	struct fg_chip *chip = dev_get_drvdata(dev);
	int rc;

	rc = fg_esr_timer_config(chip, false);
	if (rc < 0)
		pr_err("Error in configuring ESR timer, rc=%d\n", rc);

	queue_delayed_work(system_power_efficient_wq,
		&chip->ttf_work, 0);
	if (fg_sram_dump)
		queue_delayed_work(system_power_efficient_wq,
			&chip->sram_dump_work,
				msecs_to_jiffies(fg_sram_dump_period_ms));

	if (!work_pending(&chip->status_change_work)) {
		pm_stay_awake(chip->dev);
		schedule_work(&chip->status_change_work);
	}

	spin_lock(&chip->suspend_lock);
	chip->suspended = false;
	spin_unlock(&chip->suspend_lock);

	return 0;
}

static const struct dev_pm_ops fg_gen3_pm_ops = {
	.suspend	= fg_gen3_suspend,
	.resume		= fg_gen3_resume,
};

static int fg_gen3_remove(struct platform_device *pdev)
{
	struct fg_chip *chip = dev_get_drvdata(&pdev->dev);

	fg_cleanup(chip);
	return 0;
}

static void fg_gen3_shutdown(struct platform_device *pdev)
{
	struct fg_chip *chip = dev_get_drvdata(&pdev->dev);
	int rc, bsoc;

	if (chip->charge_full) {
		rc = fg_get_sram_prop(chip, FG_SRAM_BATT_SOC, &bsoc);
		if (rc < 0) {
			pr_err("Error in getting BATT_SOC, rc=%d\n", rc);
			return;
		}

		/* We need 2 most significant bytes here */
		bsoc = (u32)bsoc >> 16;

		rc = fg_configure_full_soc(chip, bsoc);
		if (rc < 0) {
			pr_err("Error in configuring full_soc, rc=%d\n", rc);
			return;
		}
	}
}

static const struct of_device_id fg_gen3_match_table[] = {
	{.compatible = FG_GEN3_DEV_NAME},
	{},
};

static struct platform_driver fg_gen3_driver = {
	.driver = {
		.name = FG_GEN3_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = fg_gen3_match_table,
		.pm		= &fg_gen3_pm_ops,
	},
	.probe		= fg_gen3_probe,
	.remove		= fg_gen3_remove,
	.shutdown	= fg_gen3_shutdown,
};

static int __init fg_gen3_init(void)
{
	return platform_driver_register(&fg_gen3_driver);
}

static void __exit fg_gen3_exit(void)
{
	return platform_driver_unregister(&fg_gen3_driver);
}

module_init(fg_gen3_init);
module_exit(fg_gen3_exit);

MODULE_DESCRIPTION("QPNP Fuel gauge GEN3 driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" FG_GEN3_DEV_NAME);
