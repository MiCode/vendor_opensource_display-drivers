/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"mi-dsi-panel:[%s:%d] " fmt, __func__, __LINE__
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/rtc.h>
#include <linux/pm_wakeup.h>
#include <video/mipi_display.h>
#include <drm/mi_disp.h>

#include "dsi_panel.h"
#include "dsi_display.h"
#include "dsi_ctrl_hw.h"
#include "dsi_parser.h"
#include "internals.h"
#include "sde_connector.h"
#include "sde_trace.h"
#include "mi_dsi_panel.h"
#include "mi_disp_feature.h"
#include "mi_disp_print.h"
#include "mi_disp_parser.h"
#include "mi_dsi_display.h"
#include "mi_disp_lhbm.h"
#include "mi_panel_id.h"
#include "mi_disp_nvt_alpha_data.h"
#include "mi_dsi_panel_count.h"
#include "mi_kernel_timer.h"
#include "../dsi/dsi_defs.h"
#define to_dsi_display(x) container_of(x, struct dsi_display, host)

static int mi_dsi_update_lhbm_cmd_87reg(struct dsi_panel *panel,
			enum dsi_cmd_set_type type, int bl_lvl);
int mi_dsi_update_51_mipi_cmd(struct dsi_panel *panel,
			enum dsi_cmd_set_type type, int bl_lvl);

static u64 g_panel_id[MI_DISP_MAX];

typedef int (*mi_display_pwrkey_callback)(int);
#ifdef LONG_PRESS_TO_FORCE_FASTBOOT
extern void mi_display_pwrkey_callback_set(mi_display_pwrkey_callback);
#endif

static int mi_panel_id_init(struct dsi_panel *panel)
{
	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	g_panel_id[mi_get_disp_id(panel->type)] = panel->mi_cfg.mi_panel_id;

	return 0;
}

enum mi_project_panel_id mi_get_panel_id_by_dsi_panel(struct dsi_panel *panel)
{
	if (!panel) {
		DISP_ERROR("invalid params\n");
		return PANEL_ID_INVALID;
	}

	return mi_get_panel_id(panel->mi_cfg.mi_panel_id);
}

enum mi_project_panel_id mi_get_panel_id_by_disp_id(int disp_id)
{
	if (!is_support_disp_id(disp_id)) {
		DISP_ERROR("Unsupported display id\n");
		return PANEL_ID_INVALID;
	}

	return mi_get_panel_id(g_panel_id[disp_id]);
}

int mi_dsi_panel_init(struct dsi_panel *panel)
{
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	DISP_ERROR("mi_dsi_panel_init panel type:%s panel name %s  ", panel->type, panel->name);

	//mi_dsi_panel_count_init(panel);

	mi_cfg->dsi_panel = panel;
	mutex_init(&mi_cfg->doze_lock);

	mi_cfg->disp_wakelock = wakeup_source_register(NULL, "disp_wakelock");
	if (!mi_cfg->disp_wakelock) {
		DISP_ERROR("doze_wakelock wake_source register failed");
		return -ENOMEM;
	}

	mi_dsi_panel_parse_config(panel);
	mi_panel_id_init(panel);
//	atomic_set(&mi_cfg->real_brightness_clone, 0);
#ifdef LONG_PRESS_TO_FORCE_FASTBOOT
	mi_display_pwrkey_callback_set(mi_display_powerkey_callback);
#endif
	return 0;
}

int mi_dsi_panel_deinit(struct dsi_panel *panel)
{
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	if (mi_cfg->disp_wakelock) {
		wakeup_source_unregister(mi_cfg->disp_wakelock);
	}
	return 0;
}

int mi_dsi_acquire_wakelock(struct dsi_panel *panel)
{
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	if (mi_cfg->disp_wakelock) {
		__pm_stay_awake(mi_cfg->disp_wakelock);
	}

	return 0;
}

int mi_dsi_release_wakelock(struct dsi_panel *panel)
{
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	if (mi_cfg->disp_wakelock) {
		__pm_relax(mi_cfg->disp_wakelock);
	}

	return 0;

}

bool is_aod_and_panel_initialized(struct dsi_panel *panel)
{
	if ((panel->power_mode == SDE_MODE_DPMS_LP1 ||
		panel->power_mode == SDE_MODE_DPMS_LP2) &&
		dsi_panel_initialized(panel)){
		return true;
	} else {
		return false;
	}
}

int mi_dsi_panel_esd_irq_ctrl(struct dsi_panel *panel,
				bool enable)
{
	int ret  = 0;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);
	ret = mi_dsi_panel_esd_irq_ctrl_locked(panel, enable);
	mutex_unlock(&panel->panel_lock);

	return ret;
}

int mi_dsi_panel_esd_irq_ctrl_locked(struct dsi_panel *panel,
				bool enable)
{
	struct mi_dsi_panel_cfg *mi_cfg;
	struct irq_desc *desc;

	if (!panel || !panel->panel_initialized) {
		DISP_ERROR("Panel not ready!\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;
	if (gpio_is_valid(mi_cfg->esd_err_irq_gpio)) {
		if (mi_cfg->esd_err_irq) {
			if (enable) {
				if (!mi_cfg->esd_err_enabled) {
					desc = irq_to_desc(mi_cfg->esd_err_irq);
					if (!irq_settings_is_level(desc))
						desc->istate &= ~IRQS_PENDING;
					enable_irq_wake(mi_cfg->esd_err_irq);
					enable_irq(mi_cfg->esd_err_irq);
					mi_cfg->esd_err_enabled = true;
					DISP_INFO("[%s] esd irq is enable\n", panel->type);
				}
			} else {
				if (mi_cfg->esd_err_enabled) {
					disable_irq_wake(mi_cfg->esd_err_irq);
					disable_irq_nosync(mi_cfg->esd_err_irq);
					mi_cfg->esd_err_enabled = false;
					DISP_INFO("[%s] esd irq is disable\n", panel->type);
				}
			}
		}
	} else {
		DISP_INFO("[%s] esd irq gpio invalid\n", panel->type);
	}

	return 0;
}

static void mi_disp_set_dimming_delayed_work_handler(struct kthread_work *work)
{
	struct disp_delayed_work *delayed_work = container_of(work,
					struct disp_delayed_work, delayed_work.work);
	struct dsi_panel *panel = (struct dsi_panel *)(delayed_work->data);
	struct disp_feature_ctl ctl;

	memset(&ctl, 0, sizeof(struct disp_feature_ctl));
	ctl.feature_id = DISP_FEATURE_DIMMING;
	ctl.feature_val = FEATURE_ON;

	DISP_INFO("[%s] panel set backlight dimming on\n", panel->type);
	mi_dsi_acquire_wakelock(panel);
	mi_dsi_panel_set_disp_param(panel, &ctl);
	mi_dsi_release_wakelock(panel);

	kfree(delayed_work);
}

int mi_dsi_panel_tigger_dimming_delayed_work(struct dsi_panel *panel)
{
	int disp_id = 0;
	struct disp_feature *df = mi_get_disp_feature();
	struct disp_display *dd_ptr;
	struct disp_delayed_work *delayed_work;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	delayed_work = kzalloc(sizeof(*delayed_work), GFP_KERNEL);
	if (!delayed_work) {
		DISP_ERROR("failed to allocate delayed_work buffer\n");
		return -ENOMEM;
	}

	disp_id = mi_get_disp_id(panel->type);
	dd_ptr = &df->d_display[disp_id];

	kthread_init_delayed_work(&delayed_work->delayed_work,
			mi_disp_set_dimming_delayed_work_handler);
	delayed_work->dd_ptr = dd_ptr;
	delayed_work->wq = &dd_ptr->pending_wq;
	delayed_work->data = panel;

	return kthread_queue_delayed_work(dd_ptr->worker, &delayed_work->delayed_work,
				msecs_to_jiffies(panel->mi_cfg.panel_on_dimming_delay));
}


static void mi_disp_timming_switch_delayed_work_handler(struct kthread_work *work)
{
	struct disp_delayed_work *delayed_work = container_of(work,
					struct disp_delayed_work, delayed_work.work);
	struct dsi_panel *dsi_panel = (struct dsi_panel *)(delayed_work->data);

	mi_dsi_acquire_wakelock(dsi_panel);

	dsi_panel_acquire_panel_lock(dsi_panel);
	if (dsi_panel->panel_initialized) {
		dsi_panel_tx_cmd_set(dsi_panel, DSI_CMD_SET_MI_FRAME_SWITCH_MODE_SEC);
		DISP_INFO("DSI_CMD_SET_MI_FRAME_SWITCH_MODE_SEC\n");
	} else {
		DISP_ERROR("Panel not initialized, don't send DSI_CMD_SET_MI_FRAME_SWITCH_MODE_SEC\n");
	}
	dsi_panel_release_panel_lock(dsi_panel);

	mi_dsi_release_wakelock(dsi_panel);

	kfree(delayed_work);
}

int mi_dsi_panel_tigger_timming_switch_delayed_work(struct dsi_panel *panel)
{
	int disp_id = 0;
	struct disp_feature *df = mi_get_disp_feature();
	struct disp_display *dd_ptr;
	struct disp_delayed_work *delayed_work;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	delayed_work = kzalloc(sizeof(*delayed_work), GFP_KERNEL);
	if (!delayed_work) {
		DISP_ERROR("failed to allocate delayed_work buffer\n");
		return -ENOMEM;
	}

	disp_id = mi_get_disp_id(panel->type);
	dd_ptr = &df->d_display[disp_id];

	kthread_init_delayed_work(&delayed_work->delayed_work,
			mi_disp_timming_switch_delayed_work_handler);
	delayed_work->dd_ptr = dd_ptr;
	delayed_work->wq = &dd_ptr->pending_wq;
	delayed_work->data = panel;
	return kthread_queue_delayed_work(dd_ptr->worker, &delayed_work->delayed_work,
				msecs_to_jiffies(20));
}

int mi_dsi_update_flat_mode_on_cmd(struct dsi_panel *panel,
		enum dsi_cmd_set_type type)
{
	struct dsi_cmd_update_info *info = NULL;
	struct dsi_display_mode *cur_mode = NULL;
	u8 update_D5_cfg = 0;
	u32 cmd_update_index = 0;
	u8  cmd_update_count = 0;
	int i = 0;

	if (type != DSI_CMD_SET_MI_FLAT_MODE_ON) {
		DISP_ERROR("invalid type parameter\n");
		return -EINVAL;
	}

	if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	cur_mode = panel->cur_mode;

	cmd_update_index = DSI_CMD_SET_MI_FLAT_MODE_ON_UPDATE;
	info = cur_mode->priv_info->cmd_update[cmd_update_index];
	cmd_update_count = cur_mode->priv_info->cmd_update_count[cmd_update_index];
	for (i = 0; i < cmd_update_count; i++ ) {
		if (info && info->mipi_address == 0xD5) {
			update_D5_cfg = 0x73;
			DISP_INFO("panel id is 0x%x, update_D5_cfg is 0x73", panel->id_config.build_id);
			mi_dsi_panel_update_cmd_set(panel, cur_mode,
				type, info,
				&update_D5_cfg, sizeof(update_D5_cfg));
		}

		info++;
	}

	return 0;
}

bool is_hbm_fod_on(struct dsi_panel *panel)
{
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;
	int feature_val;
	bool is_fod_on = false;

	feature_val = mi_cfg->feature_val[DISP_FEATURE_LOCAL_HBM];
	switch (feature_val) {
	case LOCAL_HBM_NORMAL_WHITE_1000NIT:
	case LOCAL_HBM_NORMAL_WHITE_750NIT:
	case LOCAL_HBM_NORMAL_WHITE_500NIT:
	case LOCAL_HBM_NORMAL_WHITE_110NIT:
	case LOCAL_HBM_NORMAL_GREEN_500NIT:
	case LOCAL_HBM_HLPM_WHITE_1000NIT:
	case LOCAL_HBM_HLPM_WHITE_110NIT:
		is_fod_on = true;
		break;
	default:
		break;
	}

	if (mi_cfg->feature_val[DISP_FEATURE_HBM_FOD] == FEATURE_ON) {
		is_fod_on = true;
	}

	return is_fod_on;
}

bool is_dc_on_skip_backlight(struct dsi_panel *panel, u32 bl_lvl)
{
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;

	if (!mi_cfg->dc_feature_enable)
		return false;
	if (mi_cfg->feature_val[DISP_FEATURE_DC] == FEATURE_OFF)
		return false;
	if (mi_cfg->dc_type == TYPE_CRC_SKIP_BL && bl_lvl < mi_cfg->dc_threshold)
		return true;
	else
		return false;
}

bool is_backlight_set_skip(struct dsi_panel *panel, u32 bl_lvl)
{
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;
	int feature_val = -1;

	if (mi_cfg->in_fod_calibration || is_hbm_fod_on(panel)) {
		if (bl_lvl != 0) {
			if (mi_get_panel_id(panel->mi_cfg.mi_panel_id) == P17_PANEL_PA) {
				DISP_INFO("[%s] skip set backlight %d due to LHBM is on,"
					"but update alpha\n", panel->type, bl_lvl);
				feature_val = mi_cfg->feature_val[DISP_FEATURE_LOCAL_HBM];
				if (feature_val== LOCAL_HBM_NORMAL_WHITE_1000NIT ||
					feature_val == LOCAL_HBM_HLPM_WHITE_1000NIT) {
					mi_dsi_update_lhbm_cmd_87reg(panel,
						DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT, bl_lvl);
					dsi_panel_tx_cmd_set(panel,
						DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT);
				} else if (feature_val == LOCAL_HBM_NORMAL_WHITE_110NIT ||
					feature_val == LOCAL_HBM_HLPM_WHITE_110NIT) {
					mi_dsi_update_lhbm_cmd_87reg(panel,
						DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT, bl_lvl);
					dsi_panel_tx_cmd_set(panel,
						DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT);
				} else {
					DISP_INFO("[%s] skip set backlight %d due to fod hbm "
							"or fod calibration\n", panel->type, bl_lvl);
				}
				return true;
			}
			DISP_INFO("[%s] update 51 reg to %d even LHBM is on,",
				panel->type, bl_lvl);
			return false;
		} else {
			DSI_INFO("[%s] skip set backlight %d due to fod hbm "
					"or fod calibration\n", panel->type, bl_lvl);
		}
		return true;
	} else if (bl_lvl != 0 && is_dc_on_skip_backlight(panel, bl_lvl)) {
		DISP_DEBUG("[%s] skip set backlight %d due to DC on\n", panel->type, bl_lvl);
		return true;
	} else if (panel->power_mode == SDE_MODE_DPMS_LP1 && bl_lvl == 0) {
		DSI_INFO("[%s] skip set backlight 0 due to LP1 on\n", panel->type);
		return true;
	} else {
		return false;
	}
}

void mi_dsi_panel_update_last_bl_level(struct dsi_panel *panel, int brightness)
{
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return;
	}

	mi_cfg = &panel->mi_cfg;

	if ((mi_cfg->last_bl_level == 0 || mi_cfg->dimming_state == STATE_DIM_RESTORE) &&
		brightness > 0 && !is_hbm_fod_on(panel) && !mi_cfg->in_fod_calibration) {
		if (!panel->mi_cfg.dimming_need_update_speed)
			mi_dsi_panel_tigger_dimming_delayed_work(panel);
		else {
			mi_cfg->feature_val[DISP_FEATURE_DIMMING] = FEATURE_ON;
			DISP_INFO("[%s] enable dimming setting.\n", panel->type);
		}

		if (mi_cfg->dimming_state == STATE_DIM_RESTORE)
			mi_cfg->dimming_state = STATE_NONE;
	}

	mi_cfg->last_bl_level = brightness;
	if (brightness != 0)
		mi_cfg->last_no_zero_bl_level = brightness;

	return;
}

void mi_dsi_panel_update_dimming_frame(struct dsi_panel *panel, u8 dimming_switch, u8 frame)
{
	static u8 frame_state = DIMMING_SPEED_0FRAME;
	static u8 dimming_switch_state = DIMMING_SPEED_SWITCH_DEFAULT;

	if (frame_state == frame && dimming_switch_state == dimming_switch) {
		DISP_ERROR("don't need to update dimming state\n");
		return;
	}

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return;
	}
/*
	if (dimming_switch_state != dimming_switch) {
		if (dimming_switch == DIMMING_SPEED_SWITCH_ON) {
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGON);
			DISP_INFO("[%s] panel set dimming on.\n", panel->type);
		} else if (dimming_switch == DIMMING_SPEED_SWITCH_OFF) {
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGOFF);
			DISP_INFO("[%s] panel set dimming off.\n", panel->type);
		}
		dimming_switch_state = dimming_switch;
	}
*/
	if (dimming_switch_state == DIMMING_SPEED_SWITCH_ON) {
		if (frame == DIMMING_SPEED_8FRAME && frame_state != frame) {
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMING_8FRAME);
			DISP_INFO("[%s] panel set dimming frame %d.\n", panel->type, frame);
		} else if (frame == DIMMING_SPEED_4FRAME && frame_state != frame) {
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMING_4FRAME);
			DISP_INFO("[%s] panel set dimming frame %d.\n", panel->type, frame);
		}
	}
	frame_state = frame;

	return;
}

