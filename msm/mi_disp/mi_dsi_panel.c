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
#include <linux/string.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <video/mipi_display.h>
#include <drm/mi_disp.h>
#include <linux/soc/qcom/panel_event_notifier.h>
#include <linux/soc/qcom/smem.h>

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
#include "mi_panel_id.h"
#include "mi_disp_dbv_data.h"
#include "mi_disp_lhbm.h"

#define SMEM_SW_DISPLAY_LOCKDOWN_TABLE 499
#define to_dsi_display(x) container_of(x, struct dsi_display, host)

void mi_dsi_update_backlight_in_aod(struct dsi_panel *panel,
			bool restore_backlight);

static u64 g_panel_id[MI_DISP_MAX];

struct dsi_panel *g_panel;

typedef int (*mi_display_pwrkey_callback)(int);
extern void mi_display_pwrkey_callback_set(mi_display_pwrkey_callback);
/**
 * dsi_pwr_enable_vregs() - enable/disable regulators
 */
int mi_dsi_pwr_enable_vregs(struct dsi_regulator_info *regs, bool enable, int index)
{
	int rc = 0;
	struct dsi_vreg *vreg;
	int num_of_v = 0;
	u32 pre_on_ms, post_on_ms;
	u32 pre_off_ms, post_off_ms;

	if (enable) {
		vreg = &regs->vregs[index];
		pre_on_ms = vreg->pre_on_sleep;
		post_on_ms = vreg->post_on_sleep;

		if (vreg->pre_on_sleep)
			usleep_range((pre_on_ms * 1000),
					(pre_on_ms * 1000) + 10);

		rc = regulator_set_load(vreg->vreg,
					vreg->enable_load);
		if (rc < 0) {
			DSI_ERR("mi Setting optimum mode failed for %s\n",
					vreg->vreg_name);
			goto error;
		}
		num_of_v = regulator_count_voltages(vreg->vreg);
		if (num_of_v > 0) {
			rc = regulator_set_voltage(vreg->vreg,
							vreg->min_voltage,
							vreg->max_voltage);
			if (rc) {
				DSI_ERR("mi Set voltage(%s) fail, rc=%d\n",
						vreg->vreg_name, rc);
				goto error_disable_opt_mode;
			}
		}

		rc = regulator_enable(vreg->vreg);
		if (rc) {
			DSI_ERR("mi enable failed for %s, rc=%d\n",
					vreg->vreg_name, rc);
			goto error_disable_voltage;
		}
		if (vreg->post_on_sleep)
			usleep_range((post_on_ms * 1000),
					(post_on_ms * 1000) + 10);
	} else {
		vreg = &regs->vregs[index];
		pre_off_ms = vreg->pre_off_sleep;
		post_off_ms = vreg->post_off_sleep;

		if (pre_off_ms)
			usleep_range((pre_off_ms * 1000),
					(pre_off_ms * 1000) + 10);

		(void)regulator_disable(regs->vregs[index].vreg);

		if (post_off_ms)
			usleep_range((post_off_ms * 1000),
					(post_off_ms * 1000) + 10);

		(void)regulator_set_load(regs->vregs[index].vreg,
					regs->vregs[index].disable_load);

		num_of_v = regulator_count_voltages(vreg->vreg);
		if (num_of_v > 0)
			(void)regulator_set_voltage(regs->vregs[index].vreg,
					regs->vregs[index].off_min_voltage,
					regs->vregs[index].max_voltage);
	}
	DISP_INFO("mi_dsi_pwr_enable_vregs %s, enable:%d\n",vreg->vreg_name, enable);
	return 0;
error_disable_opt_mode:
	(void)regulator_set_load(regs->vregs[index].vreg,
				 regs->vregs[index].disable_load);

error_disable_voltage:
	if (num_of_v > 0)
		(void)regulator_set_voltage(regs->vregs[index].vreg,
					    0, regs->vregs[index].max_voltage);
error:
	vreg = &regs->vregs[index];
	pre_off_ms = vreg->pre_off_sleep;
	post_off_ms = vreg->post_off_sleep;

	if (pre_off_ms)
		usleep_range((pre_off_ms * 1000),
				(pre_off_ms * 1000) + 10);

	(void)regulator_disable(regs->vregs[index].vreg);

	if (post_off_ms)
		usleep_range((post_off_ms * 1000),
				(post_off_ms * 1000) + 10);

	(void)regulator_set_load(regs->vregs[index].vreg,
					regs->vregs[index].disable_load);

	num_of_v = regulator_count_voltages(regs->vregs[index].vreg);
	if (num_of_v > 0)
		(void)regulator_set_voltage(regs->vregs[index].vreg,
			0, regs->vregs[index].max_voltage);

	return rc;
}

void dsi_panel_gesture_enable(bool enable)
{
	if (!g_panel) {
		DISP_ERROR("invalid params\n");
		return;
	}
	g_panel->mi_cfg.tddi_gesture_flag = enable;
}
EXPORT_SYMBOL(dsi_panel_gesture_enable);

void mi_display_gesture_callback_register(void (*cb)(void))
{
	if (!g_panel) {
		DISP_ERROR("invalid params\n");
		return;
	}
	g_panel->mi_cfg.mi_display_gesture_cb = cb;
	DISP_INFO("func %pF is set.\n", cb);
	return;
}
EXPORT_SYMBOL(mi_display_gesture_callback_register);

static int mi_panel_id_init(struct dsi_panel *panel)
{
	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	g_panel = panel;
	g_panel->mi_cfg.mi_display_gesture_cb = NULL;
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

int mi_dsi_panel_pre_init(struct dsi_panel *panel)
{
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	int rc = 0;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;
	mi_cfg->dsi_panel = panel;

	rc = panel->utils.read_u64(panel->utils.data, "mi,panel-id", &mi_cfg->mi_panel_id);
	if (rc) {
		mi_cfg->mi_panel_id = 0;
		DISP_ERROR("mi,panel-id not specified\n");
	} else {
		DISP_DEBUG("mi,panel-id is 0x%llx (%s)\n",
			mi_cfg->mi_panel_id, mi_get_panel_id_name(mi_cfg->mi_panel_id));
	}

	rc = mi_panel_id_init(panel);

	return rc;
}

int mi_dsi_panel_init(struct dsi_panel *panel)
{
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	mutex_init(&mi_cfg->doze_lock);

	mi_cfg->disp_wakelock = wakeup_source_register(NULL, "disp_wakelock");
	if (!mi_cfg->disp_wakelock) {
		DISP_ERROR("doze_wakelock wake_source register failed");
		return -ENOMEM;
	}

	mi_dsi_panel_parse_config(panel);
	atomic_set(&mi_cfg->brightness_clone, 0);

	mi_display_pwrkey_callback_set(mi_display_powerkey_callback);

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
		dsi_panel_tx_cmd_set(dsi_panel, DSI_CMD_SET_MI_FRAME_SWITCH_MODE_SEC, false);
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

	if (mi_cfg->in_fod_calibration) {
		DISP_INFO("[%s] skip set backlight %d due to fod calibration\n", panel->type, bl_lvl);
		return true;
	} else if (is_hbm_fod_on(panel)) {
		if (bl_lvl != 0) {
			DISP_INFO("[%s] update 51 reg to %d even LHBM is on,",
				panel->type, bl_lvl);
			return false;
		} else {
			DISP_INFO("[%s] skip set backlight %d due to fod hbm\n", panel->type, bl_lvl);
		}
		return true;
	} else if (bl_lvl != 0 && is_dc_on_skip_backlight(panel, bl_lvl)) {
		DISP_DEBUG("[%s] skip set backlight %d due to DC on\n", panel->type, bl_lvl);
		return true;
	} else if (panel->power_mode == SDE_MODE_DPMS_LP1) {
		if (bl_lvl == 0) {
			DISP_INFO("[%s] skip set backlight %d due to LP1 on\n", panel->type, bl_lvl);
			return true;
		}
		return false;
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
		mi_dsi_panel_tigger_dimming_delayed_work(panel);
		if (mi_cfg->dimming_state == STATE_DIM_RESTORE)
			mi_cfg->dimming_state = STATE_NONE;
	}

	mi_cfg->last_bl_level = brightness;
	if (brightness != 0)
		mi_cfg->last_no_zero_bl_level = brightness;

	return;
}

int mi_dsi_print_register_value_log(struct dsi_panel *panel,
		struct dsi_cmd_desc *cmd)
{
	u8 *buf = NULL;
	u32 bl_lvl = 0;
	int i = 0;
	struct mi_dsi_panel_cfg *mi_cfg;
	static int use_count = 20;
	char *buffer = NULL;
	int buf_size = 0;
	int count = 0;

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

			if (use_count-- > 0)
				DISP_TIME_INFO("[%s] set 51 backlight %d\n", panel->type, bl_lvl);

			if (!mi_cfg->last_bl_level || !bl_lvl) {
					use_count = 20;
			}
		}

		if (mi_get_register_log_mask(buf[0]) & BACKLIGHT_LOG_ENABLE) {
			DISP_INFO("[%s] [0x51 backlight debug] tx_len = %lu\n",
					panel->type, cmd->msg.tx_len);
			for (i = 0; i < cmd->msg.tx_len; i++) {
				DISP_INFO("[%s] [0x51 backlight debug] tx_buf[%d] = 0x%02X\n",
					panel->type, i, buf[i]);
			}

			if (mi_get_register_log_mask(buf[0]) & BACKLIGHT_LOG_DUMP_STACK)
				dump_stack();
		}
	} else if (buf && mi_get_register_log_mask(buf[0])) {
		buf_size = cmd->msg.tx_len * 5 + 1;
		buffer = kzalloc(buf_size, GFP_KERNEL);
		if (!buffer) {
			return -ENOMEM;
		}
		memset(buffer, 0, buf_size);
		for (i = 0; i < cmd->msg.tx_len; i++) {
			count += snprintf(buffer + count, buf_size - count, "0x%02X ", buf[i]);
		}
		if (count == buf_size - 1) {
			buffer[count] = '\0';
			DISP_INFO("register debug log : %s\n", buffer);
			kfree(buffer);
		} else {
			kfree(buffer);
			return -EINVAL;
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

	DISP_DEBUG("%s length = %d\n", cmd_set_update_map[type], length);

	for (j = DSI_CMD_SET_PRE_ON; j < DSI_CMD_SET_MAX; j++) {
		if (strstr(cmd_set_update_map[type], cmd_set_prop_map[j])) {
			type1 = j;
			DISP_DEBUG("find type(%d) is [%s] commands\n", type1,
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
		DISP_DEBUG("update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
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

	DISP_DEBUG("WxH: %dx%d, FPS: %d\n", mode->timing.h_active,
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

		len = dsi_host_transfer_sub(panel->host, cmds, false);
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

	rc = dsi_panel_tx_cmd_set(panel, type, false);

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
		mi_cfg->last_doze_brightness = mi_cfg->doze_brightness;
		mi_cfg->doze_brightness = doze_brightness;
		DISP_INFO("Skip! [%s] set doze brightness %d due to FOD_HBM_ON\n",
			panel->type, doze_brightness);
	} else if (panel->mi_cfg.panel_state == PANEL_STATE_ON
		|| mi_cfg->doze_brightness != doze_brightness) {
		if (doze_brightness == DOZE_BRIGHTNESS_HBM) {
			panel->mi_cfg.panel_state = PANEL_STATE_DOZE_HIGH;
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM, false);
			if (rc) {
				DISP_ERROR("[%s] failed to send DOZE_HBM cmd, rc=%d\n",
					panel->type, rc);
			}
		} else if (doze_brightness == DOZE_BRIGHTNESS_LBM) {
			panel->mi_cfg.panel_state = PANEL_STATE_DOZE_LOW;
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM, false);
			if (rc) {
				DISP_ERROR("[%s] failed to send DOZE_LBM cmd, rc=%d\n",
					panel->type, rc);
			}
		}
		mi_cfg->last_doze_brightness = mi_cfg->doze_brightness;
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

	rc = dsi_panel_create_cmd_packets(ctl->tx_ptr, ctl->tx_len, packet_count,
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

	mi_cfg = &panel->mi_cfg;

	if (brightness_clone > mi_cfg->max_brightness_clone)
		brightness_clone = mi_cfg->max_brightness_clone;

	atomic_set(&mi_cfg->brightness_clone, brightness_clone);

	disp_id = mi_get_disp_id(panel->type);
	mi_disp_feature_event_notify_by_type(disp_id,
			MI_DISP_EVENT_BRIGHTNESS_CLONE,
			sizeof(u32), atomic_read(&mi_cfg->brightness_clone));

	return rc;
}

int mi_dsi_panel_get_brightness_clone(struct dsi_panel *panel,
			u32 *brightness_clone)
{
	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	*brightness_clone = atomic_read(&panel->mi_cfg.brightness_clone);

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
	DISP_INFO("[%s] mi_dsi_update_backlight_in_aod bl_lvl=%d\n",
			panel->type, bl_lvl);
	if (panel->bl_config.bl_inverted_dbv)
		bl_lvl = (((bl_lvl & 0xff) << 8) | (bl_lvl >> 8));
	mipi_dsi_dcs_set_display_brightness(dsi, bl_lvl);

	return;
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
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_ON, false);
		rc |= dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_SEC_ON, false);
	} else {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_OFF, false);
		rc |= dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_SEC_OFF, false);
	}
	DISP_INFO("send flat %s cmd immediately", enable ? "ON" : "OFF");
#else
	/*flat cmd will be send out at mi_sde_connector_flat_fence*/
	DISP_DEBUG("flat cmd should send out sync with te");
#endif
	return rc;
}

