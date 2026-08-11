/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#ifndef _MI_DSI_PANEL_H_
#define _MI_DSI_PANEL_H_

#include <linux/types.h>
#include "dsi_defs.h"
#include "mi_disp_feature.h"
#include <linux/pm_wakeup.h>
#include <drm/mi_disp.h>

/* ---------------------------- */
/* | 15 - 14 | 13 - 7 | 6 - 0 | */
/* ---------------------------- */
/* |   mode  | sf fps | min fps| */
/* ---------------------------- -*/
/* mode: 1 idle, 2 auto, 3 qsync */
/*   1 << 14 | 120 << 7 | 24     */
/* ---------------------------- */
#define FPS_NORMAL           0
#define FPS_VALUE_MASK       0x7F
#define FPS_SF_FPS_OFFSET    7
#define FPS_MODE_OFFSET      14
#define FPS_MODE_VALUE_MASK  0x3
#define FPS_MODE_IDLE        1
#define FPS_MODE_AUTO        2
#define FPS_MODE_QSYNC       3
#define FPS_COUNT(DDIC_MODE,DDIC_FPS,DDIC_MIN_FPS) ((DDIC_MODE * 100000) + (DDIC_FPS * 100) + DDIC_MIN_FPS)

#define PMIC_PWRKEY_BARK_TRIGGER 1
#define PMIC_PWRKEY_TRIGGER 2
#define DISPLAY_DELAY_SHUTDOWN_TIME_MS 1800

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

/* Peak offset default data: 9 registers (B0~B8), 9 values each */
static const int16_t peak_offset_default_60hz[9][9] = {
	{    0, -249, -222, -194, -184, -154, -137, -126, -115},  /* B0 */
	{  -96,  -77,  -61,  -47,  -29,    5,   44,   83,  130},  /* B1 */
	{  227,  372,  515,  564,  671,  743,  743,    0,    0},  /* B2 */
	{    0, -287, -237, -205, -195, -163, -148, -136, -126},  /* B3 */
	{ -109,  -93,  -79,  -67,  -52,  -25,    2,   30,   61},  /* B4 */
	{  127,  208,  290,  313,  362,  386,  386,    0,    0},  /* B5 */
	{    0, -261, -223, -190, -182, -150, -134, -122, -111},  /* B6 */
	{  -92,  -73,  -55,  -40,  -21,   14,   52,   89,  131},  /* B7 */
	{  222,  337,  452,  485,  569,  613,  613,    0,    0},  /* B8 */
};

static const int16_t peak_offset_default_120hz[9][9] = {
	{    0, -279, -244, -210, -196, -162, -144, -130, -119},  /* B0 */
	{  -99,  -80,  -63,  -48,  -30,    3,   43,   80,  126},  /* B1 */
	{  221,  364,  504,  551,  654,  733,  733,    0,    0},  /* B2 */
	{    0, -322, -260, -222, -206, -172, -153, -141, -130},  /* B3 */
	{ -112,  -97,  -82,  -69,  -54,  -27,    0,   28,   58},  /* B4 */
	{  124,  203,  283,  305,  354,  377,  377,    0,    0},  /* B5 */
	{    0, -292, -245, -207, -193, -157, -140, -126, -115},  /* B6 */
	{  -94,  -74,  -57,  -41,  -22,   12,   50,   85,  128},  /* B7 */
	{  217,  329,  441,  475,  554,  598,  598,    0,    0},  /* B8 */
};

static const int16_t peak_offset_default_144hz[9][9] = {
	{    0, -304, -264, -224, -207, -170, -150, -136, -124},  /* B0 */
	{ -102,  -84,  -67,  -52,  -34,   -1,   38,   72,  119},  /* B1 */
	{  220,  365,  509,  567,  681,  760,  760,    0,    0},  /* B2 */
	{    0, -350, -278, -239, -220, -181, -162, -147, -136},  /* B3 */
	{ -117, -102,  -87,  -75,  -61,  -34,   -7,   22,   51},  /* B4 */
	{  117,  195,  277,  299,  352,  383,  383,    0,    0},  /* B5 */
	{    0, -313, -262, -221, -203, -165, -145, -130, -116},  /* B6 */
	{  -96,  -77,  -61,  -44,  -27,    8,   46,   82,  124},  /* B7 */
	{  214,  328,  443,  483,  566,  618,  618,    0,    0},  /* B8 */
};