int mi_dsi_print_51_backlight_log(struct dsi_panel *panel,
		struct dsi_cmd_desc *cmd)
{
	u8 *buf = NULL;
	u32 bl_lvl = 0;
	int i = 0;
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!panel || !cmd) {
		DISP_ERROR("Invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;
	buf = (u8 *)cmd->msg.tx_buf;
	if (buf && buf[0] == MIPI_DCS_SET_DISPLAY_BRIGHTNESS) {
		if (cmd->msg.tx_len >= 3) {
			if (panel->bl_config.bl_inverted_dbv)
				bl_lvl = (buf[1] << 8) | buf[2];
			else
				bl_lvl = buf[1] | (buf[2] << 8);

		DISP_TIME_INFO("[%s] set 51 backlight %d\n", panel->type, bl_lvl);

		}

		if (mi_get_backlight_log_mask() & BACKLIGHT_LOG_ENABLE) {
			DISP_INFO("[%s] [0x51 backlight debug] tx_len = %lu\n",
					panel->type, cmd->msg.tx_len);
			for (i = 0; i < cmd->msg.tx_len; i++) {
				DISP_INFO("[%s] [0x51 backlight debug] tx_buf[%d] = 0x%02X\n",
					panel->type, i, buf[i]);
			}

			if (mi_get_backlight_log_mask() & BACKLIGHT_LOG_DUMP_STACK)
				dump_stack();
		}
	}

	return 0;
}

int mi_dsi_panel_parse_sub_timing(struct mi_mode_info *mi_mode,
				  struct dsi_parser_utils *utils)
{
	int rc = 0;
	const char *ddic_mode;

	rc = utils->read_u32(utils->data,
				"qcom,mdss-dsi-panel-framerate",
				&mi_mode->timing_refresh_rate);
	if (rc) {
		DISP_ERROR("failed to read qcom,mdss-dsi-panel-framerate, rc=%d\n",
		       rc);
		goto error;
	}

	ddic_mode = utils->get_property(utils->data, "mi,mdss-dsi-ddic-mode", NULL);
	if (ddic_mode) {
		if (!strcmp(ddic_mode, "normal")) {
			mi_mode->ddic_mode = DDIC_MODE_NORMAL;
		} else if (!strcmp(ddic_mode, "idle")) {
			mi_mode->ddic_mode= DDIC_MODE_IDLE;
		} else if (!strcmp(ddic_mode, "auto")) {
			mi_mode->ddic_mode= DDIC_MODE_AUTO;
		} else if (!strcmp(ddic_mode, "qsync")) {
			mi_mode->ddic_mode = DDIC_MODE_QSYNC;
		} else if (!strcmp(ddic_mode, "diff")) {
			mi_mode->ddic_mode = DDIC_MODE_DIFF;
		} else if (!strcmp(ddic_mode, "test")) {
			mi_mode->ddic_mode = DDIC_MODE_TEST;
		} else {
			DISP_INFO("Unrecognized ddic mode, default is normal mode\n");
			mi_mode->ddic_mode = DDIC_MODE_NORMAL;
		}
	} else {
		DISP_DEBUG("Falling back ddic mode to default normal mode\n");
		mi_mode->ddic_mode = DDIC_MODE_NORMAL;
		mi_mode->sf_refresh_rate = mi_mode->timing_refresh_rate;
		mi_mode->ddic_min_refresh_rate = mi_mode->timing_refresh_rate;
	}

    if (mi_mode->ddic_mode != DDIC_MODE_NORMAL) {
		rc = utils->read_u32(utils->data,
				"mi,mdss-dsi-sf-framerate",
				  &mi_mode->sf_refresh_rate);
		if (rc) {
			DISP_ERROR("failed to read mi,mdss-dsi-sf-framerate, rc=%d\n", rc);
			goto error;
		}

		rc = utils->read_u32(utils->data,
				"mi,mdss-dsi-ddic-min-framerate",
				  &mi_mode->ddic_min_refresh_rate);
		if (rc) {
			DISP_ERROR("failed to read mi,mdss-dsi-ddic-min-framerate, rc=%d\n", rc);
			goto error;
		}
	}

	DISP_INFO("ddic_mode:%s, sf_refresh_rate:%d, ddic_min_refresh_rate:%d\n",
		get_ddic_mode_name(mi_mode->ddic_mode),
		mi_mode->sf_refresh_rate, mi_mode->ddic_min_refresh_rate);

error:
	return rc;
}

int mi_dsi_print_log(struct dsi_panel *panel,
		struct dsi_cmd_desc *cmd)
{
	u8 *buf = NULL;
	int i = 0;
	if (!panel || !cmd) {
		DISP_ERROR("Invalid params\n");
		return -EINVAL;
	}
	buf = (u8 *)cmd->msg.tx_buf;
    if (buf) {
	for (i = 0; i < cmd->msg.tx_len; i++) {
		DISP_INFO("[%s] [debug] tx_buf[%d] = 0x%02X\n",
			panel->type, i, buf[i]);
	}
	}
	return 0;
}

static int mi_dsi_panel_parse_cmd_sets_update_sub(struct dsi_panel *panel,
		struct dsi_display_mode *mode, enum dsi_cmd_set_type type)
{
	int rc = 0;
	int j = 0, k = 0;
	u32 length = 0;
	u32 count = 0;
	u32 size = 0;
	u32 *arr_32 = NULL;
	const u32 *arr;
	struct dsi_parser_utils *utils = &panel->utils;
	struct dsi_cmd_update_info *info;
	int type1 = -1;

	if (!mode || !mode->priv_info) {
		DISP_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	arr = utils->get_property(utils->data, cmd_set_update_map[type],
		&length);

	if (!arr) {
		DISP_DEBUG("[%s] commands not defined\n", cmd_set_update_map[type]);
		return rc;
	}

	if (length & 0x1) {
		DISP_ERROR("[%s] syntax error\n", cmd_set_update_map[type]);
		rc = -EINVAL;
		goto error;
	}

	DISP_INFO("%s length = %d\n", cmd_set_update_map[type], length);

	for (j = DSI_CMD_SET_PRE_ON; j < DSI_CMD_SET_MAX; j++) {
		if (strstr(cmd_set_update_map[type], cmd_set_prop_map[j])) {
			type1 = j;
			DISP_INFO("find type(%d) is [%s] commands\n", type1,
				cmd_set_prop_map[type1]);
			break;
		}
	}
	if (type1 < 0 || j == DSI_CMD_SET_MAX) {
		rc = -EINVAL;
		goto error;
	}

	length = length / sizeof(u32);
	size = length * sizeof(u32);

	arr_32 = kzalloc(size, GFP_KERNEL);
	if (!arr_32) {
		rc = -ENOMEM;
		goto error;
	}

	rc = utils->read_u32_array(utils->data, cmd_set_update_map[type],
						arr_32, length);
	if (rc) {
		DISP_ERROR("[%s] read failed\n", cmd_set_update_map[type]);
		goto error_free_arr_32;
	}

	count = length / 3;
	size = count * sizeof(*info);
	info = kzalloc(size, GFP_KERNEL);
	if (!info) {
		rc = -ENOMEM;
		goto error_free_arr_32;
	}

	mode->priv_info->cmd_update[type] = info;
	mode->priv_info->cmd_update_count[type] = count;

	for (k = 0; k < length; k += 3) {
		info->type = type1;
		info->mipi_address= arr_32[k];
		info->index= arr_32[k + 1];
		info->length= arr_32[k + 2];
		DISP_INFO("update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
			cmd_set_update_map[type], info->mipi_address,
			info->index, info->length);
		info++;
	}
error_free_arr_32:
	kfree(arr_32);
error:
	return rc;
}


int mi_dsi_panel_parse_cmd_sets_update(struct dsi_panel *panel,
		struct dsi_display_mode *mode)
{
	int rc = 0;
	int i = 0;


	if (!mode || !mode->priv_info) {
		DISP_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	DISP_INFO("WxH: %dx%d, FPS: %d\n", mode->timing.h_active,
			mode->timing.v_active, mode->timing.refresh_rate);

	for (i = 0; i < DSI_CMD_UPDATE_MAX; i++) {
		rc = mi_dsi_panel_parse_cmd_sets_update_sub(panel, mode, i);
	}

	return rc;
}

int mi_dsi_panel_update_cmd_set(struct dsi_panel *panel,
			struct dsi_display_mode *mode, enum dsi_cmd_set_type type,
			struct dsi_cmd_update_info *info, u8 *payload, u32 size)
{
	int rc = 0;
	int i = 0;
	struct dsi_cmd_desc *cmds = NULL;
	u32 count;
	u8 *tx_buf = NULL;
	size_t tx_len;

	if (!panel || !mode || !mode->priv_info)
	{
		DISP_ERROR("invalid panel or cur_mode params\n");
		return -EINVAL;
	}

	if (!info || !payload) {
		DISP_ERROR("invalid info or payload params\n");
		return -EINVAL;
	}

	if (type != info->type || size != info->length) {
		DISP_ERROR("please check type(%d, %d) or update size(%d, %d)\n",
			type, info->type, info->length, size);
		return -EINVAL;
	}

	cmds = mode->priv_info->cmd_sets[type].cmds;
	count = mode->priv_info->cmd_sets[type].count;
	if (count == 0) {
		DISP_ERROR("[%s] No commands to be sent\n", cmd_set_prop_map[type]);
		return -EINVAL;
	}

	DISP_DEBUG("WxH: %dx%d, FPS: %d\n", mode->timing.h_active,
			mode->timing.v_active, mode->timing.refresh_rate);

	DISP_DEBUG("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
			panel->type, cmd_set_prop_map[info->type],
			info->mipi_address, info->index, info->length);
	for (i = 0; i < size; i++) {
		DISP_DEBUG("[%s] payload[%d] = 0x%02X\n", panel->type, i, payload[i]);
	}

	if (cmds && count >= info->index) {
		tx_buf = (u8 *)cmds[info->index].msg.tx_buf;
		tx_len = cmds[info->index].msg.tx_len;
		if (tx_buf && tx_buf[0] == info->mipi_address && tx_len >= info->length) {
			memcpy(&tx_buf[1], payload, info->length);
			for (i = 0; i < tx_len; i++) {
				DISP_DEBUG("[%s] tx_buf[%d] = 0x%02X\n",
					panel->type, i, tx_buf[i]);
			}
		} else {
			if (tx_buf) {
				DISP_ERROR("[%s] %s mipi address(0x%02X != 0x%02X)\n",
					panel->type, cmd_set_prop_map[type],
					tx_buf[0], info->mipi_address);
			} else {
				DISP_ERROR("[%s] panel tx_buf is NULL pointer\n",
					panel->type);
			}
			rc = -EINVAL;
		}
	} else {
		DISP_ERROR("[%s] panel cmd[%s] index error\n",
			panel->type, cmd_set_prop_map[type]);
		rc = -EINVAL;
	}

	return rc;
}
int mi_dsi_panel_parse_gamma_config(struct dsi_panel *panel,
		struct dsi_display_mode *mode)
{
	int rc;
	struct dsi_display_mode_priv_info *priv_info;
	struct dsi_parser_utils *utils = &panel->utils;
	u32 gamma_cfg[2] = {0, 0};
	u32 lhbm_gamma_cfg[2] = {0, 0};

	priv_info = mode->priv_info;

	rc = utils->read_u32_array(utils->data, "mi,mdss-flat-status-control-gamma-cfg",
			gamma_cfg, 2);
	if (rc) {
		DISP_DEBUG("mi,mdss-flat-status-control-gamma-cfg not defined rc=%d\n", rc);
	} else {
		DISP_INFO("FPS: %d, gamma cfg: 0x%02X, 0x%02X\n",
			mode->timing.refresh_rate, gamma_cfg[0], gamma_cfg[1]);
		priv_info->flat_on_gamma = gamma_cfg[0];
		priv_info->flat_off_gamma = gamma_cfg[1];
	}

	rc = utils->read_u32_array(utils->data, "mi,mdss-flat-status-control-lhbm-gamma-cfg",
			lhbm_gamma_cfg, 2);
	if (rc) {
		DISP_DEBUG("mi,mdss-flat-status-control-lhbm-gamma-cfg not defined rc=%d\n", rc);
	} else {
		DISP_INFO("FPS: %d, lhbm gamma cfg: 0x%02X, 0x%02X\n",
			mode->timing.refresh_rate, lhbm_gamma_cfg[0], lhbm_gamma_cfg[1]);
		priv_info->flat_on_lhbm_gamma = lhbm_gamma_cfg[0];
		priv_info->flat_off_lhbm_gamma = lhbm_gamma_cfg[1];
	}

	return 0;
}

int mi_dsi_panel_parse_timing_fps_params(struct dsi_display_mode *mode,
				struct dsi_parser_utils *utils)
{
	int rc = 0;

	struct dsi_display_mode_priv_info *priv_info;

	if ( !mode || !mode->priv_info)
		return -EINVAL;

	priv_info = mode->priv_info;

	priv_info->mi_per_timing_fps_len = utils->count_u32_elems(utils->data, "mi,mdss-dsi-supported-fps-list");
	if (priv_info->mi_per_timing_fps_len <= 0 ) {
		DSI_ERR("not support dfps-list for the panel, rc = %d\n", rc);
		return 0;
	}

	priv_info->mi_per_timing_fps = kcalloc(priv_info->mi_per_timing_fps_len, sizeof(u32), GFP_KERNEL);
	if (!priv_info->mi_per_timing_fps)
		return -ENOMEM;

	rc = utils->read_u32_array(utils->data,
			"mi,mdss-dsi-supported-fps-list", priv_info->mi_per_timing_fps, priv_info->mi_per_timing_fps_len);
	if (rc) {
		DSI_ERR("unable to read mi,mdss-dsi-supported-fps-list, rc = %d\n", rc);
		return -EINVAL;
	}

	return rc;
}

int mi_dsi_panel_write_cmd_set(struct dsi_panel *panel,
				struct dsi_panel_cmd_set *cmd_sets)
{
	int rc = 0, i = 0;
	ssize_t len;
	struct dsi_cmd_desc *cmds;
	u32 count;
	enum dsi_cmd_set_state state;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	cmds = cmd_sets->cmds;
	count = cmd_sets->count;
	state = cmd_sets->state;

	if (count == 0) {
		DISP_DEBUG("[%s] No commands to be sent for state\n", panel->type);
		goto error;
	}

	for (i = 0; i < count; i++) {
		cmds->ctrl_flags = 0;

		if (state == DSI_CMD_SET_STATE_LP)
			cmds->msg.flags |= MIPI_DSI_MSG_USE_LPM;

		len = dsi_host_transfer_sub(panel->host, cmds);
		if (len < 0) {
			rc = len;
			DISP_ERROR("failed to set cmds, rc=%d\n", rc);
			goto error;
		}
		if (cmds->post_wait_ms)
			usleep_range(cmds->post_wait_ms * 1000,
					((cmds->post_wait_ms * 1000) + 10));
		cmds++;
	}
error:
	return rc;
}

int mi_dsi_panel_read_batch_number(struct dsi_panel *panel)
{
	int rc = 0;
	unsigned long mode_flags_backup = 0;
	u8 rdbuf[8];
	ssize_t read_len = 0;
	u8 read_batch_number = 0;

	int i = 0;
	struct panel_batch_info info[] = {
		{0x00, "P0.0"},
		{0x01, "P0.1"},
		{0x10, "P1.0"},
		{0x11, "P1.1"},
		{0x12, "P1.2"},
		{0x13, "P1.2"},
		{0x20, "P2.0"},
		{0x21, "P2.1"},
		{0x30, "MP"},
	};

	if (!panel || !panel->host) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	mutex_lock(&panel->panel_lock);

	mode_flags_backup = panel->mipi_device.mode_flags;
	panel->mipi_device.mode_flags |= MIPI_DSI_MODE_LPM;
	read_len = mipi_dsi_dcs_read(&panel->mipi_device, 0xDA, rdbuf, 1);
	panel->mipi_device.mode_flags = mode_flags_backup;
	if (read_len > 0) {
		read_batch_number = rdbuf[0];
		panel->mi_cfg.panel_batch_number = read_batch_number;
		for (i = 0; i < ARRAY_SIZE(info); i++) {
			if (read_batch_number == info[i].batch_number) {
				DISP_INFO("panel batch is %s\n", info[i].batch_name);
				break;
			}
		}
		rc = 0;
 		panel->mi_cfg.panel_batch_number_read_done = true;
	} else {
		DISP_ERROR("failed to read panel batch number\n");
		panel->mi_cfg.panel_batch_number = 0;
		rc = -EAGAIN;
		panel->mi_cfg.panel_batch_number_read_done = false;
	}

	mutex_unlock(&panel->panel_lock);
	return rc;
}

int mi_dsi_panel_write_dsi_cmd_set(struct dsi_panel *panel,
			int type)
{
	int rc = 0;
	int i = 0, j = 0;
	u8 *tx_buf = NULL;
	u8 *buffer = NULL;
	int buf_size = 1024;
	u32 cmd_count = 0;
	int buf_count = 1024;
	struct dsi_cmd_desc *cmds;
	enum dsi_cmd_set_state state;
	struct dsi_display_mode *mode;

	if (!panel || !panel->cur_mode || type < 0 || type >= DSI_CMD_SET_MAX) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	buffer = kzalloc(buf_size, GFP_KERNEL);
	if (!buffer) {
		return -ENOMEM;
	}

	mutex_lock(&panel->panel_lock);

	mode = panel->cur_mode;
	cmds = mode->priv_info->cmd_sets[type].cmds;
	cmd_count = mode->priv_info->cmd_sets[type].count;
	state = mode->priv_info->cmd_sets[type].state;

	if (cmd_count == 0) {
		DISP_ERROR("[%s] No commands to be sent\n", cmd_set_prop_map[type]);
		rc = -EAGAIN;
		goto error;
	}

	DISP_INFO("set cmds [%s], count (%d), state(%s)\n",
		cmd_set_prop_map[type], cmd_count,
		(state == DSI_CMD_SET_STATE_LP) ? "dsi_lp_mode" : "dsi_hs_mode");

	for (i = 0; i < cmd_count; i++) {
		memset(buffer, 0, buf_size);
		buf_count = snprintf(buffer, buf_size, "%02zX", cmds->msg.tx_len);
		tx_buf = (u8 *)cmds->msg.tx_buf;
		for (j = 0; j < cmds->msg.tx_len ; j++) {
			buf_count += snprintf(buffer + buf_count,
					buf_size - buf_count, " %02X", tx_buf[j]);
		}
		DISP_DEBUG("[%d] %s\n", i, buffer);
		cmds++;
	}

	rc = dsi_panel_tx_cmd_set(panel, type);

error:
	mutex_unlock(&panel->panel_lock);
	kfree(buffer);
	return rc;
}

ssize_t mi_dsi_panel_show_dsi_cmd_set_type(struct dsi_panel *panel,
			char *buf, size_t size)
{
	ssize_t count = 0;
	int type = 0;

	if (!panel || !buf) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	count = snprintf(buf, size, "%s: dsi cmd_set name\n", "id");

	for (type = DSI_CMD_SET_PRE_ON; type < DSI_CMD_SET_MAX; type++) {
		count += snprintf(buf + count, size - count, "%02d: %s\n",
				     type, cmd_set_prop_map[type]);
	}

	return count;
}

int mi_dsi_panel_update_doze_cmd_locked(struct dsi_panel *panel, u8 value)
{
	int rc = 0;
	struct dsi_display_mode *cur_mode = NULL;
	struct dsi_cmd_update_info *info = NULL;
	u32 cmd_update_index = 0;
	u32 cmd_update_count = 0;
	int i = 0;

	if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	cur_mode = panel->cur_mode;

	cmd_update_index = DSI_CMD_SET_MI_DOZE_LBM_UPDATE;
	info = cur_mode->priv_info->cmd_update[cmd_update_index];
	cmd_update_count = cur_mode->priv_info->cmd_update_count[cmd_update_index];
	for (i = 0; i < cmd_update_count; i++){
		DISP_INFO("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
			panel->type, cmd_set_prop_map[info->type],
			info->mipi_address, info->index, info->length);
		mi_dsi_panel_update_cmd_set(panel, cur_mode,
			DSI_CMD_SET_MI_DOZE_LBM, info,
			&value, sizeof(value));
		info++;
	}

	return rc;
}

int mi_dsi_panel_set_doze_brightness(struct dsi_panel *panel,
			u32 doze_brightness)
{
	int rc = 0;
	struct mi_dsi_panel_cfg *mi_cfg;

	mutex_lock(&panel->panel_lock);

	if (!dsi_panel_initialized(panel)) {
		DISP_ERROR("panel is not initialized!\n");
		rc = -EINVAL;
		goto exit;
	}

	mi_cfg = &panel->mi_cfg;

	if (is_hbm_fod_on(panel)) {
		mi_cfg->doze_brightness = doze_brightness;
		DISP_INFO("Skip! [%s] set doze brightness %d due to FOD_HBM_ON\n",
			panel->type, doze_brightness);
	} else if (panel->mi_cfg.aod_to_normal_statue == true || panel->cur_mode->timing.refresh_rate != 30) {
		mi_cfg->doze_brightness = doze_brightness;
		mi_dsi_update_backlight_in_aod(panel, false);
	} else if (panel->mi_cfg.panel_state == PANEL_STATE_ON
		|| mi_cfg->doze_brightness != doze_brightness) {
		/*
		if (doze_brightness == DOZE_BRIGHTNESS_HBM) {
			mi_cfg->last_doze_brightness = doze_brightness;
			panel->mi_cfg.panel_state = PANEL_STATE_DOZE_HIGH;
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM);
			if (rc) {
				DISP_ERROR("[%s] failed to send DOZE_HBM cmd, rc=%d\n",
					panel->type, rc);
			}
		} else if (doze_brightness == DOZE_BRIGHTNESS_LBM) {
			panel->mi_cfg.panel_state = PANEL_STATE_DOZE_LOW;
			mi_cfg->last_doze_brightness = doze_brightness;
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM);
			if (rc) {
				DISP_ERROR("[%s] failed to send DOZE_LBM cmd, rc=%d\n",
					panel->type, rc);
			}
		}
		*/
		mi_cfg->doze_brightness = doze_brightness;
		DISP_TIME_INFO("[%s] set doze brightness to %s\n",
			panel->type, get_doze_brightness_name(doze_brightness));
	} else {
		DISP_INFO("[%s] %s has been set, skip\n", panel->type,
			get_doze_brightness_name(doze_brightness));
	}

exit:
	mutex_unlock(&panel->panel_lock);

	return rc;
}

int mi_dsi_panel_get_doze_brightness(struct dsi_panel *panel,
			u32 *doze_brightness)
{
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	mi_cfg = &panel->mi_cfg;
	*doze_brightness =  mi_cfg->doze_brightness;

	mutex_unlock(&panel->panel_lock);

	return 0;
}

int mi_dsi_panel_get_brightness(struct dsi_panel *panel,
			u32 *brightness)
{
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	mi_cfg = &panel->mi_cfg;
	*brightness =  mi_cfg->last_bl_level;

	mutex_unlock(&panel->panel_lock);

	return 0;
}

int mi_dsi_panel_write_dsi_cmd(struct dsi_panel *panel,
			struct dsi_cmd_rw_ctl *ctl)
{
	struct dsi_panel_cmd_set cmd_sets = {0};
	u32 packet_count = 0;
	u32 dlen = 0;
	int rc = 0;

	mutex_lock(&panel->panel_lock);

	if (!panel || !panel->panel_initialized) {
		DISP_ERROR("Panel not initialized!\n");
		rc = -EAGAIN;
		goto exit_unlock;
	}

	if (!ctl->tx_len || !ctl->tx_ptr) {
		DISP_ERROR("[%s] invalid params\n", panel->type);
		rc = -EINVAL;
		goto exit_unlock;
	}

	rc = dsi_panel_get_cmd_pkt_count(ctl->tx_ptr, ctl->tx_len, &packet_count);
	if (rc) {
		DISP_ERROR("[%s] write dsi commands failed, rc=%d\n",
			panel->type, rc);
		goto exit_unlock;
	}

	DISP_DEBUG("[%s] packet-count=%d\n", panel->type, packet_count);

	rc = dsi_panel_alloc_cmd_packets(&cmd_sets, packet_count);
	if (rc) {
		DISP_ERROR("[%s] failed to allocate cmd packets, rc=%d\n",
			panel->type, rc);
		goto exit_unlock;
	}

	rc = dsi_panel_create_cmd_packets(ctl->tx_ptr, dlen, packet_count,
				cmd_sets.cmds);
	if (rc) {
		DISP_ERROR("[%s] failed to create cmd packets, rc=%d\n",
			panel->type, rc);
		goto exit_free1;
	}

	if (ctl->tx_state == MI_DSI_CMD_LP_STATE) {
		cmd_sets.state = DSI_CMD_SET_STATE_LP;
	} else if (ctl->tx_state == MI_DSI_CMD_HS_STATE) {
		cmd_sets.state = DSI_CMD_SET_STATE_HS;
	} else {
		DISP_ERROR("[%s] command state unrecognized\n",
			panel->type);
		goto exit_free1;
	}

	rc = mi_dsi_panel_write_cmd_set(panel, &cmd_sets);
	if (rc) {
		DISP_ERROR("[%s] failed to send cmds, rc=%d\n", panel->type, rc);
		goto exit_free2;
	}

exit_free2:
	if (ctl->tx_len && ctl->tx_ptr)
		dsi_panel_destroy_cmd_packets(&cmd_sets);
exit_free1:
	if (ctl->tx_len && ctl->tx_ptr)
		dsi_panel_dealloc_cmd_packets(&cmd_sets);
exit_unlock:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int mi_dsi_panel_set_brightness_clone(struct dsi_panel *panel,
			u32 brightness_clone)
{
	int rc = 0;
	struct mi_dsi_panel_cfg *mi_cfg;
	int disp_id = MI_DISP_PRIMARY;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	mi_cfg = &panel->mi_cfg;

	mi_cfg->user_brightness_clone = brightness_clone;
#if 0
	if (!mi_cfg->thermal_dimming_enabled) {
		if (brightness_clone > mi_cfg->thermal_max_brightness_clone)
			brightness_clone = mi_cfg->thermal_max_brightness_clone;
	}
#endif
	if (brightness_clone > mi_cfg->max_brightness_clone)
		brightness_clone = mi_cfg->max_brightness_clone;

	atomic_set(&mi_cfg->real_brightness_clone, brightness_clone);
	DISP_TIME_INFO("[%s] set brightness clone to %d\n",
			panel->type, atomic_read(&mi_cfg->real_brightness_clone));

	disp_id = mi_get_disp_id(panel->type);
	mi_disp_feature_event_notify_by_type(disp_id,
			MI_DISP_EVENT_BRIGHTNESS_CLONE,
			sizeof(mi_cfg->real_brightness_clone),
			atomic_read(&mi_cfg->real_brightness_clone));

	mutex_unlock(&panel->panel_lock);

	return rc;
}

int mi_dsi_panel_get_brightness_clone(struct dsi_panel *panel,
			u32 *brightness_clone)
{
	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	*brightness_clone = atomic_read(&panel->mi_cfg.real_brightness_clone);

	return 0;
}

int mi_dsi_panel_get_max_brightness_clone(struct dsi_panel *panel,
			u32 *max_brightness_clone)
{
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;
	*max_brightness_clone =  mi_cfg->max_brightness_clone;

	return 0;
}
int mi_dsi_panel_switch_page_locked(struct dsi_panel *panel, u8 page_index)
{
	int rc = 0;
	u8 tx_buf[5] = {0x55, 0xAA, 0x52, 0x08, page_index};
	struct dsi_display_mode_priv_info *priv_info;
	struct dsi_cmd_update_info *info = NULL;
	u32 cmd_update_index = 0;
	u32 cmd_update_count = 0;
	int i = 0;

	if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info) {
		DISP_ERROR("[%s] invalid params or not initialized\n", panel->type);
		return -EINVAL;
	}

	if (!panel->panel_initialized) {
		DISP_ERROR("[%s] panel not initialized\n", panel->type);
		return -EINVAL;
	}

	priv_info = panel->cur_mode->priv_info;
	cmd_update_index = DSI_CMD_SET_MI_SWITCH_PAGE_UPDATE;
	info = priv_info->cmd_update[cmd_update_index];
	cmd_update_count = priv_info->cmd_update_count[cmd_update_index];
	for (i = 0; i < cmd_update_count; i++){
		DISP_DEBUG("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
			panel->type, cmd_set_prop_map[info->type],
			info->mipi_address, info->index, info->length);
		mi_dsi_panel_update_cmd_set(panel, panel->cur_mode,
			DSI_CMD_SET_MI_SWITCH_PAGE, info,
			tx_buf, sizeof(tx_buf));
		info++;
	}

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_SWITCH_PAGE);
	if (rc) {
		DISP_ERROR("[%s] failed to send switch page(%d) cmd\n",
			panel->type, page_index);
	}

	return rc;
}

int mi_dsi_panel_switch_page(struct dsi_panel *panel, u8 page_index)
{
	int rc = 0;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	rc = mi_dsi_panel_switch_page_locked(panel, page_index);

	mutex_unlock(&panel->panel_lock);

	return rc;
}

int mi_dsi_panel_read_and_update_flat_param(struct dsi_panel *panel)
{
	int ret = 0;
	mutex_lock(&panel->panel_lock);
	ret = mi_dsi_panel_read_and_update_flat_param_locked(panel);
	mutex_unlock(&panel->panel_lock);
	return ret;
}


int mi_dsi_panel_read_and_update_flat_param_locked(struct dsi_panel *panel)
{

	int rc = 0;
	struct flat_mode_cfg *flat_cfg = NULL;
	struct dsi_display *display;
	u32 num_display_modes;
	struct dsi_display_mode *mode;
	struct dsi_display_mode_priv_info *priv_info;
	struct dsi_cmd_update_info *info = NULL;
	u32 cmd_update_index = 0;
	u32 cmd_update_count = 0;
	u8 rdbuf[10];
	uint request_rx_len = 0;
	unsigned long mode_flags_backup = 0;
	ssize_t read_len = 0;
	int i = 0, j = 0;

	if (!panel || !panel->host) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (!panel->mi_cfg.flat_update_flag) {
		DISP_DEBUG("[%s] flat_update_flag is not configed\n", panel->type);
		return 0;
	}

	flat_cfg = &panel->mi_cfg.flat_cfg;
	if (flat_cfg->update_done) {
		DISP_DEBUG("flat param already updated\n");
		rc = 0;
		goto exit;
	}

	DISP_INFO("[%s] flat param update start\n", panel->type);

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_PREPARE_READ_FLAT);
	if (rc) {
		DISP_ERROR("[%s] failed to send PREPARE_READ_FLAT cmd\n",
			panel->type);
		goto exit;
	}

	if (sizeof(rdbuf) >= sizeof(flat_cfg->flat_on_data) ) {
		request_rx_len = sizeof(flat_cfg->flat_on_data);
	} else {
		DISP_ERROR("please check 0xB8 read_buf size, must > or = %lu\n",
			sizeof(flat_cfg->flat_on_data));
		rc = -EAGAIN;
		goto exit;
	}

	mode_flags_backup = panel->mipi_device.mode_flags;
	panel->mipi_device.mode_flags |= MIPI_DSI_MODE_LPM;
	read_len = mipi_dsi_dcs_read(&panel->mipi_device, 0xB8,
		rdbuf, request_rx_len);
	panel->mipi_device.mode_flags = mode_flags_backup;

	if (read_len == request_rx_len) {
		memcpy(flat_cfg->flat_on_data, rdbuf, read_len);
		for (i = 0; i < sizeof(flat_cfg->flat_on_data); i++)
			DISP_INFO("flat read 0xB8, flat_on_data[%d] = 0x%02X\n",
				i, flat_cfg->flat_on_data[i]);
	} else {
		DISP_INFO("read flat 0xB8 failed (%zd)\n", read_len);
		rc = -EAGAIN;
		goto exit;
	}

	num_display_modes = panel->num_display_modes;
	display = to_dsi_display(panel->host);
	if (!display || !display->modes) {
		DISP_ERROR("invalid display or display->modes ptr\n");
		rc = -EINVAL;
		goto exit;
	}

	for (i = 0; i < num_display_modes; i++) {
		mode = &display->modes[i];
		DISP_INFO("[%s] update %d fps flat cmd\n", panel->type,
				mode->timing.refresh_rate);

		priv_info = mode->priv_info;
		cmd_update_index = DSI_CMD_SET_MI_FLAT_MODE_ON_UPDATE;
		info = priv_info->cmd_update[cmd_update_index];
		cmd_update_count = priv_info->cmd_update_count[cmd_update_index];
		for (j = 0; j < cmd_update_count; j++){
			DISP_DEBUG("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
				panel->type, cmd_set_prop_map[info->type],
				info->mipi_address, info->index, info->length);
			mi_dsi_panel_update_cmd_set(panel, mode,
				DSI_CMD_SET_MI_FLAT_MODE_ON, info,
				flat_cfg->flat_on_data, sizeof(flat_cfg->flat_on_data));
			info++;
		}
	}

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_PREPARE_READ_FLAT_OFF);
	if (rc) {
		DISP_ERROR("[%s] failed to send PREPARE_READ_FLAT_OFF cmd\n",
			panel->type);
		goto exit;
	}

	if (sizeof(rdbuf) >= sizeof(flat_cfg->flat_off_data) ) {
		request_rx_len = sizeof(flat_cfg->flat_off_data);
	} else {
		DISP_ERROR("please check 0xB8 read_buf size, must > or = %lu\n",
			sizeof(flat_cfg->flat_off_data));
		rc = -EAGAIN;
		goto exit;
	}

	mode_flags_backup = panel->mipi_device.mode_flags;
	panel->mipi_device.mode_flags |= MIPI_DSI_MODE_LPM;
	read_len = mipi_dsi_dcs_read(&panel->mipi_device, 0xB8,
		rdbuf, request_rx_len);
	panel->mipi_device.mode_flags = mode_flags_backup;

	if (read_len == request_rx_len) {
		memcpy(flat_cfg->flat_off_data, rdbuf, read_len);
		for (i = 0; i < sizeof(flat_cfg->flat_off_data); i++)
			DISP_INFO("flat off read 0xB8, flat_off_data[%d] = 0x%02X\n",
				i, flat_cfg->flat_off_data[i]);
	} else {
		DISP_INFO("read flat off 0xB8 failed (%zd)\n", read_len);
		rc = -EAGAIN;
		goto exit;
	}

	for (i = 0; i < num_display_modes; i++) {
		mode = &display->modes[i];
		DISP_INFO("[%s] update %d fps flat off cmd\n", panel->type,
				mode->timing.refresh_rate);

		priv_info = mode->priv_info;
		cmd_update_index = DSI_CMD_SET_MI_FLAT_MODE_OFF_UPDATE;
		info = priv_info->cmd_update[cmd_update_index];
		cmd_update_count = priv_info->cmd_update_count[cmd_update_index];
		for (j = 0; j < cmd_update_count; j++){
			DISP_DEBUG("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
				panel->type, cmd_set_prop_map[info->type],
				info->mipi_address, info->index, info->length);
			mi_dsi_panel_update_cmd_set(panel, mode,
				DSI_CMD_SET_MI_FLAT_MODE_OFF, info,
				flat_cfg->flat_off_data, sizeof(flat_cfg->flat_off_data));
			info++;
		}
	}

exit:

	return rc;
}