int mi_dsi_panel_csc_by_temper_comp(struct dsi_panel *panel, int temp_val)
{
	int rc = 0;
	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	mutex_lock(&panel->panel_lock);
	rc = mi_dsi_panel_csc_by_temper_comp_locked(panel, temp_val);
	if (mi_get_panel_id_by_dsi_panel(panel) == O10U_PANEL_PA) {
		rc = mi_dsi_panel_nova_csc_by_temper_comp_locked(panel, temp_val);
	}
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int mi_dsi_panel_csc_by_temper_comp_locked(struct dsi_panel *panel, int temp_val)
{
	int rc = 0;
	int real_temp_val = temp_val;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	mi_cfg = &panel->mi_cfg;
	if (mi_get_panel_id_by_dsi_panel(panel) != O2_PANEL_PA && mi_get_panel_id_by_dsi_panel(panel) != O1_PANEL_PA &&
		mi_get_panel_id_by_dsi_panel(panel) != O11U_PANEL_PA) {
		return rc;
	}
	if (panel->id_config.build_id < PANEL_P20) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CSC_BY_TEMPER_COMP_OFF_MODE, false);
		return rc;
	}
	if (temp_val < 32) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CSC_BY_TEMPER_COMP_OFF_MODE, false);
	} else if (temp_val >= 32 && temp_val < 36) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CSC_BY_TEMPER_COMP_32_36_MODE, false);
	} else if (temp_val >= 36 && temp_val < 40) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CSC_BY_TEMPER_COMP_36_40_MODE, false);
	} else if ((mi_get_panel_id_by_dsi_panel(panel) == O2_PANEL_PA || mi_get_panel_id_by_dsi_panel(panel) == O1_PANEL_PA
			|| mi_get_panel_id_by_dsi_panel(panel) == O11U_PANEL_PA) && temp_val >= 40 && temp_val < 45) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CSC_BY_TEMPER_COMP_40_MODE, false);
	} else if ((mi_get_panel_id_by_dsi_panel(panel) == O2_PANEL_PA || mi_get_panel_id_by_dsi_panel(panel) == O1_PANEL_PA
			|| mi_get_panel_id_by_dsi_panel(panel) == O11U_PANEL_PA) && temp_val >= 45) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CSC_BY_TEMPER_COMP_45_MODE, false);
	} else {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CSC_BY_TEMPER_COMP_40_MODE, false);
	}
	mi_cfg->real_dbi_state = real_temp_val;
	return rc;
}

int mi_dsi_panel_gamma_by_temper_comp(struct dsi_panel *panel, int temp_val)
{
	int rc = 0;
	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	mutex_lock(&panel->panel_lock);
	rc = mi_dsi_panel_gamma_by_temper_comp_locked(panel, temp_val);
	mutex_unlock(&panel->panel_lock);
	return rc;
}

int mi_dsi_panel_gamma_by_temper_comp_locked(struct dsi_panel *panel, int temp_val)
{
	int rc = 0;
	int real_temp_val = temp_val;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	mi_cfg = &panel->mi_cfg;

	if (temp_val < 27) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_GAMMA_BY_TEMPER_COMP_OFF_MODE, false);
	} else if (temp_val >= 28 && temp_val < 32) {
		rc = dsi_panel_tx_cmd_set(panel,DSI_CMD_SET_MI_GAMMA_BY_TEMPER_COMP_28_32_MODE, false);
	} else if (temp_val >= 33 && temp_val < 38) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_GAMMA_BY_TEMPER_COMP_33_38_MODE, false);
	} else if (temp_val >= 39 && temp_val < 42) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_GAMMA_BY_TEMPER_COMP_39_42_MODE, false);
	} else if (temp_val >= 43 && temp_val < 45) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_GAMMA_BY_TEMPER_COMP_ABOVE_42_MODE, false);
	} else {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_GAMMA_BY_TEMPER_COMP_ABOVE_42_MODE, false);
	}
	mi_cfg->real_dbi_state = real_temp_val;
	return rc;
}

int mi_dsi_panel_nova_csc_by_temper_comp_locked(struct dsi_panel *panel, int temp_val)
{
	int rc = 0;
	int real_temp_val = temp_val;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	mi_cfg = &panel->mi_cfg;
	if (mi_get_panel_id_by_dsi_panel(panel) != O10U_PANEL_PA) {
		return rc;
	}

	if (temp_val < 26) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CSC_BY_TEMPER_COMP_OFF_MODE, false);
	} else if (temp_val >= 26 && temp_val < 28) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CSC_BY_TEMPER_COMP_26_28_MODE, false);
	} else if (temp_val >= 28 && temp_val < 30) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CSC_BY_TEMPER_COMP_28_30_MODE, false);
	} else if (temp_val >= 30 && temp_val < 32) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CSC_BY_TEMPER_COMP_30_32_MODE, false);
	} else if (temp_val >= 32 && temp_val < 34) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CSC_BY_TEMPER_COMP_32_34_MODE, false);
	} else if (temp_val >= 34 && temp_val < 36) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CSC_BY_TEMPER_COMP_34_36_MODE, false);
	} else if (temp_val >= 36 && temp_val < 38) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CSC_BY_TEMPER_COMP_36_38_MODE, false);
	} else if (temp_val >= 38 && temp_val < 40) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CSC_BY_TEMPER_COMP_38_40_MODE, false);
	} else {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_CSC_BY_TEMPER_COMP_40_MODE, false);
	}
	mi_cfg->real_dbi_state = real_temp_val;
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
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_ROUND_CORNER_ON, false);
		else
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_ROUND_CORNER_OFF, false);

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

int mi_dsi_panel_set_fp_unlock_state(struct dsi_panel *panel,
			u32 fp_unlock_value)
{
	int rc = 0;
	int update_bl = 0;

	mutex_lock(&panel->panel_lock);
	if((mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O1_PANEL_PA ||
		mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O1_PANEL_PB) &&
		fp_unlock_value == FINGERPRINT_UNLOCK_SUCCESS)
		panel->mi_cfg.fp_unlock_success = true;

	if (!dsi_panel_initialized(panel)) {
		DISP_ERROR("panel is not initialized!\n");
		rc = -EINVAL;
		goto exit;
	}

	if ((panel->power_mode == SDE_MODE_DPMS_LP1 ||
  		panel->power_mode == SDE_MODE_DPMS_LP2) &&
		!panel->mi_cfg.fullscreen_aod_status &&
		fp_unlock_value == FINGERPRINT_UNLOCK_SUCCESS) {
		if (mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O3_PANEL_PA) {
			if (panel->mi_cfg.last_no_zero_bl_level < 207) {
				mi_dsi_update_51_mipi_cmd(panel, DSI_CMD_SET_NOLP, update_bl);
				if (mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O3_PANEL_PA) {
					mi_dsi_update_switch_cmd_O3(panel, DSI_CMD_SET_NOLP_UPDATE, DSI_CMD_SET_NOLP);
				}
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_NOLP, false);
			}
		} else if (mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O2_PANEL_PA || mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O1_PANEL_PA
					|| mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O1_PANEL_PB) {
			if (panel->mi_cfg.last_no_zero_bl_level < 412) {
				mi_dsi_update_51_mipi_cmd(panel, DSI_CMD_SET_NOLP, update_bl);
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_NOLP, false);
			}
		}
	}

exit:
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
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DC_ON, false);
			mi_cfg->real_dc_state = FEATURE_ON;
		} else {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DC_OFF, false);
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

bool mi_dsi_panel_need_tx_or_rx_cmd(u32 feature_id)
{
	switch (feature_id) {
	case DISP_FEATURE_SENSOR_LUX:
	case DISP_FEATURE_FOLD_STATUS:
	case DISP_FEATURE_FULLSCREEN_AOD_STATUS:
		return false;
	default:
		return true;
	}
}

static int mi_dsi_panel_update_lhbm_param_O10U_PA(struct dsi_panel * panel,
		enum dsi_cmd_set_type type, int bl_lvl)
{
	struct dsi_display_mode_priv_info *priv_info = NULL;
	struct dsi_cmd_update_info *info = NULL;
	u32 cmd_update_index = 0;
	u32 cmd_update_count = 0;
	u8 bl_index = 0, i = 0;
	u32 tmp;
	u8 d1_reg_buf[6] = {0x24,0xC0,0x22,0x08,0x29,0xAC};

	if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if(!panel->mi_cfg.read_lhbm_gamma_success) {
		DISP_ERROR("read_lhbm_gamma failed\n");
		return -EINVAL;
	}

	if (0x0 <= bl_lvl && bl_lvl < 0x11C1) bl_index = 0;
	else if (bl_lvl < 0x1DA0) bl_index = 1;
	else if (bl_lvl < 0x22AB) bl_index = 2;
	else if (bl_lvl < 0x2B11) bl_index = 3;
	else if (bl_lvl < 0x2B74) bl_index = 4;
	else if (bl_lvl < 0x2BD7) bl_index = 5;
	else if (bl_lvl < 0x2C3A) bl_index = 6;
	else if (bl_lvl < 0x2C9D) bl_index = 7;
	else if (bl_lvl < 0x2D00) bl_index = 8;
	else if (bl_lvl < 0x2D63) bl_index = 9;
	else if (bl_lvl < 0x2DC5) bl_index = 10;
	else if (bl_lvl < 0x2E28) bl_index = 11;
	else if (bl_lvl < 0x2E8B) bl_index = 12;
	else if (bl_lvl < 0x2EEE) bl_index = 13;
	else if (bl_lvl < 0x2F51) bl_index = 14;
	else if (bl_lvl < 0x2FB4) bl_index = 15;
	else if (bl_lvl < 0x3017) bl_index = 16;
	else if (bl_lvl < 0x307A) bl_index = 17;
	else if (bl_lvl < 0x30DC) bl_index = 18;
	else if (bl_lvl < 0x313F) bl_index = 19;
	else if (bl_lvl < 0x31A2) bl_index = 20;
	else if (bl_lvl < 0x3205) bl_index = 21;
	else if (bl_lvl < 0x3268) bl_index = 22;
	else if (bl_lvl < 0x32CB) bl_index = 23;
	else if (bl_lvl < 0x332E) bl_index = 24;
	else if (bl_lvl < 0x3390) bl_index = 25;
	else if (bl_lvl <= 0x3FFF) bl_index = 26;