static const int16_t peak_offset_default_165hz[9][9] = {
	{    0, -301, -259, -219, -203, -167, -149, -134, -123},  /* B0 */
	{ -103,  -82,  -66,  -51,  -32,    2,   39,   78,  124},  /* B1 */
	{  219,  366,  508,  560,  665,  751,  751,    0,    0},  /* B2 */
	{    0, -355, -275, -232, -215, -179, -161, -146, -135},  /* B3 */
	{ -117, -100,  -85,  -72,  -58,  -31,   -5,   24,   53},  /* B4 */
	{  118,  197,  279,  302,  356,  380,  380,    0,    0},  /* B5 */
	{    0, -316, -259, -215, -198, -162, -144, -129, -117},  /* B6 */
	{  -96,  -77,  -60,  -43,  -24,   11,   45,   82,  123},  /* B7 */
	{  212,  327,  442,  480,  563,  616,  616,    0,    0},  /* B8 */
};

enum mafr_area_index {
	MAFR_FIRST_AREA = BIT(0),
	MAFR_SECOND_AREA = BIT(1),
	MAFR_THIRD_AREA = BIT(2),
	MAFR_FULL_AREA = 0xFF,
};

enum dbv_Interval {
	DBV_IN19_27 = 0x0,
	DBV_IN28_4B,
	DBV_IN4C_83,
	DBV_MAX
};

struct dsi_panel;

enum backlight_dimming_state {
	STATE_NONE,
	STATE_DIM_BLOCK,
	STATE_DIM_RESTORE,
	STATE_ALL
};

enum panel_state {
	PANEL_STATE_OFF = 0,
	PANEL_STATE_ON,
	PANEL_STATE_DOZE_HIGH,
	PANEL_STATE_DOZE_LOW,
	PANEL_STATE_MAX,
};

enum dc_lut_state {
	DC_LUT_60HZ,
	DC_LUT_120HZ,
	DC_LUT_MAX
};

enum dc_feature_type {
	TYPE_NONE = 0,
	TYPE_CRC_SKIP_BL,
	TYPE_DC_PWM_SWITCH_SKIP_BL
};

/* Enter/Exit DC_LUT info */
struct dc_lut_cfg {
	bool update_done;
	u8 enter_dc_lut[DC_LUT_MAX][75];
	u8 exit_dc_lut[DC_LUT_MAX][75];
};

struct flat_mode_cfg {
	bool update_done;
	int cur_flat_state;  /*only use when flat cmd need sync with te*/
	u8 flat_on_data[4];
	u8 flat_off_data[4];
};

enum demura_dc_state {
	STATE_UNKNOW,
	STATE_ON,
	STATE_OFF
};

struct mi_dsi_panel_cfg {
	struct dsi_panel *dsi_panel;

	/* xiaomi panel id */
	u64 mi_panel_id;

	bool is_tddi_flag;
	bool tddi_gesture_flag;
	bool panel_dead_flag;

	/* xiaomi feature values */
	int feature_val[DISP_FEATURE_MAX];

	/* indicate esd check gpio and config irq */
	int esd_err_irq_gpio;
	int esd_err_irq;
	int esd_err_irq_flags;
	bool esd_err_enabled;
	bool panel_build_id_read_needed;

	/* indicate second esd check gpio and config irq */
	int esd_err_irq_gpio_second;
	int esd_err_irq_second;
	int esd_err_irq_flags_second;
	bool esd_err_enabled_second;

	/* brightness control */
	u32 last_bl_level;
	u32 last_no_zero_bl_level;
	atomic_t brightness_clone;
	u32 max_brightness_clone;
	bool layer_has_aod;
	bool isPreBrightnessOn;

	/* AOD control */
	u32 doze_brightness;
	u32 last_doze_brightness;
	struct mutex doze_lock;
	u32 fullscreen_aod_status;
	struct wakeup_source *disp_wakelock;
	int doze_hbm_dbv_level;
	int doze_lbm_dbv_level;
	bool aod_status;
	bool sf_set_doze_brightness;
	bool panel_forbid_fps_switch_in_aod;