void mi_dsi_update_backlight_in_aod(struct dsi_panel *panel, bool restore_backlight)
{
	int bl_lvl = 0;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;
	struct mipi_dsi_device *dsi = &panel->mipi_device;

	if (restore_backlight) {
		bl_lvl = mi_cfg->last_bl_level;
	} else {
		switch (mi_cfg->doze_brightness) {
			case DOZE_BRIGHTNESS_HBM:
				bl_lvl = mi_cfg->doze_hbm_dbv_level;
				break;
			case DOZE_BRIGHTNESS_LBM:
				bl_lvl = mi_cfg->doze_lbm_dbv_level;
				break;
			default:
				return;
		}
	}

	if(panel->mi_cfg.aod_status || panel->power_mode == SDE_MODE_DPMS_LP2) {
		bl_lvl = panel->mi_cfg.last_bl_level;
		panel->mi_cfg.aod_status = false;
	}

	DISP_INFO("[%s] mi_dsi_update_backlight_in_aod bl_lvl=%d\n",
			panel->type, bl_lvl);
	if (panel->bl_config.bl_inverted_dbv)
		bl_lvl = (((bl_lvl & 0xff) << 8) | (bl_lvl >> 8));
	mipi_dsi_dcs_set_display_brightness(dsi, bl_lvl);

	return;
}