	switch (type) {
		case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT:
			cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT_UPDATE;
			memcpy(&d1_reg_buf[0], &panel->mi_cfg.lhbm_rgb_param[0],sizeof(u8)*6);
			for(i = 1; i < 6; i += 2) {
				tmp = (((d1_reg_buf[i-1]) << 8) | d1_reg_buf[i])*4*(lhbm_o10u_white_high_ratio_pa[bl_index])/1000;
				d1_reg_buf[i-1] = (tmp >> 8) & 0xFF;
				d1_reg_buf[i] = tmp & 0xFF;
			}
			break;
		case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT:
			cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT_UPDATE;
			memcpy(&d1_reg_buf[0], &panel->mi_cfg.lhbm_rgb_param[6],sizeof(u8)*6);
			for(i = 1; i < 6; i += 2) {
				tmp = (((d1_reg_buf[i-1]) << 8) | d1_reg_buf[i])*4*(lhbm_o10u_white_low_ratio_pa[bl_index])/1000;
				d1_reg_buf[i-1] = (tmp >> 8) & 0xFF;
				d1_reg_buf[i] = tmp & 0xFF;
			}
			break;
		case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT:
			cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT_UPDATE;
			memcpy(&d1_reg_buf[0], &panel->mi_cfg.lhbm_rgb_param[0],sizeof(u8)*6);
			for(i = 1; i < 6; i += 2) {
				tmp = (((d1_reg_buf[i-1]) << 8) | d1_reg_buf[i])*4*(lhbm_o10u_white_high_ratio_pa[bl_index])/1000;
				d1_reg_buf[i-1] = (tmp >> 8) & 0xFF;
				d1_reg_buf[i] = tmp & 0xFF;
			}
			break;
		case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT:
			cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT_UPDATE;
			memcpy(&d1_reg_buf[0], &panel->mi_cfg.lhbm_rgb_param[6],sizeof(u8)*6);
			for(i = 1; i < 6; i += 2) {
				tmp = (((d1_reg_buf[i-1]) << 8) | d1_reg_buf[i])*4*(lhbm_o10u_white_low_ratio_pa[bl_index])/1000;
				d1_reg_buf[i-1] = (tmp >> 8) & 0xFF;
				d1_reg_buf[i] = tmp & 0xFF;
			}
			break;
		default:
			DISP_ERROR("[%s] unsupport cmd %s\n", panel->type, cmd_set_prop_map[type]);
			return -EINVAL;
	}

	priv_info = panel->cur_mode->priv_info;
	info = priv_info->cmd_update[cmd_update_index];
	cmd_update_count = priv_info->cmd_update_count[cmd_update_index];
	for (i = 0; i < cmd_update_count; i++) {
		if (info) {
			DISP_INFO("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d) d1_reg_buf[0](0x%02X) bl(%d) bl_index(%d)\n",
				panel->type, cmd_set_prop_map[info->type], info->mipi_address,
				info->index, info->length, d1_reg_buf[0], bl_lvl, bl_index);
			if(info->mipi_address == 0xD1) {
				mi_dsi_panel_update_cmd_set(panel, panel->cur_mode,
						type, info, d1_reg_buf, sizeof(d1_reg_buf));
			}
			info++;
		}
	}

	return 0;
}

static int mi_dsi_panel_update_lhbm_param_O10U_PB(struct dsi_panel * panel,
		enum dsi_cmd_set_type type, int bl_lvl)
{
	struct dsi_display_mode_priv_info *priv_info = NULL;
	struct dsi_cmd_update_info *info = NULL;
	u32 cmd_update_index = 0;
	u32 cmd_update_count = 0;
	u8 bl_index = 0, i = 0;
	u32 tmp;
	u8 d1_reg_buf[6] = {0x26,0xC4,0x24,0x10,0x2C,0x4A};

	if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if(!panel->mi_cfg.read_lhbm_gamma_success) {
		DISP_ERROR("read_lhbm_gamma failed\n");
		return -EINVAL;
	}

	if (0x0 <= bl_lvl && bl_lvl <= 0x0438) bl_index = 0;
	else if (bl_lvl <= 0x05D3) bl_index = 1;
	else if (bl_lvl <= 0x076D) bl_index = 2;
	else if (bl_lvl <= 0x0907) bl_index = 3;
	else if (bl_lvl <= 0x0AA1) bl_index = 4;
	else if (bl_lvl <= 0x0DAF) bl_index = 5;
	else if (bl_lvl <= 0x10BC) bl_index = 6;
	else if (bl_lvl <= 0x13CA) bl_index = 7;
	else if (bl_lvl <= 0x16D7) bl_index = 8;
	else if (bl_lvl <= 0x19E5) bl_index = 9;
	else if (bl_lvl <= 0x1CF2) bl_index = 10;
	else if (bl_lvl <= 0x1FFF) bl_index = 11;
	else if (bl_lvl <= 0x2557) bl_index = 12;
	else if (bl_lvl <= 0x2AAE) bl_index = 13;
	else if (bl_lvl <= 0x2B26) bl_index = 14;
	else if (bl_lvl <= 0x2B9E) bl_index = 15;
	else if (bl_lvl <= 0x2C16) bl_index = 16;
	else if (bl_lvl <= 0x2C8D) bl_index = 17;
	else if (bl_lvl <= 0x2D05) bl_index = 18;
	else if (bl_lvl <= 0x2D7D) bl_index = 19;
	else if (bl_lvl <= 0x2DF4) bl_index = 20;
	else if (bl_lvl <= 0x2E6C) bl_index = 21;
	else if (bl_lvl <= 0x2EE4) bl_index = 22;
	else if (bl_lvl <= 0x2F5B) bl_index = 23;
	else if (bl_lvl <= 0x2FD3) bl_index = 24;
	else if (bl_lvl <= 0x304B) bl_index = 25;
	else if (bl_lvl <= 0x30C2) bl_index = 26;
	else if (bl_lvl <= 0x313A) bl_index = 27;
	else if (bl_lvl <= 0x31B2) bl_index = 28;
	else if (bl_lvl <= 0x3229) bl_index = 29;
	else if (bl_lvl <= 0x32A1) bl_index = 30;
	else if (bl_lvl <= 0x3319) bl_index = 31;
	else if (bl_lvl <= 0x0FFF) bl_index = 32;

	switch (type) {
		case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT:
			cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT_UPDATE;
			memcpy(&d1_reg_buf[0], &panel->mi_cfg.lhbm_rgb_param[0],sizeof(u8)*6);
			for(i = 1; i < 6; i += 2) {
				tmp = (((d1_reg_buf[i-1]) << 8) | d1_reg_buf[i])*4*(lhbm_o10u_white_high_ratio_pb[(i-1)/2][bl_index])/1000;
				d1_reg_buf[i-1] = (tmp >> 8) & 0x3F;
				d1_reg_buf[i] = tmp & 0xFF;
			}
			break;
		case DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT:
			cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT_UPDATE;
			memcpy(&d1_reg_buf[0], &panel->mi_cfg.lhbm_rgb_param[6],sizeof(u8)*6);
			for(i = 1; i < 6; i += 2) {
				tmp = (((d1_reg_buf[i-1]) << 8) | d1_reg_buf[i])*4*(lhbm_o10u_white_low_ratio_pb[(i-1)/2][bl_index])/1000;
				d1_reg_buf[i-1] = (tmp >> 8) & 0x3F;
				d1_reg_buf[i] = tmp & 0xFF;
			}
			break;
		case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT:
			cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT_UPDATE;
			memcpy(&d1_reg_buf[0], &panel->mi_cfg.lhbm_rgb_param[0],sizeof(u8)*6);
			for(i = 1; i < 6; i += 2) {
				tmp = (((d1_reg_buf[i-1]) << 8) | d1_reg_buf[i])*4*(lhbm_o10u_white_high_ratio_pb[(i-1)/2][bl_index])/1000;
				d1_reg_buf[i-1] = (tmp >> 8) & 0x3F;
				d1_reg_buf[i] = tmp & 0xFF;
			}
			break;
		case DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT:
			cmd_update_index = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT_UPDATE;
			memcpy(&d1_reg_buf[0], &panel->mi_cfg.lhbm_rgb_param[6],sizeof(u8)*6);
			for(i = 1; i < 6; i += 2) {
				tmp = (((d1_reg_buf[i-1]) << 8) | d1_reg_buf[i])*4*(lhbm_o10u_white_low_ratio_pb[(i-1)/2][bl_index])/1000;
				d1_reg_buf[i-1] = (tmp >> 8) & 0x3F;
				d1_reg_buf[i] = tmp & 0xFF;
			}
			break;
		default:
			DISP_ERROR("[%s] unsupport cmd %s\n", panel->type, cmd_set_prop_map[type]);
			return -EINVAL;
	}

	priv_info = panel->cur_mode->priv_info;
	info = priv_info->cmd_update[cmd_update_index];
	cmd_update_count = priv_info->cmd_update_count[cmd_update_index];
	for (i = 0; i < cmd_update_count; i++) {
		if (info) {
			DISP_INFO("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d) d1_reg_buf[0](0x%02X) bl(%d) bl_index(%d)\n",
				panel->type, cmd_set_prop_map[info->type], info->mipi_address,
				info->index, info->length, d1_reg_buf[0], bl_lvl, bl_index);
			if(info->mipi_address == 0xD1) {
				mi_dsi_panel_update_cmd_set(panel, panel->cur_mode,
						type, info, d1_reg_buf, sizeof(d1_reg_buf));
			}
			info++;
		}
	}

	return 0;
}