	/* DDIC round corner */
	bool ddic_round_corner_enabled;

	/* DC */
	bool dc_feature_enable;
	bool dc_update_flag;
	enum dc_feature_type dc_type;
	u32 dc_threshold;
	struct dc_lut_cfg dc_cfg;
	u32 real_dc_state;
	/*dbi*/
	int real_dbi_state;

	/* flat mode */
	bool flatmode_default_on_enabled;
	bool flat_sync_te;
	bool flat_update_flag;
	struct flat_mode_cfg flat_cfg;

	/* peak hdr */
	bool is_peak_hdr;
	u8 gamma_rgb_param[6];
	bool read_gamma_success;

    /* record the last refresh_rate */
	u32 last_refresh_rate;
	u32 last_fps_mode;
	u32 last_fps;

	/* Dimming */
	u32 panel_on_dimming_delay;
	u32 dimming_state;
	bool disable_ic_dimming;
	u32 ic_dimming_by_feature;

	/* Panel status */
	int panel_state;

	u8 panel_batch_number;
	bool panel_batch_number_read_done;

	u32 hbm_backlight_threshold;
	bool count_hbm_by_backlight;
	int pmic_pwrkey_status;

	/*DDIC ADD CMD Type*/
	enum dsi_cmd_set_type skip_source_type;
	enum dsi_cmd_set_type dbi_bwg_type;

	/*hbm gamma read*/
	bool aod_enter_flags;
	bool aod_exit_flags;

	/* mafr(multi-area refresh rate) config */
	bool mafr_enable_flag_;
	bool last_mafr_enable_flag_;
	struct dsi_rect  surface_view_roi;
	struct dsi_rect  last_surface_view_roi;
	/* mafr debug config */
	bool mafr_debug_switch;
	bool mafr_debug_feature_enable;
	u32 first_mafr_framerate;
	u32 second_mafr_framerate;
	u32 third_mafr_framerate;
	/* timestamp for DFS backlight zero */
	u64 timestamp_panelon;
	u64 timestamp_backlight_zero;
	bool whether_backlight_zero;
	/* Local HBM */
	bool local_hbm_enabled;
	u32 lhbm_ui_ready_delay_frame;
	u32 lhbm_lbl_mode_threshold;
	u32 lhbm_hbm_mode_threshold;
	bool need_fod_animal_in_normal;
	bool aod_to_normal_statue;
	bool in_fod_calibration;
	bool lhbm_off_sync_te;
	u8 lhbm_rgb_param[18];
	bool read_lhbm_gamma_success;
	/*DBV Demura*/
	bool demura_switch_enabled;
	enum demura_dc_state demura_dc;
	bool demura_cmd_executed;
	/*TP callback for tddi reset*/
	void (*mi_display_gesture_cb)(void);
	bool fp_unlock_success;
	u8 o3_dbv_param[2];
	bool backlight_set_skip_in_lp;
	bool multi_timing_enable;
	u8 panel_type;
	/*emv info for asic*/
	struct MiEmvConfig emv_cfg;
	struct mutex emv_info_lock;

	/* peak offset raw data: [9] registers (B0~B8), [18] bytes per register
	* peak offset on data: [9][18] (2 bytes combined + default, split back)
	*/
	unsigned char peak_offset_off_60hz[9][18];
	unsigned char peak_offset_off_120hz[9][18];
	unsigned char peak_offset_off_144hz[9][18];
	unsigned char peak_offset_off_165hz[9][18];
	unsigned char peak_offset_on_60hz[9][18];
	unsigned char peak_offset_on_120hz[9][18];
	unsigned char peak_offset_on_144hz[9][18];
	unsigned char peak_offset_on_165hz[9][18];
};

struct panel_batch_info
{
	u8 batch_number;       /* Panel batch number */
	char *batch_name;      /* Panel batch name */
};

struct drm_panel_build_id_config {
	struct dsi_panel_cmd_set id_cmd;
	struct dsi_panel_cmd_set sub_id_cmd;
	u32 id_cmds_rlen;
	u8 build_id;
};