static int mi_dsi_update_lhbm_cmd_87reg(struct dsi_panel *panel,
			enum dsi_cmd_set_type type, int bl_lvl)
{
	struct dsi_display_mode_priv_info *priv_info = NULL;
	struct dsi_cmd_update_info *info = NULL;
	u32 cmd_update_index = 0;
	u32 cmd_update_count = 0;
	u8 alpha_buf[2] = {0};
	u8 lhbm_cmd_reg[4] = {0x11, 0xA0, 0x00, 0x00};
	int j = 0;

	if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (!panel->mi_cfg.lhbm_alpha_ctrlaa) {
		DISP_DEBUG("local hbm can't use alpha control AA area brightness\n");
		return 0;
	}

	if (mi_get_panel_id(panel->mi_cfg.mi_panel_id) == P17_PANEL_PA) {
		if (bl_lvl > 276) {
			lhbm_cmd_reg[1] = 0xAF;
			lhbm_cmd_reg[2] = 0xFF;
		} else {
			lhbm_cmd_reg[1] = lhbm_cmd_reg[1] + (aa_alpha_P17_PANEL_PA[bl_lvl] >> 8) & 0xFF;
			lhbm_cmd_reg[2] = aa_alpha_P17_PANEL_PA[bl_lvl] & 0xFF;
		}
		DISP_INFO("bl_lvl = %d, lhbm_cmd_reg[1] = 0x%2X, lhbm_cmd_reg[2] = 0x%2X\n",
				bl_lvl, lhbm_cmd_reg[1], lhbm_cmd_reg[2]);
	} else {
		DISP_INFO("[%s] bl_lvl = %d, alpha = %d\n",
				panel->type, bl_lvl,  aa_alpha_set[bl_lvl]);
		alpha_buf[0] = (aa_alpha_set[bl_lvl] >> 8) & 0x0f;
		alpha_buf[1] = aa_alpha_set[bl_lvl] & 0xff;
	}

	priv_info = panel->cur_mode->priv_info;

	switch (type) {
	case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_GREEN_500NIT:
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_GREEN_500NIT_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT:
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT:
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT:
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT:
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT_UPDATE;
		break;
	default :
		DISP_ERROR("[%s] unsupport cmd %s\n",
				panel->type, cmd_set_prop_map[type]);
		return -EINVAL;
	}

	info = priv_info->cmd_update[cmd_update_index];
	cmd_update_count = priv_info->cmd_update_count[cmd_update_index];
	for (j = 0; j < cmd_update_count; j++) {
		DISP_DEBUG("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
			panel->type, cmd_set_prop_map[info->type],
			info->mipi_address, info->index, info->length);
		if (mi_get_panel_id(panel->mi_cfg.mi_panel_id) == P17_PANEL_PA) {
			if (info && (info->mipi_address == 0x97)) {
				mi_dsi_panel_update_cmd_set(panel, panel->cur_mode,
						type, info, lhbm_cmd_reg, info->length);
				break;
			} else {
				DISP_ERROR("[%s] error mipi address (0x%02X)\n", panel->type, info->mipi_address);
				info++;
				continue;
			}
		} else {
			if (info && info->mipi_address != 0x87) {
				DISP_ERROR("[%s] error mipi address (0x%02X)\n", panel->type, info->mipi_address);
				info++;
				continue;
			} else {
				mi_dsi_panel_update_cmd_set(panel, panel->cur_mode,
						type, info, alpha_buf, info->length);
				break;
			}
		}
	}

	return 0;
}

static int mi_dsi_update_lhbm_cmd_DF_reg(struct dsi_panel *panel,
			enum dsi_cmd_set_type type, int bl_lvl)
{
	struct dsi_display_mode_priv_info *priv_info = NULL;
	struct dsi_cmd_update_info *info = NULL;
	u32 cmd_update_index = 0;
	u32 cmd_update_count = 0;
	u8 df_reg_buf[2] = {0};
	int j = 0;

	if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (!panel->mi_cfg.lhbm_ctrl_df_reg) {
		DISP_DEBUG("local hbm can't use control DF reg\n");
		return 0;
	}

	if(bl_lvl > 327) {
		df_reg_buf[0] = ((bl_lvl * 4) >> 8) & 0xFF;
		df_reg_buf[1] = (bl_lvl * 4) & 0xFF;
	}else {
		df_reg_buf[0] = 0x05;
		df_reg_buf[1] = 0x1C;
	}
	DISP_INFO("[%s] bl_lvl = %d, df reg = 0x%x 0x%x\n",
			panel->type, bl_lvl, df_reg_buf[0], df_reg_buf[1]);


	priv_info = panel->cur_mode->priv_info;
	switch (type) {
	case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_GREEN_500NIT:
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_GREEN_500NIT_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT:
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT:
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT_UPDATE;
		break;
	default :
		DISP_ERROR("[%s] unsupport cmd %s\n",
				panel->type, cmd_set_prop_map[type]);
		return -EINVAL;
	}

	info = priv_info->cmd_update[cmd_update_index];
	cmd_update_count = priv_info->cmd_update_count[cmd_update_index];
	for (j = 0; j < cmd_update_count; j++) {
		DISP_INFO("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
			panel->type, cmd_set_prop_map[info->type],
			info->mipi_address, info->index, info->length);
		if (info && info->mipi_address != 0xDF) {
			DISP_ERROR("[%s] error mipi address (0x%02X)\n", panel->type, info->mipi_address);
			info++;
			continue;
		} else {
			mi_dsi_panel_update_cmd_set(panel, panel->cur_mode,
					type, info, df_reg_buf, sizeof(df_reg_buf));
			break;
		}
	}

	return 0;
}


int mi_dsi_update_nolp_cmd_B2reg(struct dsi_panel *panel,
			enum dsi_cmd_set_type type)
{
	struct dsi_display_mode_priv_info *priv_info = NULL;
	struct dsi_cmd_update_info *info = NULL;
	u32 cmd_update_index = 0;
	u8 set_buf[1] = {0};

	panel->mi_cfg.panel_state = PANEL_STATE_ON;

	if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	priv_info = panel->cur_mode->priv_info;

	if (panel->mi_cfg.feature_val[DISP_FEATURE_DC] == FEATURE_OFF)
		set_buf[0] = 0x18;
	else
		set_buf[0] = 0x98;

	switch (type) {
	case DSI_CMD_SET_NOLP:
		cmd_update_index = DSI_CMD_SET_NOLP_UPDATE;
		break;
	case DSI_CMD_SET_MI_DOZE_HBM_NOLP:
		cmd_update_index = DSI_CMD_SET_MI_DOZE_HBM_NOLP_UPDATE;
		break;
	case DSI_CMD_SET_MI_DOZE_LBM_NOLP:
		cmd_update_index = DSI_CMD_SET_MI_DOZE_LBM_NOLP_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT:
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT_B2_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT:
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT_B2_UPDATE;
		break;
	default :
		DISP_ERROR("[%s] unsupport cmd %s\n",
				panel->type, cmd_set_prop_map[type]);
		return -EINVAL;
	}

	info = priv_info->cmd_update[cmd_update_index];
	if (info && info->mipi_address != 0xB2) {
		DISP_ERROR("error mipi address (0x%02X)\n", info->mipi_address);
		return -EINVAL;
	}
	mi_dsi_panel_update_cmd_set(panel, panel->cur_mode,
			type, info, set_buf, sizeof(set_buf));

	return 0;
}

int mi_dsi_panel_read_and_update_doze_param(struct dsi_panel *panel)
{
	int ret = 0;
	mutex_lock(&panel->panel_lock);
	ret = mi_dsi_panel_read_and_update_doze_param_locked(panel);
	mutex_unlock(&panel->panel_lock);
	return ret;
}