int mi_dsi_panel_set_lhbm_fod_locked(struct dsi_panel *panel,
		struct disp_feature_ctl *ctl)
{
	int rc = 0;
	int update_bl_lvl = 0;
	u32 last_bl_level_store = 0;
	enum dsi_cmd_set_type lhbm_type = DSI_CMD_SET_MAX;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!panel || !ctl || ctl->feature_id != DISP_FEATURE_LOCAL_HBM) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mi_cfg = &panel->mi_cfg;

	if (mi_cfg->feature_val[DISP_FEATURE_FOD_CALIBRATION_BRIGHTNESS] > 0) {
		DISP_TIME_INFO("fod calibration brightness, update alpha,"
				"last bl (%d)-->fod calibration bl(%d)",
				mi_cfg->last_bl_level,
				mi_cfg->feature_val[DISP_FEATURE_FOD_CALIBRATION_BRIGHTNESS]);
		last_bl_level_store = mi_cfg->last_bl_level;
		mi_cfg->last_bl_level = mi_cfg->feature_val[DISP_FEATURE_FOD_CALIBRATION_BRIGHTNESS];
	}

	DISP_TIME_INFO("[%s] Local HBM: %s\n", panel->type,
			get_local_hbm_state_name(ctl->feature_val));

	switch (ctl->feature_val) {
	case LOCAL_HBM_OFF_TO_NORMAL:
		mi_dsi_update_backlight_in_aod(panel, true);
		DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL\n");

		lhbm_type = DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL;

		rc = dsi_panel_tx_cmd_set(panel, lhbm_type, false);

		mi_cfg->dimming_state = STATE_DIM_RESTORE;
		mi_dsi_panel_update_last_bl_level(panel, mi_cfg->last_bl_level);
		break;
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT:
		DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT\n");
		mi_dsi_update_backlight_in_aod(panel, false);
		lhbm_type = DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL;

		DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL\n");
		rc = dsi_panel_tx_cmd_set(panel, lhbm_type, false);

		if (mi_cfg->need_fod_animal_in_normal)
			panel->mi_cfg.aod_to_normal_statue = true;
		mi_cfg->dimming_state = STATE_DIM_RESTORE;
		break;
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE:
		/* display backlight value should equal unlock brightness */
		DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE\n");
		mi_dsi_update_backlight_in_aod(panel, true);
		lhbm_type = DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL;

		DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL\n");
		rc = dsi_panel_tx_cmd_set(panel, lhbm_type, false);
		mi_cfg->dimming_state = STATE_DIM_RESTORE;
		break;
	case LOCAL_HBM_NORMAL_WHITE_1000NIT:
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT\n");
		lhbm_type = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_1000NIT;
		update_bl_lvl = mi_cfg->last_bl_level;
		if (is_aod_and_panel_initialized(panel)) {
			switch (mi_cfg->doze_brightness) {
			case DOZE_BRIGHTNESS_HBM:
				update_bl_lvl = mi_cfg->doze_hbm_dbv_level;
				DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT in doze_hbm_dbv_level\n");
				break;
			case DOZE_BRIGHTNESS_LBM:
				update_bl_lvl = mi_cfg->doze_lbm_dbv_level;
				DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT in doze_lbm_dbv_level\n");
				break;
			default:
				update_bl_lvl = mi_cfg->doze_hbm_dbv_level;
				DISP_INFO("LOCAL_HBM_NORMAL_WHITE_1000NIT use doze_hbm_dbv_level as defaults\n");
				break;
			}
		}
		if (mi_get_panel_id_by_dsi_panel(panel) == O10U_PANEL_PA) {
			mi_dsi_panel_update_lhbm_param_O10U_PA(panel, lhbm_type, update_bl_lvl);
		}
		if (mi_get_panel_id_by_dsi_panel(panel) == O10U_PANEL_PB) {
			mi_dsi_panel_update_lhbm_param_O10U_PB(panel, lhbm_type, update_bl_lvl);
		}
		rc = dsi_panel_tx_cmd_set(panel, lhbm_type, false);
		break;
	case LOCAL_HBM_NORMAL_WHITE_110NIT:
		DISP_DEBUG("LOCAL_HBM_NORMAL_WHITE_110NIT\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		lhbm_type = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_WHITE_110NIT;
		update_bl_lvl = mi_cfg->last_bl_level;
		if (is_aod_and_panel_initialized(panel)) {
			switch (mi_cfg->doze_brightness) {
			case DOZE_BRIGHTNESS_HBM:
				update_bl_lvl = mi_cfg->doze_hbm_dbv_level;
				DISP_INFO("LOCAL_HBM_NORMAL_WHITE_110NIT in doze_hbm_dbv_level\n");
				break;
			case DOZE_BRIGHTNESS_LBM:
				update_bl_lvl = mi_cfg->doze_lbm_dbv_level;
				DISP_INFO("LOCAL_HBM_NORMAL_WHITE_110NIT in doze_lbm_dbv_level\n");
				break;
			default:
				update_bl_lvl = mi_cfg->doze_lbm_dbv_level;
				DISP_INFO("LOCAL_HBM_NORMAL_WHITE_110NIT use doze_lbm_dbv_level as defaults\n");
				break;
			}
		}
		if (mi_get_panel_id_by_dsi_panel(panel) == O10U_PANEL_PA) {
			mi_dsi_panel_update_lhbm_param_O10U_PA(panel, lhbm_type, update_bl_lvl);
		}
		if (mi_get_panel_id_by_dsi_panel(panel) == O10U_PANEL_PB) {
			mi_dsi_panel_update_lhbm_param_O10U_PB(panel, lhbm_type, update_bl_lvl);
		}
		rc = dsi_panel_tx_cmd_set(panel, lhbm_type, false);
		break;
	case LOCAL_HBM_NORMAL_GREEN_500NIT:
		DISP_INFO("LOCAL_HBM_NORMAL_GREEN_500NIT\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		lhbm_type = DSI_CMD_SET_MI_LOCAL_HBM_NORMAL_GREEN_500NIT;
		rc = dsi_panel_tx_cmd_set(panel, lhbm_type, false);
		break;
	case LOCAL_HBM_HLPM_WHITE_1000NIT:
		DISP_DEBUG("LOCAL_HBM_HLPM_WHITE_1000NIT\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		lhbm_type = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_1000NIT;
		update_bl_lvl = mi_cfg->last_bl_level;

		if (is_aod_and_panel_initialized(panel)) {
			switch (mi_cfg->doze_brightness) {
			case DOZE_BRIGHTNESS_HBM:
				update_bl_lvl = mi_cfg->doze_hbm_dbv_level;
				DISP_INFO("LOCAL_HBM_HLPM_WHITE_1000NIT in doze_hbm_dbv_level\n");
				break;
			case DOZE_BRIGHTNESS_LBM:
				update_bl_lvl = mi_cfg->doze_lbm_dbv_level;
				DISP_INFO("LOCAL_HBM_HLPM_WHITE_1000NIT in doze_lbm_dbv_level\n");
				break;
			default:
				update_bl_lvl = mi_cfg->doze_hbm_dbv_level;
				DISP_INFO("LOCAL_HBM_HLPM_WHITE_1000NIT use doze_hbm_dbv_level as defaults\n");
				break;
			}
		}

		if (mi_get_panel_id_by_dsi_panel(panel) == O10U_PANEL_PA) {
			mi_dsi_panel_update_lhbm_param_O10U_PA(panel, lhbm_type, update_bl_lvl);
		}
		if (mi_get_panel_id_by_dsi_panel(panel) == O10U_PANEL_PB) {
			mi_dsi_panel_update_lhbm_param_O10U_PB(panel, lhbm_type, update_bl_lvl);
		}
		mi_dsi_update_backlight_in_aod(panel, false);
		rc = dsi_panel_tx_cmd_set(panel, lhbm_type, false);

		panel->mi_cfg.panel_state = PANEL_STATE_ON;
		break;
	case LOCAL_HBM_HLPM_WHITE_110NIT:
		DISP_DEBUG("LOCAL_HBM_HLPM_WHITE_110NIT\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		lhbm_type = DSI_CMD_SET_MI_LOCAL_HBM_HLPM_WHITE_110NIT;
		update_bl_lvl = mi_cfg->last_bl_level;

		if (is_aod_and_panel_initialized(panel)) {
			switch (mi_cfg->doze_brightness) {
			case DOZE_BRIGHTNESS_HBM:
				update_bl_lvl = mi_cfg->doze_hbm_dbv_level;
				DISP_INFO("LOCAL_HBM_HLPM_WHITE_110NIT in doze_hbm_dbv_level\n");
				break;
			case DOZE_BRIGHTNESS_LBM:
				update_bl_lvl = mi_cfg->doze_lbm_dbv_level;
				DISP_INFO("LOCAL_HBM_HLPM_WHITE_110NIT in doze_lbm_dbv_level\n");
				break;
			default:
				update_bl_lvl = mi_cfg->doze_lbm_dbv_level;
				DISP_INFO("LOCAL_HBM_HLPM_WHITE_1000NIT use doze_lbm_dbv_level as defaults\n");
				break;
			}
		}
		if (mi_get_panel_id_by_dsi_panel(panel) == O10U_PANEL_PA) {
			mi_dsi_panel_update_lhbm_param_O10U_PA(panel, lhbm_type, update_bl_lvl);
		}
		if (mi_get_panel_id_by_dsi_panel(panel) == O10U_PANEL_PB) {
			mi_dsi_panel_update_lhbm_param_O10U_PB(panel, lhbm_type, update_bl_lvl);
		}
		mi_dsi_update_backlight_in_aod(panel, false);
		rc = dsi_panel_tx_cmd_set(panel, lhbm_type, false);
		panel->mi_cfg.panel_state = PANEL_STATE_ON;
		break;
	default:
		DISP_ERROR("invalid local hbm value\n");
		break;
	}

	if (mi_cfg->feature_val[DISP_FEATURE_FOD_CALIBRATION_BRIGHTNESS] > 0)
		mi_cfg->last_bl_level = last_bl_level_store;

	return rc;
}

int mi_dsi_panel_set_disp_param(struct dsi_panel *panel, struct disp_feature_ctl *ctl)
{
	int rc = 0;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	struct panel_event_notification notification = {0};
	enum panel_event_notifier_tag panel_type = PANEL_EVENT_NOTIFICATION_PRIMARY;

	if (!panel || !ctl) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	DISP_TIME_INFO("[%s] feature: %s, value: %d\n", panel->type,
			get_disp_feature_id_name(ctl->feature_id), ctl->feature_val);


	mi_cfg = &panel->mi_cfg;

	if (!panel->panel_initialized &&
		mi_dsi_panel_need_tx_or_rx_cmd(ctl->feature_id)) {
		if (ctl->feature_id == DISP_FEATURE_DC)
			mi_cfg->feature_val[DISP_FEATURE_DC] = ctl->feature_val;
		if (ctl->feature_id == DISP_FEATURE_DBI)
  			mi_cfg->feature_val[DISP_FEATURE_DBI] = ctl->feature_val;
		DISP_WARN("[%s] panel not initialized!\n", panel->type);
		rc = -ENODEV;
		goto exit;
	}
	if (!strncmp(panel->type, "primary", 7)){
		panel_type = PANEL_EVENT_NOTIFICATION_PRIMARY;
	} else {
		panel_type = PANEL_EVENT_NOTIFICATION_SECONDARY;
	}
	notification.panel = &panel->drm_panel;
	notification.notif_data.old_fps = 0;
	notification.notif_data.new_fps = 0;
	notification.notif_data.early_trigger = true;
	switch (ctl->feature_id) {
	case DISP_FEATURE_DIMMING:
		if (!mi_cfg->disable_ic_dimming){
			if (mi_cfg->dimming_state != STATE_DIM_BLOCK) {
				mi_cfg->ic_dimming_by_feature = ctl->feature_val;
				if (ctl->feature_val == FEATURE_ON )
					rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGON, false);
				else
					rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGOFF, false);
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
			notification.notif_type = DRM_PANEL_EVENT_HBM_ON;
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_ON, false);
			mi_cfg->dimming_state = STATE_DIM_BLOCK;
		} else {
			notification.notif_type = DRM_PANEL_EVENT_HBM_OFF;
			mi_dsi_update_51_mipi_cmd(panel, DSI_CMD_SET_MI_HBM_OFF,
					mi_cfg->last_bl_level);
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_HBM_OFF, false);
			mi_cfg->dimming_state = STATE_DIM_RESTORE;
		}
		panel_event_notification_trigger(panel_type, &notification);
#endif
		mi_disp_feature_event_notify_by_type(mi_get_disp_id(panel->type),
				MI_DISP_EVENT_HBM, sizeof(ctl->feature_val), ctl->feature_val);
		break;
	case DISP_FEATURE_DOZE_BRIGHTNESS:
#ifdef CONFIG_FACTORY_BUILD
		if (dsi_panel_initialized(panel) &&
			is_aod_brightness(ctl->feature_val)) {
			if (ctl->feature_val == DOZE_BRIGHTNESS_HBM)
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM, false);
			else
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM, false);
		} else {
			if (mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O3_PANEL_PA) {
				mi_dsi_update_switch_cmd_O3(panel, DSI_CMD_SET_NOLP_UPDATE,
							DSI_CMD_SET_NOLP);
			}
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_NOLP, false);
			dsi_panel_update_backlight(panel, mi_cfg->last_bl_level);
		}
#else
		if (is_aod_and_panel_initialized(panel) &&
			is_aod_brightness(ctl->feature_val)) {
			if (ctl->feature_val == DOZE_BRIGHTNESS_HBM) {
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_HBM, false);
			} else {
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DOZE_LBM, false);
			}
		} else {
			if (mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O3_PANEL_PA) {
				mi_dsi_update_switch_cmd_O3(panel, DSI_CMD_SET_NOLP_UPDATE,
							DSI_CMD_SET_NOLP);
			}
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_NOLP, false);
		}