struct drm_panel_wp_config {
	struct dsi_panel_cmd_set pre_tx_cmd;
	struct dsi_panel_cmd_set wp_cmd;
	u32 wp_read_info_index;
	u32 wp_cmds_rlen;
	u8 *return_buf;
};

struct drm_panel_cell_id_config {
	struct dsi_panel_cmd_set pre_tx_cmd;
	struct dsi_panel_cmd_set cell_id_cmd;
	struct dsi_panel_cmd_set after_tx_cmd;
	u32 cell_id_read_info_index;
	u32 cell_id_cmds_rlen;
	u8 *return_buf;
};

extern const char *cmd_set_prop_map[DSI_CMD_SET_MAX];
extern const char *cmd_set_update_map[DSI_CMD_UPDATE_MAX];

int mi_dsi_panel_pre_init(struct dsi_panel *panel);
int mi_dsi_panel_init(struct dsi_panel *panel);
int mi_dsi_panel_deinit(struct dsi_panel *panel);
int mi_dsi_acquire_wakelock(struct dsi_panel *panel);
int mi_dsi_release_wakelock(struct dsi_panel *panel);

bool is_aod_and_panel_initialized(struct dsi_panel *panel);

bool is_backlight_set_skip(struct dsi_panel *panel, u32 bl_lvl);

void mi_dsi_panel_update_last_bl_level(struct dsi_panel *panel,
			int brightness);

void mi_dsi_panel_update_ic_dimming_by_bl(struct dsi_panel *panel, u32 bl_lvl);

int mi_dsi_panel_esd_irq_ctrl(struct dsi_panel *panel,
			bool enable);

int mi_dsi_panel_esd_irq_ctrl_locked(struct dsi_panel *panel,
			bool enable);

int mi_dsi_print_register_value_log(struct dsi_panel *panel,
			struct dsi_cmd_desc *cmd);

int mi_dsi_panel_parse_sub_timing(struct mi_mode_info *mode,
			struct dsi_parser_utils *utils);

int mi_dsi_panel_parse_cmd_sets_update(struct dsi_panel *panel,
			struct dsi_display_mode *mode);

int mi_dsi_panel_update_cmd_set(struct dsi_panel *panel,
			struct dsi_display_mode *cur_mode, enum dsi_cmd_set_type type,
			struct dsi_cmd_update_info *info, u8 *payload, u32 size);

int mi_dsi_panel_write_cmd_set(struct dsi_panel *panel,
			struct dsi_panel_cmd_set *cmd_sets);

bool mi_dsi_panel_need_tx_or_rx_cmd(u32 feature_id);

int mi_dsi_panel_set_disp_param(struct dsi_panel *panel,
			struct disp_feature_ctl *ctl);

int mi_dsi_panel_get_disp_param(struct dsi_panel *panel,
			struct disp_feature_ctl *ctl);

ssize_t mi_dsi_panel_show_disp_param(struct dsi_panel *panel,
			char *buf, size_t size);

int mi_dsi_panel_set_doze_brightness(struct dsi_panel *panel,
			u32 doze_brightness);

int mi_dsi_panel_get_doze_brightness(struct dsi_panel *panel,
			u32 *doze_brightness);

int mi_dsi_panel_get_brightness(struct dsi_panel *panel,
			u32 *brightness);

int mi_dsi_panel_write_dsi_cmd(struct dsi_panel *panel,
			struct dsi_cmd_rw_ctl *ctl);

int mi_dsi_panel_write_dsi_cmd_set(struct dsi_panel *panel, int type);

ssize_t mi_dsi_panel_show_dsi_cmd_set_type(struct dsi_panel *panel,
			char *buf, size_t size);

int mi_dsi_panel_set_brightness_clone(struct dsi_panel *panel,
			u32 brightness_clone);

int mi_dsi_panel_get_brightness_clone(struct dsi_panel *panel,
			u32 *brightness_clone);

int mi_dsi_panel_get_max_brightness_clone(struct dsi_panel *panel,
			u32 *max_brightness_clone);

int mi_dsi_panel_set_dc_mode(struct dsi_panel *panel, bool enable);