int mi_dsi_panel_read_and_update_doze_param_locked(struct dsi_panel *panel)
{
	int rc = 0;
	int i = 0, j = 0;
	int rdlength = 10;
	u8 r_buf[10] = {0};
	u8 r_buf_doze_hbm[11] = {0};
	u8 r_buf_doze_lbm[11] = {0};
	u8 diff[11] = {0x08, 0x03, 0x09, 0x0E, 0x0B, 0x04, 0x0F, 0x08, 0x02, 0x15, 0x11};
	u32 num_display_modes;
	struct dsi_display *display;
	struct dsi_display_mode *mode;
	struct dsi_display_mode_priv_info *priv_info;
	struct dsi_cmd_update_info *info = NULL;
	u32 cmd_update_index = 0;
	u32 cmd_update_count = 0;

	if (!panel || !panel->host) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (!panel->panel_initialized) {
		DISP_ERROR("[%s] Panel not initialized\n", panel->type);
		rc = -EINVAL;
		goto exit;
	}

	DISP_INFO("[%s] DOZE param read cmd start\n", panel->type);

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_PARAM_READ);
	if (rc) {
		DISP_ERROR("[%s] failed to send DOZE_PARAM_READ cmds, rc=%d\n",
			panel->type, rc);
		rc = -EINVAL;
		goto exit;
	}
	rc = mipi_dsi_dcs_read(&panel->mipi_device, 0xBB, r_buf, rdlength);
	if (rc < 0) {
		DISP_ERROR("[%s] read back doze param failed, rc=%d\n", panel->type, rc);
		goto exit;
	}
	if (rc != rdlength) {
		DISP_INFO("read doze param BB failed (%d)\n", rc);
		rc = -EAGAIN;
		goto exit;
	}
	for(i = 0; i < 10; i++) {
		r_buf_doze_hbm[i] = r_buf[i];
	}

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_PARAM_READ_END);
	if (rc) {
		DISP_ERROR("[%s] failed to send DOZE_PARAM_READ cmds, rc=%d\n",
			panel->type, rc);
		rc = -EINVAL;
		goto exit;
	}
	rc = mipi_dsi_dcs_read(&panel->mipi_device, 0xBB, r_buf, rdlength);
	if (rc < 0) {
		DISP_ERROR("[%s] read back doze param failed, rc=%d\n", panel->type, rc);
		goto exit;
	}
	if (rc != rdlength) {
		DISP_INFO("read doze param BB failed (%d)\n", rc);
		rc = -EAGAIN;
		goto exit;
	}
	r_buf_doze_hbm[10] = r_buf[9];


	for(i = 0; i < 11; i++) {
		if (i == 5) {
			r_buf_doze_lbm[i] = r_buf_doze_hbm[i] - 4;
			continue;
		}
		r_buf_doze_lbm[i] = r_buf_doze_hbm[i] + diff[i];
		DISP_INFO("r_buf_doze_hbm is 0x%02X, r_buf_doze_lbm is 0x%02X",r_buf_doze_hbm[i], r_buf_doze_lbm[i]);
	}

	num_display_modes = panel->num_display_modes;
	display = to_dsi_display(panel->host);
	if (!display || !display->modes) {
		DISP_ERROR("invalid display or display->modes ptr\n");
		rc = -EINVAL;
		goto exit;
	}

	for (i = 0; i < num_display_modes; i++) {
		mode = &display->modes[i];
		DISP_INFO("[%s] update %d fps doze cmd\n", panel->type,
					mode->timing.refresh_rate);

		priv_info = mode->priv_info;
		cmd_update_index = DSI_CMD_SET_MI_DOZE_HBM_UPDATE;
		info = priv_info->cmd_update[cmd_update_index];
		cmd_update_count = priv_info->cmd_update_count[cmd_update_index];
		for (j = 0; j < cmd_update_count; j++){
			DISP_DEBUG("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
				panel->type, cmd_set_prop_map[info->type],
					info->mipi_address, info->index, info->length);
			mi_dsi_panel_update_cmd_set(panel, mode,
					DSI_CMD_SET_MI_DOZE_HBM, info,
					r_buf_doze_hbm, sizeof(r_buf_doze_hbm));
			info++;
		}

		cmd_update_index = DSI_CMD_SET_MI_DOZE_LBM_UPDATE;
		info = priv_info->cmd_update[cmd_update_index];
		cmd_update_count = priv_info->cmd_update_count[cmd_update_index];
		for (j = 0; j < cmd_update_count; j++){
			DISP_DEBUG("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
				panel->type, cmd_set_prop_map[info->type],
					info->mipi_address, info->index, info->length);
			mi_dsi_panel_update_cmd_set(panel, mode,
					DSI_CMD_SET_MI_DOZE_LBM, info,
					r_buf_doze_lbm, sizeof(r_buf_doze_lbm));
			info++;
		}
	}
	DISP_INFO("[%s] doze param read back and update end\n", panel->type);
exit:
	return rc;
}

int mi_dsi_update_51_mipi_cmd(struct dsi_panel *panel,
			enum dsi_cmd_set_type type, int bl_lvl)
{
	struct dsi_display_mode *cur_mode = NULL;
	struct dsi_cmd_update_info *info = NULL;
	u32 cmd_update_index = 0;
	u32 cmd_update_count = 0;
	u8 bl_buf[6] = {0};
	int j = 0;
	int size = 2;

	if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	cur_mode = panel->cur_mode;

	DISP_INFO("[%s] bl_lvl = %d\n", panel->type, bl_lvl);

	bl_buf[0] = (bl_lvl >> 8) & 0xff;
	bl_buf[1] = bl_lvl & 0xff;
	size = 2;

	switch (type) {
	case DSI_CMD_SET_NOLP:
		cmd_update_index = DSI_CMD_SET_NOLP_UPDATE;
		break;
	case DSI_CMD_SET_MI_HBM_ON:
		cmd_update_index = DSI_CMD_SET_MI_HBM_ON_UPDATE;
		break;
	case DSI_CMD_SET_MI_HBM_OFF:
		cmd_update_index = DSI_CMD_SET_MI_HBM_OFF_UPDATE;
		break;
	case DSI_CMD_SET_MI_DOZE_HBM:
		cmd_update_index = DSI_CMD_SET_MI_DOZE_HBM_UPDATE;
		break;
	case DSI_CMD_SET_MI_DOZE_LBM:
		cmd_update_index = DSI_CMD_SET_MI_DOZE_LBM_UPDATE;
		break;
	case DSI_CMD_SET_MI_HBM_FOD_ON:
		cmd_update_index = DSI_CMD_SET_MI_HBM_FOD_ON_UPDATE;
		break;
	case DSI_CMD_SET_MI_HBM_FOD_OFF:
		cmd_update_index = DSI_CMD_SET_MI_HBM_FOD_OFF_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT:
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT:
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT:
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_HBM:
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_HBM_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL:
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL_UPDATE;
		break;
	case DSI_CMD_SET_UHBM_ENTER:
		cmd_update_index = DSI_CMD_SET_UHBM_ENTER_UPDATE;
		break;
	default:
		DISP_ERROR("[%s] unsupport cmd %s\n",
				panel->type, cmd_set_prop_map[type]);
		return -EINVAL;
	}

	info = cur_mode->priv_info->cmd_update[cmd_update_index];
	cmd_update_count = cur_mode->priv_info->cmd_update_count[cmd_update_index];
	for (j = 0; j < cmd_update_count; j++) {
		DISP_DEBUG("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
			panel->type, cmd_set_prop_map[info->type],
			info->mipi_address, info->index, info->length);
		if (info && info->mipi_address != 0x51) {
			DISP_DEBUG("[%s] error mipi address (0x%02X)\n", panel->type, info->mipi_address);
			info++;
			continue;
		} else {
			mi_dsi_panel_update_cmd_set(panel, cur_mode,
					type, info, bl_buf, size);
			break;
		}
	}

	return 0;
}

/* Note: Factory version need flat cmd send out immediately,
 * do not care it may lead panel flash.
 * Dev version need flat cmd send out send with te
 */
static int mi_dsi_send_flat_sync_with_te_locked(struct dsi_panel *panel,
			bool enable)
{
	int rc = 0;

#ifdef CONFIG_FACTORY_BUILD
	if (enable) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_ON);
		rc |= dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_SEC_ON);
	} else {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_OFF);
		rc |= dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_SEC_OFF);
	}
	DISP_INFO("send flat %s cmd immediately", enable ? "ON" : "OFF");
#else
	/*flat cmd will be send out at mi_sde_connector_flat_fence*/
	DISP_DEBUG("flat cmd should send out sync with te");
#endif
	return rc;
}

int mi_dsi_panel_set_dbi_by_temp(struct dsi_panel *panel, int temp_val)
{
	int rc = 0;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);
	//rc = mi_dsi_panel_set_dbi_by_temp_locked(panel, temp_val);

	mutex_unlock(&panel->panel_lock);

	return rc;
}


static int mi_dsi_panel_update_lhbm_white_gamma_param(struct dsi_panel * panel,enum dsi_cmd_set_type type)
{
	int rc=0;
	struct dsi_display_mode *mode;
	struct dsi_cmd_update_info *info = NULL;
	u32 cmd_update_index = 0;
	u32 cmd_update_count = 0;
	int i= 0, j = 0, level = 1000;
	u32 sum_1200nits = 0, sum_250nits = 0;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (!panel->mi_cfg.lhbm_w1000_update_flag && !panel->mi_cfg.lhbm_w110_update_flag) {
		DISP_DEBUG("[%s] don't need update white rgb config\n", panel->type);
		return 0;
	}

	switch (type) {
	case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT:
		level = 1000;
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT:
		level = 1000;
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT:
		level = 110;
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT:
		level = 110;
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT_UPDATE;
		break;
	default:
		DISP_ERROR("[%s] unsupport cmd %s\n", panel->type, cmd_set_prop_map[type]);
		return -EINVAL;
	}

	mode = panel->cur_mode;
	info = mode->priv_info->cmd_update[cmd_update_index];
	cmd_update_count = mode->priv_info->cmd_update_count[cmd_update_index];
	for ( i= 0; i < sizeof(panel->mi_cfg.lhbm_1200nits_gamma) - 2; i++){
		sum_1200nits += panel->mi_cfg.lhbm_1200nits_gamma[i];
	}
	DISP_DEBUG("sum_1200nits=%x",sum_1200nits);
	for ( i= 0; i < sizeof(panel->mi_cfg.lhbm_250nits_gamma) - 2; i++){
		sum_250nits += panel->mi_cfg.lhbm_250nits_gamma[i];
	}
	DISP_DEBUG("sum_250nits=%x",sum_250nits);
	for (j = 0; j < cmd_update_count; j++) {
		DISP_DEBUG("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
			panel->type, cmd_set_prop_map[info->type],
			info->mipi_address, info->index, info->length);
		if (info && info->mipi_address != 0xBE) {
			DISP_ERROR("error mipi address (0x%02X)\n", info->mipi_address);
			info++;
			continue;
		} else {
			if (level == 1000) {
				if (!sum_1200nits)
					return -EINVAL;
				if( (sum_1200nits & 0xFFFF) == (panel->mi_cfg.lhbm_1200nits_gamma[17] << 8 | panel->mi_cfg.lhbm_1200nits_gamma[18]))
				{
					mi_dsi_panel_update_cmd_set(panel, mode, type, info,
						panel->mi_cfg.lhbm_1200nits_gamma, sizeof(panel->mi_cfg.lhbm_1200nits_gamma) - 2);
				}
			} else {
				if (!sum_250nits)
					return -EINVAL;
				if( (sum_250nits & 0xFFFF) == (panel->mi_cfg.lhbm_250nits_gamma[17] << 8 |panel->mi_cfg.lhbm_250nits_gamma[18]))
				{
					mi_dsi_panel_update_cmd_set(panel, mode, type, info,
						panel->mi_cfg.lhbm_250nits_gamma, sizeof(panel->mi_cfg.lhbm_250nits_gamma) - 2);
				}
			}
		}
	}

	return rc;
}

int mi_dsi_panel_read_lhbm_white_param(struct dsi_panel *panel)
{
	int ret = 0;
	mutex_lock(&panel->panel_lock);
	ret = mi_dsi_panel_read_lhbm_white_param_locked(panel);
	mutex_unlock(&panel->panel_lock);
	return ret;
}