#endif
		mi_cfg->feature_val[DISP_FEATURE_DOZE_BRIGHTNESS] = ctl->feature_val;
		break;
	case DISP_FEATURE_FLAT_MODE:
		if (!mi_cfg->flat_sync_te) {
			if (ctl->feature_val == FEATURE_ON) {
				DISP_INFO("flat mode on\n");
				mi_dsi_update_flat_mode_on_cmd(panel, DSI_CMD_SET_MI_FLAT_MODE_ON);
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_ON, false);
				rc |= dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_SEC_ON, false);
			}
			else {
				DISP_INFO("flat mode off\n");
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_OFF, false);
				rc |= dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_FLAT_MODE_SEC_OFF, false);
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
	case DISP_FEATURE_LOCAL_HBM:
		rc = mi_dsi_panel_set_lhbm_fod_locked(panel, ctl);
		mi_cfg->feature_val[DISP_FEATURE_LOCAL_HBM] = ctl->feature_val;
		break;
	case DISP_FEATURE_FOD_CALIBRATION_BRIGHTNESS:
		if (ctl->feature_val == -1) {
			DISP_INFO("FOD calibration brightness restore last_bl_level=%d\n",
				mi_cfg->last_bl_level);
			dsi_panel_update_backlight(panel, mi_cfg->last_bl_level);
			mi_cfg->in_fod_calibration = false;
		} else {
			if (ctl->feature_val >= 0 &&
				ctl->feature_val <= panel->bl_config.bl_max_level) {
				mi_cfg->in_fod_calibration = true;
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGOFF, false);
				dsi_panel_update_backlight(panel, ctl->feature_val);
				mi_cfg->dimming_state = STATE_NONE;
			} else {
				mi_cfg->in_fod_calibration = false;
				DISP_ERROR("FOD calibration invalid brightness level:%d\n",
						ctl->feature_val);
			}
		}
		mi_cfg->feature_val[DISP_FEATURE_FOD_CALIBRATION_BRIGHTNESS] = ctl->feature_val;
		break;
	case DISP_FEATURE_FP_STATUS:
		mi_cfg->feature_val[DISP_FEATURE_FP_STATUS] = ctl->feature_val;
		mi_disp_feature_event_notify_by_type(mi_get_disp_id(panel->type),
				MI_DISP_EVENT_FP, sizeof(ctl->feature_val), ctl->feature_val);
		break;
	case DISP_FEATURE_SENSOR_LUX:
		DISP_DEBUG("DISP_FEATURE_SENSOR_LUX=%d\n", ctl->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_SENSOR_LUX] = ctl->feature_val;
		break;
	case DISP_FEATURE_FOLD_STATUS:
		DISP_INFO("DISP_FEATURE_FOLD_STATUS=%d\n", ctl->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_FOLD_STATUS] = ctl->feature_val;
		break;
	case DISP_FEATURE_NATURE_FLAT_MODE:
		DISP_INFO("TODO\n");
		break;
	case DISP_FEATURE_SPR_RENDER:
		DISP_INFO("TODO\n");
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

		if ((mi_get_panel_id_by_dsi_panel(panel) == O2_PANEL_PA || mi_get_panel_id_by_dsi_panel(panel) == O11U_PANEL_PA ||
			mi_get_panel_id_by_dsi_panel(panel) == O1_PANEL_PA) &&
			(panel->id_config.build_id < PANEL_P20)) {
			rc = mi_dsi_panel_csc_by_temper_comp_locked(panel, 0);
		}
		if ((mi_get_panel_id_by_dsi_panel(panel) == O2_PANEL_PA || mi_get_panel_id_by_dsi_panel(panel) == O11U_PANEL_PA ||
			mi_get_panel_id_by_dsi_panel(panel) == O1_PANEL_PA) &&
			(panel->id_config.build_id >= PANEL_P20)) {
			rc = mi_dsi_panel_csc_by_temper_comp_locked(panel, ctl->feature_val);
			DISP_DEBUG("[%s] csc setting, rc=[%d]\n", panel->type, rc);
		}
		if (mi_get_panel_id_by_dsi_panel(panel) == O3_PANEL_PB) {
			rc = mi_dsi_panel_gamma_by_temper_comp_locked(panel, ctl->feature_val);
			DISP_DEBUG("[%s] gamma setting, rc=[%d]\n", panel->type, rc);
		}
		if (mi_get_panel_id_by_dsi_panel(panel) == O10U_PANEL_PA) {
			rc = mi_dsi_panel_nova_csc_by_temper_comp_locked(panel, ctl->feature_val);
			DISP_DEBUG("[%s] csc setting, rc=[%d]\n", panel->type, rc);
		}
		mi_cfg->feature_val[DISP_FEATURE_DBI] = ctl->feature_val;
		break;
	case DISP_FEATURE_DDIC_ROUND_CORNER:
		DISP_INFO("DDIC round corner state:%d\n", ctl->feature_val);
		rc = mi_dsi_panel_set_round_corner_locked(panel, ctl->feature_val == FEATURE_ON);
		mi_cfg->feature_val[DISP_FEATURE_DDIC_ROUND_CORNER] = ctl->feature_val;
		break;
	case DISP_FEATURE_HBM_BACKLIGHT:
		DISP_INFO("hbm backlight:%d\n", ctl->feature_val);
		if(panel->bl_config.bl_normal_max &&
			ctl->feature_val > panel->bl_config.bl_normal_max &&
			panel->mi_cfg.last_bl_level <= panel->bl_config.bl_normal_max){
			notification.notif_type = DRM_PANEL_EVENT_HBM_ON;
			panel_event_notification_trigger(panel_type, &notification);
		} else if(panel->bl_config.bl_normal_max &&
			ctl->feature_val <= panel->bl_config.bl_normal_max &&
			panel->mi_cfg.last_bl_level > panel->bl_config.bl_normal_max){
			notification.notif_type = DRM_PANEL_EVENT_HBM_OFF;
			panel_event_notification_trigger(panel_type, &notification);
		}
		panel->mi_cfg.last_bl_level = ctl->feature_val;
		dsi_panel_update_backlight(panel, panel->mi_cfg.last_bl_level);
		break;
	case DISP_FEATURE_BACKLIGHT:
#ifdef CONFIG_FACTORY_BUILD
		DISP_INFO("backlight:%d\n", ctl->feature_val);
		if(panel->bl_config.bl_normal_max &&
			ctl->feature_val > panel->bl_config.bl_normal_max &&
			panel->mi_cfg.last_bl_level <= panel->bl_config.bl_normal_max){
			notification.notif_type = DRM_PANEL_EVENT_HBM_ON;
			panel_event_notification_trigger(panel_type, &notification);
		} else if(panel->bl_config.bl_normal_max &&
			ctl->feature_val <= panel->bl_config.bl_normal_max &&
			panel->mi_cfg.last_bl_level > panel->bl_config.bl_normal_max){
			notification.notif_type = DRM_PANEL_EVENT_HBM_OFF;
			panel_event_notification_trigger(panel_type, &notification);
		}
		panel->mi_cfg.last_bl_level = ctl->feature_val;
		dsi_panel_update_backlight(panel, panel->mi_cfg.last_bl_level);
#else
		panel->mi_cfg.last_bl_level = ctl->feature_val;
		DISP_INFO("last_bl_level:%d\n", panel->mi_cfg.last_bl_level);
#endif
		break;
	case DISP_FEATURE_FULLSCREEN_AOD_STATUS:
		DISP_INFO("fullscreen aod status:%d\n", ctl->feature_val);
		mi_cfg->feature_val[DISP_FEATURE_FULLSCREEN_AOD_STATUS] = ctl->feature_val;
		mi_disp_feature_event_notify_by_type(mi_get_disp_id(panel->type),
				MI_DISP_EVENT_FULLSCREEN_AOD, sizeof(ctl->feature_val), ctl->feature_val);
		panel->mi_cfg.fullscreen_aod_status = ctl->feature_val;
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

	if ((mi_get_panel_id(panel->mi_cfg.mi_panel_id) != O1_PANEL_PA) &&
		(mi_get_panel_id(panel->mi_cfg.mi_panel_id) != O1_PANEL_PB) &&
		 panel->mi_cfg.panel_state == PANEL_STATE_ON) {
		DISP_INFO("panel already PANEL_STATE_ON, skip nolp");
		goto exit;
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

	if ((mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O1_PANEL_PA) ||
		(mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O1_PANEL_PB) ||
		(mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O2_PANEL_PA) ||
		(mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O3_PANEL_PB) ||
		(mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O11U_PANEL_PA)) {
			update_bl = panel->mi_cfg.last_bl_level;
	} else if (mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O3_PANEL_PA) {
		if (panel->mi_cfg.last_bl_level == 0) {
			if (panel->mi_cfg.last_no_zero_bl_level < update_bl)
				update_bl = panel->mi_cfg.last_bl_level;
			else
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGON, false);
		} else {
			if (panel->mi_cfg.last_bl_level > update_bl)
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGON, false);
			update_bl = panel->mi_cfg.last_bl_level;
		}
		if (!panel->mi_cfg.disable_ic_dimming) {
			mi_dsi_panel_tigger_dimming_delayed_work(panel);
		}
	}
	mi_dsi_update_51_mipi_cmd(panel, DSI_CMD_SET_NOLP, update_bl);

	if (mi_get_panel_id(panel->mi_cfg.mi_panel_id) == O3_PANEL_PA) {
		mi_dsi_update_switch_cmd_O3(panel, DSI_CMD_SET_NOLP_UPDATE, DSI_CMD_SET_NOLP);
	}

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_NOLP, false);
exit:
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
		break;
	case DISP_COUNT_INFO_SYSTEM_BUILD_VERSION:
		DISP_INFO("system build version:%s\n", count_info->tx_ptr);
		break;
	case DISP_COUNT_INFO_FRAME_DROP_COUNT:
		DISP_INFO("frame drop count:%d\n", count_info->count_info_val);
		break;
	case DISP_COUNT_INFO_SWITCH_KERNEL_FUNCTION_TIMER:
		DISP_INFO("swith function timer:%d\n", count_info->count_info_val);
		break;
	default:
		break;
	}

	return rc;
}

static const struct {
	u32 framerate;
	int cfg_index;
} mafr_framerate_map[] = {
	{120, 0},  // auto 120
	{60, 1},   // auto 120
	{101, 2},  // auto 10hz: 120 60 30 10
	{102, 3},  // auto 10hz: 60 30 10
	{1, 4},    // auto 1hz:  60 30 10 1
};

static const struct {
	u32 framerate;
	int cfg_index;
} debug_mafr_framerate_map[] = {
	{120, 8},  // normal 120hz
	{60, 7},   // normal 60hz
	{10, 2},   // auto 10hz: 120 60 30 10
};

/*
 * Function: mi_dsi_panel_get_mafr_fps_index
 * Description: This function is used to get the MAFR (Multi-Area Frame Rate) fps index
 *			  based on the current display mode and the DDIC mode of
 * 				a DSI (Display Serial Interface) panel.
 * Parameters:
 *	  panel - Pointer to the DSI panel structure containing information
 * 				about the panel and the current display mode.
 * Return:
 *	  On success, returns the MAFR fps index; on failure, returns -EINVAL.
 * Note:
 *	  The function calculates the current framerate based on the DDIC mode
 * 		and selects the appropriate MAFR FPS index from a map table.
 * 		It handles unsupported DDIC modes and unsupported framerates by returning -EINVAL.
 */