int mi_dsi_panel_set_dc_mode_locked(struct dsi_panel *panel, bool enable);

int mi_dsi_panel_set_ltmp_cmpst_locked(struct dsi_panel *panel, bool enable);

int mi_dsi_panel_set_round_corner_locked(struct dsi_panel *panel,
			bool enable);

int mi_dsi_panel_set_round_corner(struct dsi_panel *panel,
			bool enable);

int mi_dsi_update_flat_mode_on_cmd(struct dsi_panel *panel, enum dsi_cmd_set_type type);

int mi_dsi_update_timing_switch_and_flat_mode_cmd(struct dsi_panel *panel, enum dsi_cmd_set_type type);

int dsi_panel_parse_build_id_read_config(struct dsi_panel *panel);

int mi_dsi_update_51_mipi_cmd(struct dsi_panel *panel, enum dsi_cmd_set_type type, int bl_lvl);

int dsi_panel_parse_wp_reg_read_config(struct dsi_panel *panel);

int dsi_panel_parse_cell_id_read_config(struct dsi_panel *panel);

int mi_dsi_set_switch_cmd_before(struct dsi_panel *panel, int fps_mode);
int mi_dsi_panel_set_nolp_locked(struct dsi_panel *panel);
int mi_dsi_panel_update_gamma_param(struct dsi_panel * panel, u32 cmd_update_index,
               enum dsi_cmd_set_type type);
int mi_dsi_panel_set_count_info(struct dsi_panel * panel, struct disp_count_info *count_info);

int mi_dsi_panel_aod_to_normal_optimize_locked(struct dsi_panel *panel, bool enable);

int mi_dsi_update_switch_cmd_O3(struct dsi_panel *panel, u32 cmd_update_index, u32 index);

int mi_dsi_panel_parse_dc_fps_config(struct dsi_panel *panel,
		struct dsi_display_mode *mode);
int mi_dsi_panel_update_gamma_param(struct dsi_panel * panel, u32 cmd_update_index,
               enum dsi_cmd_set_type type);

int mi_dsi_panel_get_mafr_fps_index(struct dsi_panel *panel, enum mafr_area_index index);
int mi_dsi_panel_update_mafr_status(struct dsi_panel *panel, struct dsi_rect *roi);

ssize_t mi_dsi_panel_set_debug_param(struct dsi_panel *panel);
ssize_t mi_dsi_panel_get_debug_param(struct dsi_panel *panel, char *buf, size_t size);

int mi_dsi_panel_auto_limit_by_dbv(struct dsi_panel *panel, u32 bl_lvl);

int mi_dsi_panel_dbv_demura_compensate(struct dsi_panel *panel, u32 bl_lvl);

int mi_dsi_panel_csc_by_temper_comp(struct dsi_panel *panel, int temp_val);
int mi_dsi_panel_csc_by_temper_comp_locked(struct dsi_panel *panel, int temp_val);
int mi_dsi_panel_nova_csc_by_temper_comp_locked(struct dsi_panel *panel, int temp_val);

int mi_dsi_pwr_enable_vregs(struct dsi_regulator_info *regs, bool enable, int index);

int mi_dsi_panel_set_lhbm_fod_locked(struct dsi_panel *panel, struct disp_feature_ctl *ctl);

bool is_hbm_fod_on(struct dsi_panel *panel);

int mi_dsi_panel_gamma_by_temper_comp(struct dsi_panel *panel, int temp_val);
int mi_dsi_panel_gamma_by_temper_comp_locked(struct dsi_panel *panel, int temp_val);

int mi_dsi_panel_parse_timing_fps_params(struct dsi_panel *panel, struct dsi_display_mode *mode, struct dsi_parser_utils *utils);

int mi_dsi_panel_tigger_dimming_delayed_work(struct dsi_panel *panel);

ssize_t mi_dsi_panel_peakoffset_info_read(struct dsi_panel *panel);

int mi_dsi_update_peakoffset_mode_on_cmd(struct dsi_panel *panel, struct dsi_display_mode *mode);

int mi_dsi_update_peakoffset_mode_off_cmd(struct dsi_panel *panel, struct dsi_display_mode *mode);

#endif /* _MI_DSI_PANEL_H_ */