int mi_dsi_panel_read_lhbm_white_param_locked(struct dsi_panel *panel)
{
	int rc = 0;
	u8 r_buf[20] = {0};
	u8 g_buf[20] = {0};
	u8 b_buf[20] = {0};
	int rdlength = 0;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;
	int j=0;

	if (!panel || !panel->host) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (!mi_cfg->lhbm_w1000_update_flag && !mi_cfg->lhbm_w110_update_flag) {
		DISP_DEBUG("[%s] don't need read back white rgb config\n", panel->type);
		return 0;
	}

	if (!panel->panel_initialized) {
		DISP_ERROR("[%s] Panel not initialized\n", panel->type);
		rc = -EINVAL;
		goto exit;
	}

	if (mi_cfg->uefi_read_lhbm_success ||
		(mi_cfg->lhbm_w1000_readbackdone && mi_cfg->lhbm_w110_readbackdone)) {
		DISP_DEBUG("lhbm white rgb read back done\n");
		rc = 0;
		goto exit;
	}

	DISP_INFO("[%s] LHBM white read cmd start\n", panel->type);

	if (mi_cfg->lhbm_w1000_update_flag) {
		rdlength = 10;
		rc = dsi_panel_tx_cmd_set(panel,
			DSI_CMD_SET_MI_LOCAL_HBM_WHITE_1000NIT_GIRON_PRE_READ);
		if (rc) {
			DISP_ERROR("[%s] failed to send HBM_WHITE_1000NIT_GIRON_PRE_READ cmds, rc=%d\n",
				panel->type, rc);
			rc = -EINVAL;
			goto exit;
		}

		rc = mipi_dsi_dcs_read(&panel->mipi_device, 0xB2, r_buf, rdlength);
		if (rc < 0) {
			DISP_ERROR("[%s] read back w1000_on red param failed, rc=%d\n", panel->type, rc);
			goto exit;
		}

		rc = mipi_dsi_dcs_read(&panel->mipi_device, 0xB5, g_buf, rdlength);
		if (rc < 0) {
			DISP_ERROR("[%s] read back w1000_on green param failed, rc=%d\n", panel->type, rc);
			goto exit;
		}

		rc = mipi_dsi_dcs_read(&panel->mipi_device, 0xB8, b_buf, rdlength);
		if (rc < 0) {
			DISP_ERROR("[%s] read back w1000_on blue param failed, rc=%d\n", panel->type, rc);
			goto exit;
		}

		mi_cfg->whitebuf_1000_gir_on[0] = r_buf[8];
		mi_cfg->whitebuf_1000_gir_on[1] = r_buf[9];
		mi_cfg->whitebuf_1000_gir_on[2] = g_buf[8];
		mi_cfg->whitebuf_1000_gir_on[3] = g_buf[9];
		mi_cfg->whitebuf_1000_gir_on[4] = b_buf[8];
		mi_cfg->whitebuf_1000_gir_on[5] = b_buf[9];

		memset(r_buf, 0, sizeof(r_buf));
		memset(g_buf, 0, sizeof(g_buf));
		memset(b_buf, 0, sizeof(b_buf));

		rc = dsi_panel_tx_cmd_set(panel,
			DSI_CMD_SET_MI_LOCAL_HBM_WHITE_1000NIT_GIROFF_PRE_READ);
		if (rc) {
			DISP_ERROR("[%s] failed to send HBM_WHITE_1000NIT_GIROFF_PRE_READ cmds, rc=%d\n",
				panel->type, rc);
			rc = -EINVAL;
			goto exit;
		}

		rc = mipi_dsi_dcs_read(&panel->mipi_device, 0xB2, r_buf, rdlength);
		if (rc < 0) {
			DISP_ERROR("[%s] read back w1000_off red param failed, rc=%d\n", panel->type, rc);
			goto exit;
		}

		rc = mipi_dsi_dcs_read(&panel->mipi_device, 0xB5, g_buf, rdlength);
		if (rc < 0) {
			DISP_ERROR("[%s] read back w1000_off green param failed, rc=%d\n", panel->type, rc);
			goto exit;
		}

		rc = mipi_dsi_dcs_read(&panel->mipi_device, 0xB8, b_buf, rdlength);
		if (rc < 0) {
			DISP_ERROR("[%s] read back w1000_off blue param failed, rc=%d\n", panel->type, rc);
			goto exit;
		}

		mi_cfg->whitebuf_1000_gir_off[0] =r_buf[8];
		mi_cfg->whitebuf_1000_gir_off[1] =r_buf[9];
		mi_cfg->whitebuf_1000_gir_off[2] =g_buf[8];
		mi_cfg->whitebuf_1000_gir_off[3] =g_buf[9];
		mi_cfg->whitebuf_1000_gir_off[4] =b_buf[8];
		mi_cfg->whitebuf_1000_gir_off[5] =b_buf[9];

		mi_cfg->lhbm_w1000_readbackdone = true;

		for (j = 0; j < sizeof(mi_cfg->whitebuf_1000_gir_on); j++)
			DISP_INFO("read from 0xB2,0xB5,0xB8 whitebuf_1000_gir_on[%d] = 0x%02X\n", j, mi_cfg->whitebuf_1000_gir_on[j]);

		for (j = 0; j < sizeof(mi_cfg->whitebuf_1000_gir_off); j++)
			DISP_INFO("read from 0xB2,0xB5, 0xB8,whitebuf_1000_gir_off[%d] = 0x%02X\n", j, mi_cfg->whitebuf_1000_gir_off[j]);
	} else {
		mi_cfg->lhbm_w1000_readbackdone = true;
	}

	if (mi_cfg->lhbm_w110_update_flag) {
		rdlength = 4;

		rc = dsi_panel_tx_cmd_set(panel,
			DSI_CMD_SET_MI_LOCAL_HBM_WHITE_110NIT_GIRON_PRE_READ);
		if (rc) {
			DISP_ERROR("[%s] failed to send HBM_WHITE_110NIT_GIRON_PRE_READ cmds, rc=%d\n",
				panel->type, rc);
			rc = -EINVAL;
			goto exit;
		}

		rc = mipi_dsi_dcs_read(&panel->mipi_device, 0xB2, r_buf, rdlength);
		if (rc < 0) {
			DISP_ERROR("[%s] read back w110_on red param failed, rc=%d\n", panel->type, rc);
			goto exit;
		}

		rc = mipi_dsi_dcs_read(&panel->mipi_device, 0xB5, g_buf, rdlength);
		if (rc < 0) {
			DISP_ERROR("[%s] read back w110_on green param failed, rc=%d\n", panel->type, rc);
			goto exit;
		}

		rc = mipi_dsi_dcs_read(&panel->mipi_device, 0xB8, b_buf, rdlength);
		if (rc < 0) {
			DISP_ERROR("[%s] read back w110_on blue param failed, rc=%d\n", panel->type, rc);
			goto exit;
		}

		mi_cfg->whitebuf_110_gir_on[0] = r_buf[2];
		mi_cfg->whitebuf_110_gir_on[1] = r_buf[3];
		mi_cfg->whitebuf_110_gir_on[2] = g_buf[2];
		mi_cfg->whitebuf_110_gir_on[3] = g_buf[3];
		mi_cfg->whitebuf_110_gir_on[4] = b_buf[2];
		mi_cfg->whitebuf_110_gir_on[5] = b_buf[3];

		memset(r_buf, 0, sizeof(r_buf));
		memset(g_buf, 0, sizeof(g_buf));
		memset(b_buf, 0, sizeof(b_buf));

		rc = dsi_panel_tx_cmd_set(panel,
			DSI_CMD_SET_MI_LOCAL_HBM_WHITE_110NIT_GIROFF_PRE_READ);
		if (rc) {
			DISP_ERROR("[%s] failed to send HBM_WHITE_110NIT_GIROFF_PRE_READ cmds, rc=%d\n",
				panel->type, rc);
			rc = -EINVAL;
			goto exit;
		}

		rc = mipi_dsi_dcs_read(&panel->mipi_device, 0xB2, r_buf, rdlength);
		if (rc < 0) {
			DISP_ERROR("[%s] read back w110_off red param failed, rc=%d\n", panel->type, rc);
			goto exit;
		}

		rc = mipi_dsi_dcs_read(&panel->mipi_device, 0xB5, g_buf, rdlength);
		if (rc < 0) {
			DISP_ERROR("[%s] read back w110_off green param failed, rc=%d\n", panel->type, rc);
			goto exit;
		}

		rc = mipi_dsi_dcs_read(&panel->mipi_device, 0xB8, b_buf, rdlength);
		if (rc < 0) {
			DISP_ERROR("[%s] read back w110_off blue param failed, rc=%d\n", panel->type, rc);
			goto exit;
		}

		mi_cfg->whitebuf_110_gir_off[0] = r_buf[2];
		mi_cfg->whitebuf_110_gir_off[1] = r_buf[3];
		mi_cfg->whitebuf_110_gir_off[2] = g_buf[2];
		mi_cfg->whitebuf_110_gir_off[3] = g_buf[3];
		mi_cfg->whitebuf_110_gir_off[4] = b_buf[2];
		mi_cfg->whitebuf_110_gir_off[5] = b_buf[3];

		mi_cfg->lhbm_w110_readbackdone = true;

		for (j = 0; j < sizeof(mi_cfg->whitebuf_110_gir_on); j++)
			DISP_INFO("read from 0xB2,0xB5, 0xB8,whitebuf_110_gir_on[%d] = 0x%02X\n", j, mi_cfg->whitebuf_110_gir_on[j]);

		for (j = 0; j < sizeof(mi_cfg->whitebuf_110_gir_off); j++)
			DISP_INFO("read from 0xB2,0xB5, 0xB8,whitebuf_110_gir_off[%d] = 0x%02X\n", j, mi_cfg->whitebuf_110_gir_off[j]);
	} else {
		mi_cfg->lhbm_w110_readbackdone = true;
	}
	DISP_INFO("[%s] LHBM white param read back end\n", panel->type);

exit:
	return rc;
}
static int mi_dsi_panel_update_lhbm_white_param(struct dsi_panel * panel,enum dsi_cmd_set_type type,int flat_mode)
{
	int rc=0;
	struct dsi_display_mode *mode;
	struct dsi_cmd_update_info *info = NULL;
	u32 cmd_update_index = 0;
	u32 cmd_update_count = 0;
	int i= 0, j = 0, level = 1000;
	u8 *whitebuf = NULL;
	u32 r = 0, g = 0, b = 0;

	if (!panel || !panel->host) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (!panel->mi_cfg.lhbm_w1000_update_flag && !panel->mi_cfg.lhbm_w110_update_flag) {
		DISP_DEBUG("[%s] don't need update white rgb config\n", panel->type);
		return 0;
	}

	if (!panel->panel_initialized) {
		DISP_ERROR("[%s] Panel not initialized\n", panel->type);
		rc = -EINVAL;
		goto exit;
	}

	if (panel->mi_cfg.uefi_read_lhbm_success &&
		(!panel->mi_cfg.lhbm_w1000_readbackdone || !panel->mi_cfg.lhbm_w110_readbackdone)) {
		for (i = 0; i < sizeof(panel->mi_cfg.lhbm_rgb_param); i++) {
			if (i < 6)
				panel->mi_cfg.whitebuf_1000_gir_on[i] = panel->mi_cfg.lhbm_rgb_param[i];
			else if (i < 12)
					panel->mi_cfg.whitebuf_1000_gir_off[i-6] = panel->mi_cfg.lhbm_rgb_param[i];
			else if (i < 18)
				panel->mi_cfg.whitebuf_110_gir_on[i-12] = panel->mi_cfg.lhbm_rgb_param[i];
			else if (i < 24)
				panel->mi_cfg.whitebuf_110_gir_off[i-18] = panel->mi_cfg.lhbm_rgb_param[i];
		}

		if (mi_get_panel_id(panel->mi_cfg.mi_panel_id) == P17_PANEL_PA) {
			/*250nit gamma GAMMA R=R[11:0]+70; 250nit GAMMA G=G[11:0]+70; 250nit GAMMA B=B[11:0]*+70
			*/
			for (i = 0; i < 2; i++) {
				if (i == 0) {
					whitebuf = panel->mi_cfg.whitebuf_110_gir_on;
				} else if (i == 1) {
					whitebuf = panel->mi_cfg.whitebuf_110_gir_off;
				}

				r = (whitebuf[0] << 8) + whitebuf[1] + 70;
				g = (whitebuf[2] << 8) + whitebuf[3] + 70;
				b = (whitebuf[4] << 8) + whitebuf[5] + 70;

				whitebuf[0] = (r >> 8) & 0xFF;
				whitebuf[1] = r & 0xFF;
				whitebuf[2] =(g >> 8) & 0xFF;
				whitebuf[3] = g & 0xFF;
				whitebuf[4] = (b >> 8) & 0xFF;
				whitebuf[5] = b & 0xFF;
			}
		}

		panel->mi_cfg.lhbm_w1000_readbackdone = true;
		panel->mi_cfg.lhbm_w110_readbackdone = true;
	}

	switch (type) {
	case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT:
		level = 1000;
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT:
		level = 1000;
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT:
		level = 110;
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT_UPDATE;
		break;
	case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT:
		level = 110;
		cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT_UPDATE;
		break;
	default:
		DISP_ERROR("[%s] unsupport cmd %s\n", panel->type, cmd_set_prop_map[type]);
		return -EINVAL;
	}

	mode = panel->cur_mode;
	info = mode->priv_info->cmd_update[cmd_update_index];
	cmd_update_count = mode->priv_info->cmd_update_count[cmd_update_index];
	for (j = 0; j < cmd_update_count; j++) {
		DISP_DEBUG("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
			panel->type, cmd_set_prop_map[info->type],
			info->mipi_address, info->index, info->length);
		if (info && info->mipi_address != 0xD0) {
			DISP_ERROR("error mipi address (0x%02X)\n", info->mipi_address);
			info++;
			continue;
		} else {
			if (level == 110) {
				if(flat_mode!= 0) {
					mi_dsi_panel_update_cmd_set(panel, mode, type, info,
					panel->mi_cfg.whitebuf_110_gir_on, sizeof(panel->mi_cfg.whitebuf_110_gir_on));
				} else {
					mi_dsi_panel_update_cmd_set(panel, mode, type, info,
					panel->mi_cfg.whitebuf_110_gir_off, sizeof(panel->mi_cfg.whitebuf_110_gir_off));
				}
			} else {
				if(flat_mode!= 0) {
					mi_dsi_panel_update_cmd_set(panel, mode, type, info,
					panel->mi_cfg.whitebuf_1000_gir_on, sizeof(panel->mi_cfg.whitebuf_1000_gir_on));
				} else {
					mi_dsi_panel_update_cmd_set(panel, mode, type, info,
					panel->mi_cfg.whitebuf_1000_gir_off, sizeof(panel->mi_cfg.whitebuf_1000_gir_off));
				}
			}
		}
	}

exit:
	return rc;
}

int mi_dsi_panel_set_round_corner_locked(struct dsi_panel *panel,
			bool enable)
{
	int rc = 0;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (panel->mi_cfg.ddic_round_corner_enabled) {
		if (enable)
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_ROUND_CORNER_ON);
		else
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_ROUND_CORNER_OFF);

		if (rc)
			DISP_ERROR("[%s] failed to send ROUND_CORNER(%s) cmds, rc=%d\n",
				panel->type, enable ? "On" : "Off", rc);
	} else {
		DISP_INFO("[%s] ddic round corner feature not enabled\n", panel->type);
	}

	return rc;
}

int mi_dsi_panel_set_round_corner(struct dsi_panel *panel, bool enable)
{
	int rc = 0;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	rc = mi_dsi_panel_set_round_corner_locked(panel, enable);

	mutex_unlock(&panel->panel_lock);

	return rc;
}

int mi_dsi_panel_set_dc_mode(struct dsi_panel *panel, bool enable)
{
	int rc = 0;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);
	rc = mi_dsi_panel_set_dc_mode_locked(panel, enable);

	mutex_unlock(&panel->panel_lock);

	return rc;
}


int mi_dsi_panel_set_dc_mode_locked(struct dsi_panel *panel, bool enable)
{
	int rc = 0;
	struct mi_dsi_panel_cfg *mi_cfg  = NULL;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	if (mi_cfg->dc_feature_enable) {
		if (enable) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DC_ON);
			mi_cfg->real_dc_state = FEATURE_ON;
		} else {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DC_OFF);
			mi_cfg->real_dc_state = FEATURE_OFF;
		}

		if (rc)
			DISP_ERROR("failed to set DC mode: %d\n", enable);
		else
			DISP_INFO("DC mode: %s\n", enable ? "On" : "Off");
	} else {
		DISP_INFO("DC mode: TODO\n");
	}

	return rc;
}