int mi_dsi_panel_get_mafr_fps_index(struct dsi_panel *panel, enum mafr_area_index index)
{
	struct dsi_display_mode *cur_mode = NULL;
	struct mi_mode_info *mi_timing  = NULL;
	int mafr_fps_index = 0;
	u32 cur_framerate = 0;

	if (!panel || !panel->cur_mode ) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	cur_mode = panel->cur_mode;
	mi_timing = &cur_mode->mi_timing;

	// Select the current framerate based on the DDIC mode
	if (!panel->mi_cfg.mafr_debug_switch) {
		switch (mi_timing->ddic_mode) {
			case DDIC_MODE_NORMAL:
				cur_framerate = cur_mode->timing.refresh_rate;
				if (index == MAFR_FIRST_AREA || index == MAFR_THIRD_AREA) {
					cur_framerate = 101;
				}
				break;
			case DDIC_MODE_IDLE:
				cur_framerate = mi_timing->ddic_min_refresh_rate;
				break;
			case DDIC_MODE_AUTO:
			case DDIC_MODE_QSYNC:
				cur_framerate = mi_timing->sf_refresh_rate;
				if (index == MAFR_FIRST_AREA || index == MAFR_THIRD_AREA) {
					if (cur_framerate == 120) {
						cur_framerate = 101;
					} else if (cur_framerate == 60) {
						cur_framerate = 102;
					}
				}
				break;
			default:
				DISP_ERROR("unsupported ddic_mode\n");
				return -EINVAL;
		}
		if (index == MAFR_SECOND_AREA){
			DISP_INFO("get mafr ddic_mode: %d, fps change from: %d hz -> %d hz,\n",
				mi_timing->ddic_mode, panel->mi_cfg.second_mafr_framerate, cur_framerate);
			panel->mi_cfg.second_mafr_framerate = cur_framerate;
		}
		// Find the MAFR FPS index in the map table that matches the current framerate
		size_t map_size = sizeof(mafr_framerate_map) / sizeof(mafr_framerate_map[0]);
		for (size_t i = 0; i < map_size; i++) {
			if (cur_framerate == mafr_framerate_map[i].framerate) {
				mafr_fps_index = mafr_framerate_map[i].cfg_index;
				break;
			}
		}
	} else {
		if (index == MAFR_SECOND_AREA) {
			cur_framerate = panel->mi_cfg.second_mafr_framerate;
		} else if (index == MAFR_FIRST_AREA) {
			cur_framerate = panel->mi_cfg.first_mafr_framerate;
		} else if (index == MAFR_THIRD_AREA) {
			cur_framerate = panel->mi_cfg.third_mafr_framerate;
		}
		size_t map_size = sizeof(debug_mafr_framerate_map) / sizeof(debug_mafr_framerate_map[0]);
		for (size_t i = 0; i < map_size; i++) {
			if (cur_framerate == debug_mafr_framerate_map[i].framerate) {
				mafr_fps_index = debug_mafr_framerate_map[i].cfg_index;
				break;
			}
		}
	}
	// Return the MAFR FPS index
	return mafr_fps_index;
}

/*
* Function: mi_dsi_panel_update_mafr_status
 * Description: Updates the Multi-Area Frame Rate (MAFR) status for the DSI panel.
 * Parameters:
 *	  panel - Pointer to the DSI panel structure.
 *	  roi - Pointer to the region of interest (ROI) rectangle.
 * Return:
 *	  Returns 0 on success, or a negative error code on failure.
 * Notes:
 *	  This function configures the MAFR mode based on the panel's current mode and the given ROI.
 *	  It sends commands to disable and then enable MAFR mode, updating the relevant configuration
 *	  and area settings based on the panel's FPS index and the ROI provided.
 */
int mi_dsi_panel_update_mafr_status(struct dsi_panel *panel, struct dsi_rect *roi)
{
	if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info || !roi) {
		DISP_ERROR("[roi] invalid params\n");
		return -ENODATA;
	}

	if (panel->mi_cfg.mafr_debug_switch) { //debug: 1 0
		bool mafr_enable = panel->mi_cfg.mafr_debug_feature_enable;
		if (!mafr_enable) {
			// disable mafr
			int ret = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DISABLE_MAFR_MODE, false);
			DSI_INFO("success to send disable mafr cmds by debug.\n");
			if (ret) {
				DSI_ERR("[%s] failed to send DISABLE_MAFR_MODE cmds, rc=%d\n",
						panel->name, ret);
			}
			return ret;
		}
	} else {
		if (!panel->mi_cfg.mafr_enable_flag_) {
			mutex_lock(&panel->panel_lock);
			int ret = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DISABLE_MAFR_MODE, false);
			DSI_INFO("success to send disable mafr mode cmds.\n");
			if (ret) {
				DSI_ERR("[%s] failed to send DISABLE_MAFR_MODE cmds, rc=%d\n",
						panel->name, ret);
			}
			mutex_unlock(&panel->panel_lock);
			return ret;
		}
	}

	struct dsi_cmd_update_info *info = NULL;
	enum dsi_cmd_set_type cmd_type = DSI_CMD_SET_MI_ENABLE_MAFR_MODE;
	u32 cmd_update_index = DSI_CMD_SET_MI_ENABLE_MAFR_MODE_UPDATE;
	info = panel->cur_mode->priv_info->cmd_update[cmd_update_index];
	u8 cmd_update_count = panel->cur_mode->priv_info->cmd_update_count[cmd_update_index];

	u8 mi_mafr_mode[2] = {0x01, 0x05}; // link 2B: 05; no link 2B: 01
	u8 mi_mafr_area[4] = {0x00, 0x00, 0x0C, 0x7F};
	u8 mi_mafr_cfg[9][14] = {
		/*index=0  auto 120HZ */
		{0x01,0x01,0x01,0x01,0x03,0x00,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x03},
		/*index=1  auto 60HZ */
		{0x01,0x03,0x01,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x0B},
		/*index=2  auto 120 60 30 10HZ */
		{0x01,0x01,0x01,0x01,0x03,0x01,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x0B},
		/*index=3  auto 60hz 30 10HZ */
		{0x01,0x03,0x01,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x0B},
		/*index=4  idle 1HZ */
		{0x01,0x01,0x01,0x01,0x03,0x01,0x01,0x04,0x01,0x01,0x0B,0x1E,0x01,0x77},
		/*index=5  normal 10HZ, NO DIMMING */
		{0x01,0x0B,0x01,0x01,0x0B,0x01,0x01,0x0B,0x01,0x01,0x0B,0x01,0x01,0x0B},
		/*index=6  normal 30HZ */
		{0x01,0x03,0x01,0x01,0x03,0x01,0x01,0x03,0x01,0x01,0x03,0x01,0x01,0x03},
		/*index=7  normal 60HZ */
		{0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01},
		/*index=8  normal 120HZ */
		{0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x00},
	};

	int mafr_cfg_index = mi_dsi_panel_get_mafr_fps_index(panel, MAFR_SECOND_AREA);
	int mafr_low_fps_index = 5;
	int first_area_6b_index = 10;
	u8 update_length = 0;
	u8 *update_6B_reg_val = NULL;

	for (int i = 0; i < cmd_update_count; i++, info++) {
		if (info->mipi_address == 0x6B) {
			if (info->length == 4) {
				mi_mafr_area[0] = (roi->y & 0xFF00) >> 8;
				mi_mafr_area[1] = roi->y & 0xFF;
				mi_mafr_area[2] = ((roi->y - 1 + roi->h) & 0xFF00) >> 8;
				mi_mafr_area[3] = (roi->y - 1 + roi->h) & 0xFF;
				update_6B_reg_val = mi_mafr_area;
				update_length = 4;
			}

			if (info->length == 14 && (info->index == first_area_6b_index
				|| info->index == first_area_6b_index + 4)) {
				if (!panel->mi_cfg.mafr_debug_switch) {
					panel->mi_cfg.first_mafr_framerate = 10;
					panel->mi_cfg.third_mafr_framerate = 10;
				}
				if (info->index == first_area_6b_index)
					mafr_low_fps_index = mi_dsi_panel_get_mafr_fps_index(panel, MAFR_FIRST_AREA);
				else
					mafr_low_fps_index = mi_dsi_panel_get_mafr_fps_index(panel, MAFR_THIRD_AREA);

				update_6B_reg_val = mi_mafr_cfg[mafr_low_fps_index]; // Default to 1Hz
				update_length = 14;
			}

			if (info->length == 14 && info->index == first_area_6b_index + 2) {
				update_6B_reg_val = mi_mafr_cfg[mafr_cfg_index];
				update_length = 14;
			}
			// update last reg 6B
			// debug mode: using fixed mode: 6B 01;
			if (info->length == 1 && info->index == 17) {
				if (!panel->mi_cfg.mafr_debug_switch) {
					update_6B_reg_val = &mi_mafr_mode[1];
				} else {
					update_6B_reg_val = &mi_mafr_mode[0];
				}
				update_length  = 1;
				DISP_DEBUG("update [%s] mipi_address(0x%02X) index(%d) lenght(%d), "
							"update value 0x%02x.\n",
							cmd_set_prop_map[cmd_type], info->mipi_address,
							info->index, info->length, update_6B_reg_val[0]);
			}
		}

		if (update_6B_reg_val) {
			mi_dsi_panel_update_cmd_set(panel, panel->cur_mode, cmd_type, info,
										update_6B_reg_val, update_length);
			if (info->length != 1 && info->index != 17) {
				DISP_DEBUG("update [%s] mipi_address(0x%02X) index(%d) length(%d), "
							"update value 0x%02x,0x%02x,0x%02x,0x%02x\n",
							cmd_set_prop_map[cmd_type],
							info->mipi_address, info->index, info->length,
							update_6B_reg_val[0], update_6B_reg_val[1],
							update_6B_reg_val[2], update_6B_reg_val[3]);
			}
			update_6B_reg_val = NULL;
		}
	}

	mutex_lock(&panel->panel_lock);
	int ret = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_ENABLE_MAFR_MODE, false);
	DSI_INFO("success to send enable mafr mode cmds.\n");
	if (ret) {
		DSI_ERR("[%s] failed to send DSI_CMD_SET_ROI cmds, rc=%d\n",
				panel->name, ret);
	}
	mutex_unlock(&panel->panel_lock);

	return ret;
}

int mi_dsi_update_switch_cmd_O3(struct dsi_panel *panel, u32 cmd_update_index, u32 index)
{
	struct dsi_cmd_update_info *info = NULL;
	struct dsi_display_mode *cur_mode = NULL;
	int i = 0;
	u8 dc_6C_cfg = 0;
	u8  cmd_update_count = 0;

	if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	cur_mode = panel->cur_mode;

	if (panel->mi_cfg.real_dc_state == FEATURE_ON) {
		dc_6C_cfg = cur_mode->priv_info->dc_on_6Creg;
	} else {
		dc_6C_cfg = cur_mode->priv_info->dc_off_6Creg;
	}

	info = cur_mode->priv_info->cmd_update[cmd_update_index];
	cmd_update_count = cur_mode->priv_info->cmd_update_count[cmd_update_index];

	for (i = 0; i < cmd_update_count; i++ ) {
		DISP_DEBUG("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
			panel->type, cmd_set_prop_map[info->type],
			info->mipi_address, info->index, info->length);
		if (info && info->mipi_address == 0x6C) {
			mi_dsi_panel_update_cmd_set(panel, cur_mode,
				index, info, &dc_6C_cfg, sizeof(dc_6C_cfg));
		}
		info++;
	}

	return 0;
}

int mi_dsi_set_switch_cmd_before(struct dsi_panel *panel, int fps_mode)
{
	int rc = 0;
	int last_fps = 0;
	int last_fps_mode = 0;
	struct mi_dsi_panel_cfg *mi_cfg  = NULL;
	struct mi_mode_info *mi_timing  = NULL;

	if (!panel || !panel->cur_mode) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	DISP_DEBUG("[%s] DSI_CMD_SET_MI_TIMING_SWITCH BEFOR, fps=%d\n",
		       panel->name, last_fps);
	mi_cfg = &panel->mi_cfg;
	last_fps = mi_cfg->last_refresh_rate;
	last_fps_mode = mi_cfg->last_fps_mode;
	mi_timing = &panel->cur_mode->mi_timing;

	if ((last_fps == 1 && (fps_mode == DDIC_MODE_AUTO))
		|| ((fps_mode == DDIC_MODE_IDLE) && (mi_timing->ddic_min_refresh_rate == 1)
		&& (last_fps_mode == DDIC_MODE_AUTO))) {
		DSI_DEBUG("[%s] O3 switch idle 1hz to auto 1hz remove delay cmd, rc=%d\n",
		       panel->name, rc);
		return rc;
	}

	if (last_fps == 120 && ((last_fps_mode == DDIC_MODE_AUTO) || (last_fps_mode == DDIC_MODE_QSYNC))) {
		mi_dsi_update_switch_cmd_O3(panel, DSI_CMD_SET_TIMING_SWITCH_FROM_AUTO_UPDATE,
					DSI_CMD_SET_MI_TIMING_SWITCH_FROM_AUTO);
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_TIMING_SWITCH_FROM_AUTO, false);
	} else if (last_fps >= 90) {
		mi_dsi_update_switch_cmd_O3(panel, DSI_CMD_SET_TIMING_SWITCH_FROM_NORMAL_UPDATE,
					DSI_CMD_SET_MI_TIMING_SWITCH_FROM_NORMAL);
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_TIMING_SWITCH_FROM_NORMAL, false);
	} else if (last_fps >= 24) {
		mi_dsi_update_switch_cmd_O3(panel, DSI_CMD_SET_TIMING_SWITCH_FROM_SKIP_UPDATE,
					DSI_CMD_SET_MI_TIMING_SWITCH_FROM_SKIP);
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_TIMING_SWITCH_FROM_SKIP, false);
	} else if (last_fps >= 1) {
		mi_dsi_update_switch_cmd_O3(panel, DSI_CMD_SET_TIMING_SWITCH_FROM_AUTO_UPDATE,
					DSI_CMD_SET_MI_TIMING_SWITCH_FROM_AUTO);
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_TIMING_SWITCH_FROM_AUTO, false);
	}
	if (rc)
		DSI_INFO("[%s] O3 failed to send DSI_CMD_SET_MI_TIMING_SWITCH BEFOR cmds, rc=%d\n",
		       panel->name, rc);

	return rc;
}