static int mi_dsi_panel_set_lhbm_fod_locked(struct dsi_panel *panel,
		struct disp_feature_ctl *ctl)
{
	int rc = 0;
	int update_bl = 0;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!panel || !ctl || ctl->feature_id != DISP_FEATURE_LOCAL_HBM) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	DISP_TIME_INFO("[%s] Local HBM: %s\n", panel->type,
			get_local_hbm_state_name(ctl->feature_val));

	switch (ctl->feature_val) {
	case LOCAL_HBM_OFF_TO_NORMAL:
		mi_dsi_update_backlight_in_aod(panel, true);
		if (mi_cfg->feature_val[DISP_FEATURE_HBM] == FEATURE_ON) {
			DISP_INFO("LOCAL_HBM_OFF_TO_HBM\n");
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
			mi_dsi_update_51_mipi_cmd(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_HBM,
					mi_cfg->last_bl_level);
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_HBM);
		} else {
			if (mi_cfg->is_em_cycle_16_pulse && mi_cfg->feature_val[DISP_FEATURE_FP_STATUS] != AUTH_STOP) {
				DISP_INFO("LOCAL_HBM_OFF_TO_0SIZE_LHBM CVC\n");
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT_PRE);
				mi_cfg->lhbm_0size_on = true;
			} else if (mi_cfg->is_em_cycle_16_pulse) {
				DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL CVC\n");
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL_EM_CYCLE_16PULSE);
				mi_cfg->lhbm_0size_on = false;
			} else {
				mi_dsi_update_51_mipi_cmd(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL,
					mi_cfg->last_bl_level);
					DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL\n");
					rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL);
			}
			mi_cfg->dimming_state = STATE_DIM_RESTORE;
			mi_dsi_panel_update_last_bl_level(panel, mi_cfg->last_bl_level);
		}
		mi_cfg->lhbm_status = LHBM_STATUS_OFF;
		break;
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT:
		DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT\n");
		mi_dsi_update_backlight_in_aod(panel, false);
		update_bl = mi_cfg->last_bl_level;
		/* display backlight value should equal AOD brightness */
		if (is_aod_and_panel_initialized(panel)) {
			switch (mi_cfg->doze_brightness) {
			case DOZE_BRIGHTNESS_HBM:
				if (mi_cfg->last_no_zero_bl_level < mi_cfg->doze_hbm_dbv_level
					&& mi_cfg->feature_val[DISP_FEATURE_FP_STATUS] == AUTH_STOP) {
					update_bl = mi_cfg->last_no_zero_bl_level;
				} else {
					update_bl = mi_cfg->doze_hbm_dbv_level;
				}
				break;
			case DOZE_BRIGHTNESS_LBM:
				if (mi_cfg->last_no_zero_bl_level < mi_cfg->doze_lbm_dbv_level) {
					update_bl = mi_cfg->last_no_zero_bl_level;
				} else {
					update_bl = mi_cfg->doze_lbm_dbv_level;
				}
				break;
			default:
				update_bl = mi_cfg->doze_hbm_dbv_level;
				break;
			}
		}

		DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT update_bl=%d\n", update_bl);

		mi_dsi_update_51_mipi_cmd(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL, update_bl);

		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL);
		mi_cfg->dimming_state = STATE_DIM_RESTORE;
		break;
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE:
		/* display backlight value should equal unlock brightness */
		if ((mi_get_panel_id(panel->mi_cfg.mi_panel_id) == P17_PANEL_PA) && !is_hbm_fod_on(panel)) {
				DISP_INFO("local hbm is off,no need send backlight restore\n");
				break;
		} else {
			DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL\n");
			//mi_dsi_update_backlight_in_aod(panel, true);
			mi_dsi_update_51_mipi_cmd(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL,
				mi_cfg->last_bl_level);
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL);
			mi_cfg->dimming_state = STATE_DIM_RESTORE;
			mi_cfg->doze_brightness = DOZE_TO_NORMAL;
			break;
		}
	case LOCAL_HBM_NORMAL_WHITE_1000NIT:
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		mi_cfg->lhbm_status = LHBM_STATUS_WHITE_1000NIT;
		if (mi_cfg->feature_val[DISP_FEATURE_HBM] == FEATURE_ON) {
			DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT in HBM\n");
			mi_dsi_update_51_mipi_cmd(panel,
					DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT,
					panel->bl_config.bl_max_level);
		} else {
			DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT\n");
			if (is_aod_and_panel_initialized(panel)) {
				switch (mi_cfg->doze_brightness) {
				case DOZE_BRIGHTNESS_HBM:
					mi_dsi_update_51_mipi_cmd(panel,
							DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT,
							mi_cfg->doze_hbm_dbv_level);
					mi_dsi_update_lhbm_cmd_87reg(panel,
							DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT,
							mi_cfg->doze_hbm_dbv_level);
					DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT in doze_hbm_dbv_level\n");
					break;
				case DOZE_BRIGHTNESS_LBM:
					mi_dsi_update_51_mipi_cmd(panel,
							DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT,
							mi_cfg->doze_lbm_dbv_level);
					mi_dsi_update_lhbm_cmd_87reg(panel,
							DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT,
							mi_cfg->doze_lbm_dbv_level);
					DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT in doze_lbm_dbv_level\n");
					break;
				default:
					mi_dsi_update_51_mipi_cmd(panel,
							DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT,
							mi_cfg->doze_hbm_dbv_level);
					mi_dsi_update_lhbm_cmd_87reg(panel,
							DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT,
							mi_cfg->doze_hbm_dbv_level);
					DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT use doze_hbm_dbv_level as defaults\n");
					break;
				}
			} else {
				mi_dsi_update_51_mipi_cmd(panel,
						DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT,
						mi_cfg->last_bl_level);
				mi_dsi_update_lhbm_cmd_87reg(panel,
						DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT,
						mi_cfg->last_bl_level);
			}
		}

		mi_dsi_update_lhbm_cmd_DF_reg(panel,
				DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT,
				mi_cfg->last_bl_level);

		mi_dsi_panel_update_lhbm_white_param(panel,
				DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT,
				mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE]);
		mi_dsi_panel_update_lhbm_white_gamma_param(panel,
				DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT);
		if (mi_cfg->is_em_cycle_16_pulse)
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT_EM_CYCLE_16PULSE);
		else
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT);
		break;
	case LOCAL_HBM_NORMAL_WHITE_750NIT:
		if (mi_cfg->is_em_cycle_16_pulse) {
			DISP_INFO("LOCAL_HBM_NORMAL_WHITE_750NIT_CVC\n");
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_750NIT_EM_CYCLE_16PULSE);
		} else {
			DISP_INFO("LOCAL_HBM_NORMAL_WHITE_750NIT\n");
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_750NIT);
		}
		break;
	case LOCAL_HBM_NORMAL_WHITE_500NIT:
		if (mi_cfg->is_em_cycle_16_pulse) {
			DISP_INFO("LOCAL_HBM_NORMAL_WHITE_500NIT_CVC\n");
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_500NIT_EM_CYCLE_16PULSE);
		} else {
			DISP_INFO("LOCAL_HBM_NORMAL_WHITE_500NIT\n");
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_500NIT);
		}
		break;
	case LOCAL_HBM_NORMAL_WHITE_110NIT:
		DISP_INFO("LOCAL_HBM_NORMAL_WHITE_110NIT\n");
		mi_cfg->lhbm_status = LHBM_STATUS_WHITE_110NIT;
		mi_dsi_update_lhbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT,
				mi_cfg->last_bl_level);
		mi_dsi_panel_update_lhbm_white_param(panel,
				DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT,
				mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE]);
		mi_dsi_panel_update_lhbm_white_gamma_param(panel,
				DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT);
		if (mi_cfg->is_em_cycle_16_pulse)
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT_EM_CYCLE_16PULSE);
		else
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT);
		break;
	case LOCAL_HBM_NORMAL_GREEN_500NIT:
		DISP_INFO("LOCAL_HBM_NORMAL_GREEN_500NIT\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		mi_cfg->lhbm_status = LHBM_STATUS_GREEN_500NIT;
		mi_dsi_update_lhbm_cmd_DF_reg(panel,
				DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_GREEN_500NIT, mi_cfg->last_bl_level);
		mi_dsi_update_lhbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_GREEN_500NIT,
				mi_cfg->last_bl_level);
		if (mi_cfg->is_em_cycle_16_pulse)
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_GREEN_500NIT_EM_CYCLE_16PULSE);
		else
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_GREEN_500NIT);
		break;
	case LOCAL_HBM_HLPM_WHITE_1000NIT:
		DISP_INFO("LOCAL_HBM_HLPM_WHITE_1000NIT\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		mi_cfg->lhbm_status = LHBM_STATUS_WHITE_1000NIT;
		mi_dsi_update_nolp_cmd_B2reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT);
		mi_dsi_update_lhbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT,
				mi_cfg->last_bl_level);
		if (is_aod_and_panel_initialized(panel)) {
			switch (mi_cfg->doze_brightness) {
			case DOZE_BRIGHTNESS_HBM:
				mi_dsi_update_51_mipi_cmd(panel,
					DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT,
					mi_cfg->doze_hbm_dbv_level);
				mi_dsi_update_lhbm_cmd_87reg(panel,
						DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT,
						mi_cfg->doze_hbm_dbv_level);
				DISP_INFO("LOCAL_HBM_HLPM_WHITE_1000NIT in doze_hbm_dbv_level\n");
				break;
			case DOZE_BRIGHTNESS_LBM:
				mi_dsi_update_51_mipi_cmd(panel,
					DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT,
					mi_cfg->doze_lbm_dbv_level);
				mi_dsi_update_lhbm_cmd_87reg(panel,
						DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT,
						mi_cfg->doze_lbm_dbv_level);
				DISP_INFO("LOCAL_HBM_HLPM_WHITE_1000NIT in doze_lbm_dbv_level\n");
				break;
			default:
				mi_dsi_update_51_mipi_cmd(panel,
					DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT,
					mi_cfg->doze_hbm_dbv_level);
				mi_dsi_update_lhbm_cmd_87reg(panel,
						DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT,
						mi_cfg->doze_hbm_dbv_level);
				DISP_INFO("LOCAL_HBM_HLPM_WHITE_1000NIT use doze_hbm_dbv_level as defaults\n");
				break;
			}
		}
		mi_dsi_update_backlight_in_aod(panel, false);
		mi_dsi_panel_update_lhbm_white_param(panel,
				DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT,
				mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE]);
		mi_dsi_panel_update_lhbm_white_gamma_param(panel,
				DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT);
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT);
		panel->mi_cfg.panel_state = PANEL_STATE_ON;
		break;
	case LOCAL_HBM_HLPM_WHITE_110NIT:
		DISP_INFO("LOCAL_HBM_HLPM_WHITE_110NIT\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		mi_cfg->lhbm_status = LHBM_STATUS_WHITE_110NIT;
		mi_dsi_update_nolp_cmd_B2reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT);
		mi_dsi_update_lhbm_cmd_87reg(panel, DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT,
				mi_cfg->last_bl_level);
		if (is_aod_and_panel_initialized(panel)) {
			switch (mi_cfg->doze_brightness) {
			case DOZE_BRIGHTNESS_HBM:
				mi_dsi_update_51_mipi_cmd(panel,
					DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT,
					mi_cfg->doze_hbm_dbv_level);
				mi_dsi_update_lhbm_cmd_87reg(panel,
						DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT,
						mi_cfg->doze_hbm_dbv_level);
				DISP_INFO("LOCAL_HBM_HLPM_WHITE_110NIT in doze_hbm_dbv_level\n");
				break;
			case DOZE_BRIGHTNESS_LBM:
				mi_dsi_update_51_mipi_cmd(panel,
					DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT,
					mi_cfg->doze_lbm_dbv_level);
				mi_dsi_update_lhbm_cmd_87reg(panel,
						DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT,
						mi_cfg->doze_lbm_dbv_level);
				DISP_INFO("LOCAL_HBM_HLPM_WHITE_110NIT in doze_lbm_dbv_level\n");
				break;
			default:
				mi_dsi_update_51_mipi_cmd(panel,
					DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT,
					mi_cfg->doze_lbm_dbv_level);
				mi_dsi_update_lhbm_cmd_87reg(panel,
						DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT,
						mi_cfg->doze_lbm_dbv_level);
				DISP_INFO("LOCAL_HBM_HLPM_WHITE_1000NIT use doze_lbm_dbv_level as defaults\n");
				break;
			}
		}
		mi_dsi_update_backlight_in_aod(panel, false);
		mi_dsi_panel_update_lhbm_white_param(panel,
				DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT,
				mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE]);
		mi_dsi_panel_update_lhbm_white_gamma_param(panel,
				DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT);
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT);
		panel->mi_cfg.panel_state = PANEL_STATE_ON;
		break;
	case LOCAL_HBM_OFF_TO_HLPM:
		DISP_INFO("LOCAL_HBM_OFF_TO_HLPM\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		mi_cfg->lhbm_status = LHBM_STATUS_OFF;
		panel->mi_cfg.panel_state = PANEL_STATE_DOZE_HIGH;
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_HLPM);
		break;
	case LOCAL_HBM_OFF_TO_LLPM:
		DISP_INFO("LOCAL_HBM_OFF_TO_LLPM\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		mi_cfg->lhbm_status = LHBM_STATUS_OFF;
		panel->mi_cfg.panel_state = PANEL_STATE_DOZE_LOW;
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_LLPM);
		break;
	default:
		DISP_ERROR("invalid local hbm value\n");
		break;
	}

	return rc;
}

int mi_dsi_panel_aod_to_normal_optimize_locked(struct dsi_panel *panel,
		bool enable)
{
	int rc = 0;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!panel || !panel->panel_initialized) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	if (!mi_cfg->need_fod_animal_in_normal)
		return 0;

	DISP_TIME_DEBUG("[%s] fod aod_to_normal: %d\n", panel->type, enable);

	if (is_hbm_fod_on(panel)) {
		DSI_INFO("fod hbm on, skip fod aod_to_normal: %d\n", enable);
		return 0;
	}

	if (enable && mi_cfg->panel_state != PANEL_STATE_ON) {
		switch (mi_cfg->doze_brightness) {
		case DOZE_BRIGHTNESS_HBM:
			DISP_INFO("enter DOZE HBM NOLP\n");
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM_NOLP);
			if (rc) {
				DISP_ERROR("[%s] failed to send MI_DOZE_HBM_NOLP cmd, rc=%d\n",
					panel->type, rc);
			} else {
				panel->mi_cfg.panel_state = PANEL_STATE_ON;
				mi_cfg->aod_to_normal_statue = true;
				if (mi_get_panel_id_by_dsi_panel(panel) == O2_PANEL_PA)
					mi_cfg->is_em_cycle_16_pulse = true;
			}
			break;
		case DOZE_BRIGHTNESS_LBM:
			DISP_INFO("enter DOZE LBM NOLP\n");
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM_NOLP);
			if (rc) {
				DISP_ERROR("[%s] failed to send MI_DOZE_LBM_NOLP cmd, rc=%d\n",
					panel->type, rc);
			} else {
				panel->mi_cfg.panel_state = PANEL_STATE_ON;
				mi_cfg->aod_to_normal_statue = true;
				if (mi_get_panel_id_by_dsi_panel(panel) == O2_PANEL_PA)
					mi_cfg->is_em_cycle_16_pulse = true;
			}
			break;
		default:
			break;
		}
	} else if (!enable && mi_cfg->panel_state == PANEL_STATE_ON &&
		mi_cfg->feature_val[DISP_FEATURE_FP_STATUS] != AUTH_STOP) {
		switch (mi_cfg->doze_brightness) {
		case DOZE_BRIGHTNESS_HBM:
			DISP_INFO("enter DOZE HBM\n");
			mi_dsi_update_51_mipi_cmd(panel, DSI_CMD_SET_MI_DOZE_HBM,
					mi_cfg->doze_hbm_dbv_level);
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM);
			if (rc) {
				DISP_ERROR("[%s] failed to send MI_DOZE_HBM_NOLP cmd, rc=%d\n",
					panel->type, rc);
			}
			panel->mi_cfg.panel_state = PANEL_STATE_DOZE_HIGH;
			mi_cfg->aod_to_normal_statue = false;
			mi_cfg->is_em_cycle_16_pulse = false;
			break;
		case DOZE_BRIGHTNESS_LBM:
			DISP_INFO("enter DOZE LBM\n");
			mi_dsi_update_51_mipi_cmd(panel, DSI_CMD_SET_MI_DOZE_LBM,
					mi_cfg->doze_lbm_dbv_level);
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM);
			if (rc) {
				DISP_ERROR("[%s] failed to send MI_DOZE_LBM_NOLP cmd, rc=%d\n",
					panel->type, rc);
			}
			panel->mi_cfg.panel_state = PANEL_STATE_DOZE_LOW;
			mi_cfg->aod_to_normal_statue = false;
			mi_cfg->is_em_cycle_16_pulse = false;
			break;
		default:
			break;
		}
	}

	return rc;
}

bool mi_dsi_panel_is_need_tx_cmd(u32 feature_id)
{
	switch (feature_id) {
	case DISP_FEATURE_SENSOR_LUX:
	case DISP_FEATURE_FOLD_STATUS:
		return false;
	default:
		return true;
	}
}

int mi_dsi_panel_set_disp_param(struct dsi_panel *panel, struct disp_feature_ctl *ctl)
{
	int rc = 0;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!panel || !ctl) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	DISP_TIME_INFO("[%s] feature: %s, value: %d\n", panel->type,
			get_disp_feature_id_name(ctl->feature_id), ctl->feature_val);


	mi_cfg = &panel->mi_cfg;

	if (!panel->panel_initialized &&
		mi_dsi_panel_is_need_tx_cmd(ctl->feature_id)) {
		if (ctl->feature_id == DISP_FEATURE_DC)
			mi_cfg->feature_val[DISP_FEATURE_DC] = ctl->feature_val;
		if (ctl->feature_id == DISP_FEATURE_DBI)
  			mi_cfg->feature_val[DISP_FEATURE_DBI] = ctl->feature_val;
		DISP_WARN("[%s] panel not initialized!\n", panel->type);
		rc = -ENODEV;
		goto exit;
	}


	switch (ctl->feature_id) {
	case DISP_FEATURE_DIMMING:
		if (!mi_cfg->disable_ic_dimming){
			if (mi_cfg->dimming_state != STATE_DIM_BLOCK) {
				if (ctl->feature_val == FEATURE_ON )
					rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGON);
				else
					rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGOFF);
				mi_cfg->feature_val[DISP_FEATURE_DIMMING] = ctl->feature_val;
			} else {
				DISP_INFO("skip dimming %s\n", ctl->feature_val ? "on" : "off");
			}
		} else{
			DISP_INFO("disable_ic_dimming is %d\n", mi_cfg->disable_ic_dimming);
		}
		break;
	case DISP_FEATURE_HBM:
		mi_cfg->feature_val[DISP_FEATURE_HBM] = ctl->feature_val;
#ifdef CONFIG_FACTORY_BUILD
		if (ctl->feature_val == FEATURE_ON) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_ON);
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
		} else {
			mi_dsi_update_51_mipi_cmd(panel, DSI_CMD_SET_MI_HBM_OFF,
					mi_cfg->last_bl_level);
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_OFF);
			mi_cfg->dimming_state = STATE_DIM_RESTORE;
		}
#endif
		mi_disp_feature_event_notify_by_type(mi_get_disp_id(panel->type),
				MI_DISP_EVENT_HBM, sizeof(ctl->feature_val), ctl->feature_val);
		break;
	case DISP_FEATURE_DOZE_BRIGHTNESS:
#ifdef CONFIG_FACTORY_BUILD
		if (dsi_panel_initialized(panel) &&
			is_aod_brightness(ctl->feature_val)) {
			if (ctl->feature_val == DOZE_BRIGHTNESS_HBM)
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM);
			else
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM);
		} else {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_NOLP);
			dsi_panel_update_backlight(panel, mi_cfg->last_bl_level);
		}
#else
		if (is_aod_and_panel_initialized(panel) &&
			is_aod_brightness(ctl->feature_val)) {
			if (ctl->feature_val == DOZE_BRIGHTNESS_HBM) {
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM);
			} else {
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM);
			}
		} else {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_NOLP);
		}
#endif
		mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] = ctl->feature_val;
		break;
	case DISP_FEATURE_FLAT_MODE:
		if (!mi_cfg->flat_sync_te) {
			if (ctl->feature_val == FEATURE_ON) {
				DISP_INFO("flat mode on\n");
				mi_dsi_update_flat_mode_on_cmd(panel, DSI_CMD_SET_MI_FLAT_MODE_ON);
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_ON);
				rc |= dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_SEC_ON);
			}
			else {
				DISP_INFO("flat mode off\n");
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_OFF);
				rc |= dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_SEC_OFF);
			}
			mi_disp_feature_event_notify_by_type(mi_get_disp_id(panel->type),
				MI_DISP_EVENT_FLAT_MODE, sizeof(ctl->feature_val), ctl->feature_val);
		} else {
			rc = mi_dsi_send_flat_sync_with_te_locked(panel,
					ctl->feature_val == FEATURE_ON);
		}
		mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE] = ctl->feature_val;
		break;
	case DISP_FEATURE_CRC:
		DISP_INFO("TODO\n");
		break;
	case DISP_FEATURE_DC:
		DISP_INFO("DC mode state:%d\n", ctl->feature_val);
		if (mi_cfg->dc_feature_enable) {
			rc = mi_dsi_panel_set_dc_mode_locked(panel, ctl->feature_val == FEATURE_ON);
		}
		mi_cfg->feature_val[DISP_FEATURE_DC] = ctl->feature_val;
		mi_disp_feature_event_notify_by_type(mi_get_disp_id(panel->type),
				MI_DISP_EVENT_DC, sizeof(ctl->feature_val), ctl->feature_val);
		break;
	case DISP_FEATURE_SENSOR_LUX:
		DISP_DEBUG("DISP_FEATURE_SENSOR_LUX=%d\n", ctl->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_SENSOR_LUX] = ctl->feature_val;
		break;
	case DISP_FEATURE_FP_STATUS:
		mi_cfg->feature_val[DISP_FEATURE_FP_STATUS] = ctl->feature_val;
		mi_disp_feature_event_notify_by_type(mi_get_disp_id(panel->type),
				MI_DISP_EVENT_FP, sizeof(ctl->feature_val), ctl->feature_val);
		break;
	case DISP_FEATURE_FOLD_STATUS:
		DISP_INFO("DISP_FEATURE_FOLD_STATUS=%d\n", ctl->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_FOLD_STATUS] = ctl->feature_val;
		break;
	case DISP_FEATURE_LOCAL_HBM:
		rc = mi_dsi_panel_set_lhbm_fod_locked(panel, ctl);
		mi_cfg->feature_val[DISP_FEATURE_LOCAL_HBM] = ctl->feature_val;
		break;
	case DISP_FEATURE_NATURE_FLAT_MODE:
		DISP_INFO("TODO\n");
		break;
	case DISP_FEATURE_SPR_RENDER:
		DISP_INFO("TODO\n");
		break;
	case DISP_FEATURE_AOD_TO_NORMAL:
		rc = mi_dsi_panel_aod_to_normal_optimize_locked(panel,
				ctl->feature_val == FEATURE_ON);
		mi_cfg->feature_val[DISP_FEATURE_AOD_TO_NORMAL] = ctl->feature_val;
		break;
	case DISP_FEATURE_COLOR_INVERT:
		DISP_INFO("TODO\n");
		break;
	case DISP_FEATURE_DC_BACKLIGHT:
		DISP_INFO("TODO\n");
		break;
	case DISP_FEATURE_GIR:
		DISP_INFO("TODO\n");
		break;
	case DISP_FEATURE_DBI:
		DISP_INFO("by temp gray_level:%d\n", ctl->feature_val);
		if (mi_cfg->feature_val[DISP_FEATURE_DBI] == ctl->feature_val) {
			DISP_INFO("gray_level is the same, return\n");
			break;
		}
		//mi_dsi_panel_set_dbi_by_temp_locked(panel, ctl->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_DBI] = ctl->feature_val;
		break;
	case DISP_FEATURE_DDIC_ROUND_CORNER:
		DISP_INFO("DDIC round corner state:%d\n", ctl->feature_val);
		rc = mi_dsi_panel_set_round_corner_locked(panel, ctl->feature_val == FEATURE_ON);
		mi_cfg->feature_val[DISP_FEATURE_DDIC_ROUND_CORNER] = ctl->feature_val;
		break;
	case DISP_FEATURE_HBM_BACKLIGHT:
		DISP_INFO("hbm backlight:%d\n", ctl->feature_val);
		panel->mi_cfg.last_bl_level = ctl->feature_val;
		dsi_panel_update_backlight(panel, panel->mi_cfg.last_bl_level);
		break;
	case DISP_FEATURE_BACKLIGHT:
		DISP_INFO("backlight:%d\n", ctl->feature_val);
		panel->mi_cfg.last_bl_level = ctl->feature_val;
		dsi_panel_update_backlight(panel, panel->mi_cfg.last_bl_level);
		break;
	case DISP_FEATURE_CABC:
        	DISP_INFO("feature_val is %d",ctl->feature_val);
        	switch (ctl->feature_val)
        	{
        	case CABC_OFF:
			DISP_INFO("entry bkl cabc close \n");
            		dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_CABCOFF);
            		break;
        	case CABC_UI_ON:
			DISP_INFO("entry bkl cabc1  UI\n");
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_CABCUION);
			break;
		case CABC_STILL_ON:
			DISP_INFO("entry bkl cabc2  STILL\n");
			dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_CABCSTILLON);
			break;
		case CABC_MOVIE_ON:
			 DISP_INFO("entry bkl cabc3 MOVIE \n");
			 dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_CABCMOVIEON);
			 break;
		default:
			 break;
		}
		mi_cfg->feature_val[DISP_FEATURE_CABC] = ctl->feature_val;
		break;
	default:
		DISP_ERROR("invalid feature argument: %d\n", ctl->feature_id);
		break;
	}
exit:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int mi_dsi_panel_get_disp_param(struct dsi_panel *panel,
			struct disp_feature_ctl *ctl)
{
	struct mi_dsi_panel_cfg *mi_cfg;
	int i = 0;

	if (!panel || !ctl) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (!is_support_disp_feature_id(ctl->feature_id)) {
		DISP_ERROR("unsupported disp feature id\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);
	mi_cfg = &panel->mi_cfg;
	for (i = DISP_FEATURE_DIMMING; i < DISP_FEATURE_MAX; i++) {
		if (i == ctl->feature_id) {
			ctl->feature_val =  mi_cfg->feature_val[i];
			DISP_INFO("%s: %d\n", get_disp_feature_id_name(ctl->feature_id),
				ctl->feature_val);
		}
	}
	mutex_unlock(&panel->panel_lock);

	return 0;
}

ssize_t mi_dsi_panel_show_disp_param(struct dsi_panel *panel,
			char *buf, size_t size)
{
	struct mi_dsi_panel_cfg *mi_cfg;
	ssize_t count = 0;
	int i = 0;

	if (!panel || !buf || !size) {
		DISP_ERROR("invalid params\n");
		return -EAGAIN;
	}

	count = snprintf(buf, size, "%40s: feature vaule\n", "feature name[feature id]");

	mutex_lock(&panel->panel_lock);
	mi_cfg = &panel->mi_cfg;
	for (i = DISP_FEATURE_DIMMING; i < DISP_FEATURE_MAX; i++) {
		count += snprintf(buf + count, size - count, "%36s[%02d]: %d\n",
				get_disp_feature_id_name(i), i, mi_cfg->feature_val[i]);

	}
	mutex_unlock(&panel->panel_lock);

	return count;
}

int dsi_panel_parse_build_id_read_config(struct dsi_panel *panel)
{
	struct drm_panel_build_id_config *id_config;
	struct dsi_parser_utils *utils = &panel->utils;
	int rc = 0;

	if (!panel) {
		DISP_ERROR("Invalid Params\n");
		return -EINVAL;
	}

	id_config = &panel->id_config;
	id_config->build_id = 0;
	if (!id_config)
		return -EINVAL;

	rc = utils->read_u32(utils->data, "mi,panel-build-id-read-length",
                                &id_config->id_cmds_rlen);
	if (rc) {
		id_config->id_cmds_rlen = 0;
		return -EINVAL;
	}

	dsi_panel_parse_cmd_sets_sub(&id_config->id_cmd,
			DSI_CMD_SET_MI_PANEL_BUILD_ID, utils);
	if (!(id_config->id_cmd.count)) {
		DISP_ERROR("panel build id read command parsing failed\n");
		return -EINVAL;
	}
	return 0;
}

int dsi_panel_parse_wp_reg_read_config(struct dsi_panel *panel)
{
	struct drm_panel_wp_config *wp_config;
	struct dsi_parser_utils *utils = &panel->utils;
	int rc = 0;

	if (!panel) {
		DISP_ERROR("Invalid Params\n");
		return -EINVAL;
	}

	wp_config = &panel->wp_config;
	if (!wp_config)
		return -EINVAL;

	dsi_panel_parse_cmd_sets_sub(&wp_config->wp_cmd,
			DSI_CMD_SET_MI_PANEL_WP_READ, utils);
	if (!wp_config->wp_cmd.count) {
		DISP_ERROR("wp_info read command parsing failed\n");
		return -EINVAL;
	}

	dsi_panel_parse_cmd_sets_sub(&wp_config->pre_tx_cmd,
			DSI_CMD_SET_MI_PANEL_WP_READ_PRE_TX, utils);
	if (!wp_config->pre_tx_cmd.count)
		DISP_INFO("wp_info pre command parsing failed\n");

	rc = utils->read_u32(utils->data, "mi,mdss-dsi-panel-wp-read-length",
				&wp_config->wp_cmds_rlen);
	if (rc || !wp_config->wp_cmds_rlen) {
		wp_config->wp_cmds_rlen = 0;
		return -EINVAL;
	}

	rc = utils->read_u32(utils->data, "mi,mdss-dsi-panel-wp-read-index",
				&wp_config->wp_read_info_index);
	if (rc || !wp_config->wp_read_info_index) {
		wp_config->wp_read_info_index = 0;
	}

	wp_config->return_buf = kcalloc(wp_config->wp_cmds_rlen,
		sizeof(unsigned char), GFP_KERNEL);
	if (!wp_config->return_buf) {
		return -ENOMEM;
	}
	return 0;
}

int dsi_panel_parse_cell_id_read_config(struct dsi_panel *panel)
{
	struct drm_panel_cell_id_config *cell_id_config;
	struct dsi_parser_utils *utils = &panel->utils;
	int rc = 0;

	if (!panel) {
		DISP_ERROR("Invalid Params\n");
		return -EINVAL;
	}

	cell_id_config = &panel->cell_id_config;
	if (!cell_id_config)
		return -EINVAL;

	dsi_panel_parse_cmd_sets_sub(&cell_id_config->cell_id_cmd,
			DSI_CMD_SET_MI_PANEL_CELL_ID_READ, utils);
	if (!cell_id_config->cell_id_cmd.count) {
		DISP_ERROR("cell_id_info read command parsing failed\n");
		return -EINVAL;
	}

	dsi_panel_parse_cmd_sets_sub(&cell_id_config->pre_tx_cmd,
			DSI_CMD_SET_MI_PANEL_CELL_ID_READ_PRE_TX, utils);
	if (!cell_id_config->pre_tx_cmd.count)
		DISP_INFO("cell_id_info pre command parsing failed\n");

	dsi_panel_parse_cmd_sets_sub(&cell_id_config->after_tx_cmd,
			DSI_CMD_SET_MI_PANEL_CELL_ID_READ_AFTER_TX, utils);
	if (!cell_id_config->after_tx_cmd.count)
		DISP_INFO("cell_id_info after command parsing failed\n");

	rc = utils->read_u32(utils->data, "mi,mdss-dsi-panel-cell-id-read-length",
				&cell_id_config->cell_id_cmds_rlen);
	if (rc || !cell_id_config->cell_id_cmds_rlen) {
		cell_id_config->cell_id_cmds_rlen = 0;
		return -EINVAL;
	}

	rc = utils->read_u32(utils->data, "mi,mdss-dsi-panel-cell-id-read-index",
				&cell_id_config->cell_id_read_info_index);
	if (rc || !cell_id_config->cell_id_read_info_index) {
		cell_id_config->cell_id_read_info_index = 0;
	}

	cell_id_config->return_buf = kcalloc(cell_id_config->cell_id_cmds_rlen,
		sizeof(unsigned char), GFP_KERNEL);
	if (!cell_id_config->return_buf) {
		return -ENOMEM;
	}
	return 0;
}

int mi_dsi_panel_set_nolp_locked(struct dsi_panel *panel)
{
	int rc = 0;
	u32 doze_brightness = panel->mi_cfg.doze_brightness;
	int update_bl = 0;

	if (panel->mi_cfg.panel_state == PANEL_STATE_ON) {
		DISP_INFO("panel already PANEL_STATE_ON, skip nolp");
		return rc;
	}

	if (doze_brightness == DOZE_TO_NORMAL)
		doze_brightness = panel->mi_cfg.last_doze_brightness;

	switch (doze_brightness) {
		case DOZE_BRIGHTNESS_HBM:
			DISP_INFO("set doze_hbm_dbv_level in nolp");
			update_bl = panel->mi_cfg.doze_hbm_dbv_level;
			break;
		case DOZE_BRIGHTNESS_LBM:
			DISP_INFO("set doze_lbm_dbv_level in nolp");
			update_bl = panel->mi_cfg.doze_lbm_dbv_level;
			break;
		default:
			break;
	}

	mi_dsi_update_51_mipi_cmd(panel, DSI_CMD_SET_NOLP, update_bl);

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_NOLP);

	panel->power_mode = SDE_MODE_DPMS_ON;

	return rc;
}

int mi_dsi_panel_set_count_info(struct dsi_panel * panel, struct disp_count_info *count_info)
{
	int rc = 0;

	if (!panel || !count_info) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	DISP_TIME_INFO("[%s] count info type: %s, value: %d\n", panel->type,
		get_disp_count_info_type_name(count_info->count_info_type), count_info->count_info_val);

	switch (count_info->count_info_type) {
	case DISP_COUNT_INFO_POWERSTATUS:
		DISP_DEBUG("power get:%d, last power mode = %d\n", count_info->count_info_val, panel->power_mode);
		if (panel->power_mode == SDE_MODE_DPMS_OFF && count_info->count_info_val == 1) /* hal::PowerMode::ON */
			mi_dsi_panel_state_count(panel, PANEL_POWERON_COST, 1);
		break;
	case DISP_COUNT_INFO_SYSTEM_BUILD_VERSION:
		DISP_INFO("system build version:%s\n", count_info->tx_ptr);
		mi_dsi_panel_set_build_version(panel, count_info->tx_ptr, count_info->tx_len);
		break;
	case DISP_COUNT_INFO_FRAME_DROP_COUNT:
		DISP_INFO("frame drop count:%d\n", count_info->count_info_val);
		mi_dsi_panel_state_count(panel, COMMIT_LONG, count_info->count_info_val);
		break;
	case DISP_COUNT_INFO_SWITCH_KERNEL_FUNCTION_TIMER:
		DISP_INFO("swith function timer:%d\n", count_info->count_info_val);
		mi_kernel_set_recoder_suport(count_info->count_info_val == 1);
		break;
	default:
		break;
	}

	return rc;
}

int mi_dsi_panel_match_fps_setting(struct dsi_panel *panel,
				struct dsi_display_mode *adj_mode)
{
	int rc =0;
	int retval = 0;
	struct dsi_display_mode_priv_info *priv_info;

	if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info || !adj_mode) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	priv_info = panel->cur_mode->priv_info;

	if (!priv_info->cmd_sets[DSI_CMD_SET_DISP_120HZ].count) {
		pr_debug("DSI_CMD_SET_DISP_120HZ not defined, return\n");
		return 0;
	}

	if (adj_mode->timing.refresh_rate == 120)
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_120HZ);
	else if (adj_mode->timing.refresh_rate == 90)
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_90HZ);
	else if (adj_mode->timing.refresh_rate == 60)
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_60HZ);
	else if (adj_mode->timing.refresh_rate == 30) {
		panel->mi_cfg.aod_enter_flags =true;
		//	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DISP_30HZ);
	}

	if (rc) {
		pr_err("Failed to send DSI_CMD_SET_DISP_120HZ command\n");
		retval = -EAGAIN;
		goto error;
	}else
		pr_info("%s: refresh_rate[%d]\n", __func__, adj_mode->timing.refresh_rate);
	panel->mi_cfg.last_refresh_rate = panel->cur_mode->timing.refresh_rate;

error:
	return retval;
}