int mi_dsi_panel_parse_dc_fps_config(struct dsi_panel *panel,
		struct dsi_display_mode *mode)
{
	int rc;
	struct dsi_display_mode_priv_info *priv_info;
	struct dsi_parser_utils *utils = &panel->utils;
	u32 dc_6c_cfg[2] = {0, 0};

	priv_info = mode->priv_info;

	rc = utils->read_u32_array(utils->data, "mi,mdss-dc-status-control-dc-6C-cfg",
			dc_6c_cfg, 2);
	if (rc) {
		DISP_DEBUG("mi,mdss-dc-status-control-dc-6C-cfg not defined rc=%d\n", rc);
	} else {
		DISP_INFO("FPS: %d, dc 6C cfg: 0x%02X, 0x%02X\n",
			mode->timing.refresh_rate, dc_6c_cfg[0], dc_6c_cfg[1]);
		priv_info->dc_on_6Creg = dc_6c_cfg[0];
		priv_info->dc_off_6Creg = dc_6c_cfg[1];
	}

	return 0;
}

int mi_dsi_panel_auto_limit_by_dbv(struct dsi_panel *panel, u32 bl_lvl)
{
	int rc = 0;

	struct dsi_display_mode_priv_info *priv_info = NULL;
	struct dsi_cmd_update_info *info = NULL;
	u32 cmd_update_count = 0;
	int j = 0;

	if (!panel || (8191 - bl_lvl < 0))
		return -EINVAL;

	if (mi_get_panel_id_by_dsi_panel(panel) != O3_PANEL_PA)
		return 0;

	if (bl_lvl == 0) {
		panel->mi_cfg.o3_dbv_param[0] = 0x03;
		panel->mi_cfg.o3_dbv_param[1] = 0x34;
		return 0;
	} else if (8191 - bl_lvl < (ARRAY_SIZE(O3_dbv_set))) {
		if ((panel->mi_cfg.o3_dbv_param[0] == ((O3_dbv_set[8191 - bl_lvl] >> 8) & 0xFF)) &&
			(panel->mi_cfg.o3_dbv_param[1] == (O3_dbv_set[8191 - bl_lvl] & 0xFF)))
			return 0;
		panel->mi_cfg.o3_dbv_param[0] = (O3_dbv_set[8191 - bl_lvl] >> 8) & 0xFF;
		panel->mi_cfg.o3_dbv_param[1] = O3_dbv_set[8191 - bl_lvl] & 0xFF;
		DISP_DEBUG("panel->mi_cfg.o3_dbv_param[0] = 0x%02X, panel->mi_cfg.o3_dbv_param[1] = 0x%02X",
			panel->mi_cfg.o3_dbv_param[0],panel->mi_cfg.o3_dbv_param[1]);
	} else {
		if (panel->mi_cfg.o3_dbv_param[0] == 0x03 && panel->mi_cfg.o3_dbv_param[1] == 0xFF)
			return 0;
		panel->mi_cfg.o3_dbv_param[0] = 0x03;
		panel->mi_cfg.o3_dbv_param[1] = 0xFF;
		DISP_DEBUG("panel->mi_cfg.o3_dbv_param[0] = 0x%02X, panel->mi_cfg.o3_dbv_param[1] = 0x%02X",
			panel->mi_cfg.o3_dbv_param[0],panel->mi_cfg.o3_dbv_param[1]);
	}

	priv_info = panel->cur_mode->priv_info;
	info = priv_info->cmd_update[DSI_CMD_SET_MI_AUTO_LIMIT_UPDATE];
	cmd_update_count = priv_info->cmd_update_count[DSI_CMD_SET_MI_AUTO_LIMIT_UPDATE];

	for (j = 0; j < cmd_update_count; j++) {
		if (info) {
			DISP_DEBUG("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
				panel->type, cmd_set_prop_map[info->type],
				info->mipi_address, info->index, info->length);
			if(info->mipi_address == 0xC1) {
				mi_dsi_panel_update_cmd_set(panel, panel->cur_mode,
						DSI_CMD_SET_MI_AUTO_LIMIT, info, panel->mi_cfg.o3_dbv_param, sizeof(panel->mi_cfg.o3_dbv_param));
			}
			info++;
		}
	}

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_AUTO_LIMIT, false);
	if (rc) {
		DISP_ERROR("(%s) send  cmds(%s) failed! bl_lvl(%d),rc(%d)\n",
			panel->name, cmd_set_prop_map[info->type], bl_lvl, rc);
		return rc;
	}
	return rc;
}

int mi_dsi_panel_dvb_demura_compensate(struct dsi_panel *panel, u32 bl_lvl)
{
	int rc = 0;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	mi_cfg = &panel->mi_cfg;
	if (bl_lvl <= 206 && !mi_cfg->demura_switch_enabled) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_PWM_DEMURA_SWITCH, false);
		if (rc == 0) {
			mi_cfg->demura_switch_enabled = true;
        }
    }
	else if (bl_lvl > 206 && mi_cfg->demura_switch_enabled) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DC_DEMURA_SWITCH, false);
		if (rc == 0) {
			mi_cfg->demura_switch_enabled = false;
        }
    }
	return rc;
}

void mi_dsi_panel_update_ic_dimming_by_bl(struct dsi_panel *panel, u32 bl_lvl) {
	if (panel->mi_cfg.disable_ic_dimming || panel->power_mode != SDE_MODE_DPMS_ON
	|| (mi_get_panel_id_by_dsi_panel(panel) != O2_PANEL_PA && mi_get_panel_id_by_dsi_panel(panel) != O1_PANEL_PA))
		return;
	if (panel->mi_cfg.ic_dimming_by_feature == FEATURE_OFF)
		return;
	if (bl_lvl >= 411 && panel->mi_cfg.dimming_state != STATE_DIM_BLOCK &&
		panel->mi_cfg.feature_val[DISP_FEATURE_DIMMING] == FEATURE_ON) {
		dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGOFF, false);
		panel->mi_cfg.feature_val[DISP_FEATURE_DIMMING] = FEATURE_OFF;
	} else if (bl_lvl < 411 && panel->mi_cfg.dimming_state != STATE_DIM_BLOCK &&
		panel->mi_cfg.feature_val[DISP_FEATURE_DIMMING] == FEATURE_OFF)  {
		dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_MI_DIMMINGON, false);
		panel->mi_cfg.feature_val[DISP_FEATURE_DIMMING] = FEATURE_ON;
	}
}

#ifdef CONFIG_DISP_DEBUG

#define MAX_DEBUG_TYPE_LEN 128
static bool g_modified_cmd_types[MAX_DEBUG_TYPE_LEN];
static bool g_modified_other_types[MAX_DEBUG_TYPE_LEN];

#define BUF_MAX 1024
static char g_debug_buf[BUF_MAX];

enum debug_param_type {
	H_FRONT_PORCH = 0,
	H_BACK_PORCH,
	H_PULSE_WIDTH,
	V_FRONT_PORCH,
	V_BACK_PORCH,
	V_PULSE_WIDTH,
	BL_MIN_LEVEL,
	BL_MAX_LEVEL,
	DEBUG_PARAM_MAX
};

static const char* g_debug_param_name[] = {
	"qcom,mdss-dsi-h-front-porch",
	"qcom,mdss-dsi-h-back-porch",
	"qcom,mdss-dsi-h-pulse-width",
	"qcom,mdss-dsi-v-front-porch",
	"qcom,mdss-dsi-v-back-porch",
	"qcom,mdss-dsi-v-pulse-width",
	"qcom,mdss-dsi-bl-min-level",
	"qcom,mdss-dsi-bl-max-level",
	"",
};

static int read_line_from_file(char *buf, size_t buf_len, struct file *file)
{
	ssize_t read_count;
	char c;
	int pos = 0;

	if (!buf || !file) {
		DISP_ERROR("%s, null pointer!\n", __func__);
		return -1;
	}

	while (pos < buf_len - 1) {
		read_count = kernel_read(file, &c, 1, &file->f_pos);
		if (read_count < 0) {
			DISP_ERROR("%s, read error!\n", __func__);
			return read_count;
		} else if (read_count == 0) {
			break;
		}

		if (c == '\n')
			break;

		buf[pos++] = c;
	}
	buf[pos] = '\0';
	return pos;
}

static int is_hex_char(char c) {
    return (isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

static int dsi_panel_parse_debug_cmd(const char *data, u32 length,
		struct dsi_panel_cmd_set *cmd)
{
	int rc = 0;
	u32 packet_count = 0;

	if (!data || !cmd) {
		DSI_ERR("%s, null pointer!\n", __func__);
		return -EINVAL;
	}

	rc = dsi_panel_get_cmd_pkt_count(data, length, &packet_count);
	if (rc) {
		DSI_ERR("commands failed, rc=%d\n", rc);
		goto error;
	}

	rc = dsi_panel_alloc_cmd_packets(cmd, packet_count);
	if (rc) {
		DSI_ERR("failed to allocate cmd packets, rc=%d\n", rc);
		goto error;
	}

	rc = dsi_panel_create_cmd_packets(data, length, packet_count,
					  cmd->cmds);
	if (rc) {
		DSI_ERR("failed to create cmd packets, rc=%d\n", rc);
		goto error_free_mem;
	}

	return rc;
error_free_mem:
	kfree(cmd->cmds);
	cmd->cmds = NULL;
error:
	return rc;
}

static void update_debug_cmd(struct file *file, struct dsi_panel_cmd_set *cmd)
{
	char line[256] = {0};
	bool cmd_end = false;
	int index = 0;
	u8 *buf = kzalloc(PAGE_SIZE, GFP_KERNEL);

	if (!buf) {
		DISP_ERROR("%s, Failed to allocate memory!\n", __func__);
		return;
	}
	while (read_line_from_file(line, 256, file) > 0) {
		if (strstr(line, "//") || strstr(line, "/*"))
			continue;
		if (strstr(line, "];"))
			cmd_end = true;

		// proccess data, easy to get data
		for (int i = 0; line[i] != '\0' && i < 256; i++) {
			if (!is_hex_char(line[i]) && line[i] != ' ')
				line[i] = ' ';
		}

		// parse data
		char *str = kstrdup(line, GFP_KERNEL);
		char *saveptr = str;
		char *token;
		char *delimiters = " \t\n";
		if (!str) {
			DISP_ERROR("%s, Failed to allocate memory!\n", __func__);
			goto free_buf;
		}
		token = strsep(&str, delimiters);
		while (token != NULL && index < PAGE_SIZE) {
			if (strlen(token)) {
				sscanf(token, "%hhx", &buf[index++]);
			}
			token = strsep(&str, delimiters);
		}
		kfree(saveptr);

		// replace with new command
		if (cmd_end) {
			if (cmd->cmds) {
				for (int i = 0; i < cmd->count; i++) {
					if (cmd->cmds[i].msg.tx_buf) {
						kfree(cmd->cmds[i].msg.tx_buf);
						cmd->cmds[i].msg.tx_buf = NULL;
					}
				}
				kfree(cmd->cmds);
				cmd->cmds = NULL;
			}
			dsi_panel_parse_debug_cmd(buf, index, cmd);
			goto free_buf;
		}
	}

free_buf:
	kfree(buf);
}

static void update_debug_param(struct dsi_panel *panel, int type)
{
	struct dsi_display_mode *mode = panel->cur_mode;
	int matches;

	switch (type) {
	case H_FRONT_PORCH:
		matches = sscanf(g_debug_buf, "qcom,mdss-dsi-h-front-porch = <%d>;",
				&mode->timing.h_front_porch);
		if (matches < 1)
			DISP_ERROR("parse %s failed!\n", g_debug_param_name[type]);
		break;
	case H_BACK_PORCH:
		matches = sscanf(g_debug_buf, "qcom,mdss-dsi-h-back-porch = <%d>;",
				&mode->timing.h_back_porch);
		if (matches < 1)
			DISP_ERROR("parse %s failed!\n", g_debug_param_name[type]);
		break;
	case H_PULSE_WIDTH:
		matches = sscanf(g_debug_buf, "qcom,mdss-dsi-h-pulse-width = <%d>;",
				&mode->timing.h_sync_width);
		if (matches < 1)
			DISP_ERROR("parse %s failed!\n", g_debug_param_name[type]);
		break;
	case V_FRONT_PORCH:
		matches = sscanf(g_debug_buf, "qcom,mdss-dsi-v-front-porch = <%d>;",
				&mode->timing.v_front_porch);
		if (matches < 1)
			DISP_ERROR("parse %s failed!\n", g_debug_param_name[type]);
		break;
	case V_BACK_PORCH:
		matches = sscanf(g_debug_buf, "qcom,mdss-dsi-v-back-porch = <%d>;",
				&mode->timing.v_back_porch);
		if (matches < 1)
			DISP_ERROR("parse %s failed!\n", g_debug_param_name[type]);
		break;
	case V_PULSE_WIDTH:
		matches = sscanf(g_debug_buf, "qcom,mdss-dsi-v-pulse-width = <%d>;",
				&mode->timing.v_sync_width);
		if (matches < 1)
			DISP_ERROR("parse %s failed!\n", g_debug_param_name[type]);
		break;
	case BL_MIN_LEVEL:
		matches = sscanf(g_debug_buf, "qcom,mdss-dsi-bl-min-level = <%d>;",
				&panel->bl_config.bl_min_level);
		if (matches < 1)
			DISP_ERROR("parse %s failed!\n", g_debug_param_name[type]);
		break;
	case BL_MAX_LEVEL:
		matches = sscanf(g_debug_buf, "qcom,mdss-dsi-bl-max-level = <%d>;",
				&panel->bl_config.bl_max_level);
		if (matches < 1)
			DISP_ERROR("parse %s failed!\n", g_debug_param_name[type]);
		break;
	default:
		DISP_ERROR("invalid type %d\n", type);
		break;
	}
}

ssize_t mi_dsi_panel_set_debug_param(struct dsi_panel *panel)
{
	int max_cmd_type_len = min(DSI_CMD_SET_MAX, MAX_DEBUG_TYPE_LEN);
	int max_other_type_len = min(DEBUG_PARAM_MAX, MAX_DEBUG_TYPE_LEN);
	struct dsi_display_mode *mode;
	struct file *file;
	int type;

	mode = panel->cur_mode;

	file = filp_open("/data/debug_param.dtsi", O_RDONLY, 0);
	if (IS_ERR(file)) {
		DISP_ERROR("Error opening file\n");
		return -1;
	}

	while (read_line_from_file(g_debug_buf, BUF_MAX, file) > 0) {
		for (type = DSI_CMD_SET_PRE_ON; type < max_cmd_type_len; type++) {
			if (strstr(g_debug_buf, cmd_set_prop_map[type])) {
				update_debug_cmd(file, &mode->priv_info->cmd_sets[type]);
				g_modified_cmd_types[type] = true;
				break;
			}
		}
		if (type < max_cmd_type_len)
			continue;
		for (type = 0; type < max_other_type_len; type++) {
			if (strstr(g_debug_buf, g_debug_param_name[type])) {
				update_debug_param(panel, type);
				g_modified_other_types[type] = true;
				break;
			}
		}
	}
	filp_close(file, NULL);
	return 0;
}

static const char* get_disp_param_str(struct dsi_panel *panel,
		enum dsi_cmd_set_type type)
{
	struct dsi_display_mode *mode = panel->cur_mode;

	switch (type) {
	case H_FRONT_PORCH:
		snprintf(g_debug_buf, BUF_MAX, "%d", mode->timing.h_front_porch);
		break;
	case H_BACK_PORCH:
		snprintf(g_debug_buf, BUF_MAX, "%d", mode->timing.h_back_porch);
		break;
	case H_PULSE_WIDTH:
		snprintf(g_debug_buf, BUF_MAX, "%d", mode->timing.h_sync_width);
		break;
	case V_FRONT_PORCH:
		snprintf(g_debug_buf, BUF_MAX, "%d", mode->timing.v_front_porch);
		break;
	case V_BACK_PORCH:
		snprintf(g_debug_buf, BUF_MAX, "%d", mode->timing.v_back_porch);
		break;
	case V_PULSE_WIDTH:
		snprintf(g_debug_buf, BUF_MAX, "%d", mode->timing.v_sync_width);
		break;
	case BL_MIN_LEVEL:
		snprintf(g_debug_buf, BUF_MAX, "%d", panel->bl_config.bl_min_level);
		break;
	case BL_MAX_LEVEL:
		snprintf(g_debug_buf, BUF_MAX, "%d", panel->bl_config.bl_max_level);
		break;
	default:
		snprintf(g_debug_buf, BUF_MAX, "get failed");
		break;
	}
	g_debug_buf[BUF_MAX - 1] = '\0';
	return g_debug_buf;
}

static char* get_mipi_cmd_str(struct dsi_panel *panel,
		enum dsi_cmd_set_type type)
{
	int i, j;
	int count = 0;
	struct dsi_display_mode *mode;
	struct dsi_cmd_desc *cmds;
	char *buf_full = "\nwarning: buffer is full!\n";
	u8 *buf = kzalloc(PAGE_SIZE, GFP_KERNEL);

	if (!buf) {
		DISP_ERROR("%s, Failed to allocate memory!\n", __func__);
		return NULL;
	}

	mode = panel->cur_mode;
	cmds = mode->priv_info->cmd_sets[type].cmds;
	for (i = 0; i < mode->priv_info->cmd_sets[type].count && count < PAGE_SIZE; i++) {
		count += snprintf(buf + count, PAGE_SIZE - count, "%02X 00 %02X %02X %02X %02zX %02zX",
				cmds[i].msg.type, cmds[i].msg.channel, cmds[i].msg.flags, cmds[i].post_wait_ms,
				cmds[i].msg.tx_len >> 8, cmds[i].msg.tx_len & 0xff);
		for (j = 0; j < cmds[i].msg.tx_len && count < PAGE_SIZE; j++) {
			count += snprintf(buf + count, PAGE_SIZE - count, " %02X", ((u8 *)cmds[i].msg.tx_buf)[j]);
		}
		if (count < PAGE_SIZE)
			count += snprintf(buf + count, PAGE_SIZE - count, "\n");
	}
	if (count >= PAGE_SIZE)
		snprintf(buf + PAGE_SIZE - 1 - strlen(buf_full), strlen(buf_full) + 1, buf_full);

	return buf;
}

ssize_t mi_dsi_panel_get_debug_param(struct dsi_panel *panel,
			char *buf, size_t size)
{
	int max_cmd_type_len = min(DSI_CMD_SET_MAX, MAX_DEBUG_TYPE_LEN);
	int max_other_type_len = min(DEBUG_PARAM_MAX, MAX_DEBUG_TYPE_LEN);
	char *buf_full = "\nwarning: buffer is full!\n";
	int count = 0;
	int type;

	if (!panel || !buf) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	count += snprintf(buf + count, size - count, "panel refresh_rate:%u\n\n",
			panel->cur_mode->timing.refresh_rate);
	for (type = DSI_CMD_SET_PRE_ON; type < max_cmd_type_len && count < size; type++) {
		if (g_modified_cmd_types[type]) {
			u8 *cmd_str = get_mipi_cmd_str(panel, type);
			count += snprintf(buf + count, size - count, "%s:\n%s\n",
					cmd_set_prop_map[type], cmd_str);
			if (cmd_str)
				kfree(cmd_str);
		}
	}

	for (type = 0; type < max_other_type_len && count < size; type++) {
		if (g_modified_other_types[type]) {
			count += snprintf(buf + count, size - count, "%s: %s\n",
					g_debug_param_name[type], get_disp_param_str(panel, type));
		}
	}
	if (count >= size)
		snprintf(buf + size - 1 - strlen(buf_full), strlen(buf_full) + 1, buf_full);

	return count;
}

#else

ssize_t mi_dsi_panel_set_debug_param(struct dsi_panel *panel)
{
	return -1;
}

ssize_t mi_dsi_panel_get_debug_param(struct dsi_panel *panel,
			char *buf, size_t size)
{
	if (!buf) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	return snprintf(buf, size, "perf build not support!\n");
}

#endif

int mi_dsi_panel_update_gamma_param(struct dsi_panel * panel, u32 cmd_update_index,
		enum dsi_cmd_set_type type)
{
	int rc=0;
	struct dsi_display_mode *cur_mode = NULL;
	struct dsi_cmd_update_info *info = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	u32 cmd_update_count = 0;
	u8 *update_rgb_reg_ptr = NULL;
	u8 update_rgb_reg[2] = {0};
	int i = 0;

	if (!panel->mi_cfg.read_gamma_success ||
		(mi_get_panel_id_by_dsi_panel(panel) != O2_PANEL_PA && mi_get_panel_id_by_dsi_panel(panel) != O1_PANEL_PA)) {
		DISP_DEBUG("don't need update gamma config\n");
		return 0;
	}

	if (!panel || !panel->cur_mode || !panel->cur_mode->priv_info) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	cur_mode = panel->cur_mode;
	mi_cfg = &panel->mi_cfg;
	info = cur_mode->priv_info->cmd_update[cmd_update_index];
	cmd_update_count = cur_mode->priv_info->cmd_update_count[cmd_update_index];

	for (i = 0; i < cmd_update_count; i++) {
		DISP_DEBUG("[%s] update [%s] mipi_address(0x%02X) index(%d) lenght(%d)\n",
		panel->type, cmd_set_prop_map[info->type], info->mipi_address, info->index, info->length);
		update_rgb_reg[0] = mi_cfg->gamma_rgb_param[2*(i/6)];
		update_rgb_reg[1] = mi_cfg->gamma_rgb_param[2*(i/6)+1];
		DISP_DEBUG("[%s] update_rgb_reg= [%d %d]\n", panel->type, update_rgb_reg[0], update_rgb_reg[1]);
		update_rgb_reg_ptr = update_rgb_reg;
		mi_dsi_panel_update_cmd_set(panel, cur_mode, type, info, update_rgb_reg, 2);
		info++;
	}

	return rc;
}

ssize_t mi_dsi_panel_lockdown_info_read( unsigned char *plockdowninfo)
{
	size_t item_size;
	void *lockdown_ptr = NULL;

	if (!plockdowninfo) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	lockdown_ptr = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_SW_DISPLAY_LOCKDOWN_TABLE, &item_size);
	if (!IS_ERR(lockdown_ptr) && item_size > 0) {
		memcpy(plockdowninfo, lockdown_ptr, item_size);
		DISP_INFO("lockdown data size= %lu\n",item_size);
		return item_size;
	} else {
		DISP_INFO("lockdown read failed! \n");
		return -EINVAL;
	}
}
EXPORT_SYMBOL(mi_dsi_panel_lockdown_info_read);

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
