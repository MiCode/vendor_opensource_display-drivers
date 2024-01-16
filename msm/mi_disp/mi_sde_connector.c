/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "mi_sde_connector:[%s:%d] " fmt, __func__, __LINE__

#include <drm/sde_drm.h>
#include "msm_drv.h"
#include "sde_connector.h"
#include "sde_encoder.h"
#include "sde_trace.h"
#include "dsi_display.h"
#include "dsi_panel.h"

#include "mi_disp_print.h"
#include "mi_disp_feature.h"
#include "mi_dsi_panel.h"
#include "mi_dsi_display.h"
#include "mi_sde_connector.h"
#include "mi_disp_lhbm.h"
#include "mi_panel_id.h"
#include "mi_disp_event.h"

static irqreturn_t mi_esd_err_irq_handle(int irq, void *data)
{
	struct sde_connector *c_conn = (struct sde_connector *)data;
	struct dsi_display *display = NULL;
	struct drm_event event;
	int power_mode;

	if (!c_conn || !c_conn->display) {
		DISP_ERROR("invalid c_conn/display\n");
		return IRQ_HANDLED;
	}


	if (c_conn->connector_type == DRM_MODE_CONNECTOR_DSI) {
		display = (struct dsi_display *)c_conn->display;
		if (!display || !display->panel) {
			DISP_ERROR("invalid display/panel\n");
			return IRQ_HANDLED;
		}

		DISP_INFO("%s display esd irq trigging \n", display->display_type);

		dsi_panel_acquire_panel_lock(display->panel);
		mi_dsi_panel_esd_irq_ctrl_locked(display->panel, false);

		if (!dsi_panel_initialized(display->panel)) {
			DISP_ERROR("%s display panel not initialized!\n",
					display->display_type);
			dsi_panel_release_panel_lock(display->panel);
			return IRQ_HANDLED;
		}

		if (atomic_read(&(display->panel->esd_recovery_pending))) {
			DISP_INFO("%s display ESD recovery already pending\n",
					display->display_type);
			dsi_panel_release_panel_lock(display->panel);
			return IRQ_HANDLED;
		}

		if (!c_conn->panel_dead) {
			atomic_set(&display->panel->esd_recovery_pending, 1);
		} else {
			DISP_INFO("%s display already notify PANEL_DEAD\n",
					display->display_type);
			dsi_panel_release_panel_lock(display->panel);
			return IRQ_HANDLED;
		}

		power_mode = display->panel->power_mode;
		DISP_INFO("%s display, power_mode (%s)\n", display->display_type,
			get_display_power_mode_name(power_mode));

		dsi_panel_release_panel_lock(display->panel);

		if (power_mode == SDE_MODE_DPMS_ON ||
			power_mode == SDE_MODE_DPMS_LP1) {
			if (!strcmp(display->display_type, "primary"))
				mi_disp_mievent_int(MI_DISP_PRIMARY,MI_EVENT_PRI_PANEL_IRQ_ESD);
			else
				mi_disp_mievent_int(MI_DISP_SECONDARY,MI_EVENT_SEC_PANEL_IRQ_ESD);
			_sde_connector_report_panel_dead(c_conn, false);
		} else {
			c_conn->panel_dead = true;
			event.type = DRM_EVENT_PANEL_DEAD;
			event.length = sizeof(bool);
			msm_mode_object_event_notify(&c_conn->base.base,
				c_conn->base.dev, &event, (u8 *)&c_conn->panel_dead);
			SDE_EVT32(SDE_EVTLOG_ERROR);
			DISP_ERROR("%s display esd irq check failed report"
				" PANEL_DEAD conn_id: %d enc_id: %d\n",
				display->display_type,
				c_conn->base.base.id, c_conn->encoder->base.id);
		}
	}

	return IRQ_HANDLED;

}


int mi_sde_connector_register_esd_irq(struct sde_connector *c_conn)
{
	struct dsi_display *display = NULL;
	int rc = 0;

	if (!c_conn || !c_conn->display) {
		DISP_ERROR("invalid c_conn/display\n");
		return 0;
	}

	/* register esd irq and enable it after panel enabled */
	if (c_conn->connector_type == DRM_MODE_CONNECTOR_DSI) {
		display = (struct dsi_display *)c_conn->display;
		if (!display || !display->panel) {
			DISP_ERROR("invalid display/panel\n");
			return -EINVAL;
		}
		if (display->panel->mi_cfg.esd_err_irq_gpio > 0) {
			rc = request_threaded_irq(display->panel->mi_cfg.esd_err_irq,
				NULL, mi_esd_err_irq_handle,
				display->panel->mi_cfg.esd_err_irq_flags,
				"esd_err_irq", c_conn);
			if (rc) {
				DISP_ERROR("%s display register esd irq failed\n",
					display->display_type);
			} else {
				DISP_INFO("%s display register esd irq success\n",
					display->display_type);
				disable_irq(display->panel->mi_cfg.esd_err_irq);
			}
		}
	}

	return rc;
}

int mi_sde_connector_debugfs_esd_sw_trigger(void *display)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	struct drm_connector *connector = NULL;
	struct sde_connector *c_conn = NULL;
	struct drm_event event;
	struct dsi_panel *panel;
	int power_mode;

	if (!dsi_display || !dsi_display->panel || !dsi_display->drm_conn) {
		DISP_ERROR("invalid display/panel/drm_conn ptr\n");
		return -EINVAL;
	}

	panel = dsi_display->panel;

	connector = dsi_display->drm_conn;
	c_conn = to_sde_connector(connector);
	if (!c_conn) {
		DISP_ERROR("invalid sde_connector ptr\n");
		return -EINVAL;
	}

	if (!dsi_panel_initialized(dsi_display->panel)) {
		DISP_ERROR("Panel not initialized\n");
		return -EINVAL;
	}

	if (atomic_read(&(dsi_display->panel->esd_recovery_pending))) {
		DISP_INFO("[esd-test]ESD recovery already pending\n");
		return 0;
	}

	if (c_conn->panel_dead) {
		DISP_INFO("panel_dead is true, return!\n");
		return 0;
	}

	dsi_panel_acquire_panel_lock(panel);
	atomic_set(&dsi_display->panel->esd_recovery_pending, 1);
	dsi_panel_release_panel_lock(panel);

	c_conn->panel_dead = true;
	DISP_ERROR("[esd-test]esd irq check failed report PANEL_DEAD conn_id: %d enc_id: %d\n",
			c_conn->base.base.id, c_conn->encoder->base.id);

	power_mode = dsi_display->panel->power_mode;
	DISP_INFO("[esd-test]%s display enabled (%d), power_mode (%s)\n", dsi_display->display_type,
		dsi_display->enabled, get_display_power_mode_name(power_mode));
	if (dsi_display->enabled) {
		if (power_mode == SDE_MODE_DPMS_ON || power_mode == SDE_MODE_DPMS_LP1) {
			sde_encoder_display_failure_notification(c_conn->encoder, false);
		}
	}

	if (!strcmp(dsi_display->display_type, "primary"))
		mi_disp_mievent_int(MI_DISP_PRIMARY,MI_EVENT_PRI_PANEL_IRQ_ESD);
	else
		mi_disp_mievent_int(MI_DISP_SECONDARY,MI_EVENT_SEC_PANEL_IRQ_ESD);

	event.type = DRM_EVENT_PANEL_DEAD;
	event.length = sizeof(bool);
	msm_mode_object_event_notify(&c_conn->base.base,
			c_conn->base.dev, &event, (u8 *)&c_conn->panel_dead);

	return 0;
}

int mi_sde_connector_check_layer_flags(struct drm_connector *connector)
{
	int ret = 0;
	struct sde_connector *c_conn;
	struct dsi_display *display = NULL;
	u32 value;
	struct mi_layer_flags flags;

	if (!connector) {
		DISP_ERROR("invalid connector ptr\n");
		return -EINVAL;
	}

	if (connector->connector_type == DRM_MODE_CONNECTOR_DSI) {
		c_conn = to_sde_connector(connector);
		value = sde_connector_get_property(connector->state, CONNECTOR_PROP_MI_LAYER_INFO);
		memcpy(&flags, &value, sizeof(u32));
		display = (struct dsi_display *)c_conn->display;

		if (flags.gxzw_anim_changed) {
			DISP_INFO("layer gxzw_anim = %d\n", flags.gxzw_anim_present);
			if (display && mi_disp_lhbm_fod_enabled(display->panel)) {
				ret = mi_disp_lhbm_aod_to_normal_optimize(display, flags.gxzw_anim_present);
				if (ret == -EAGAIN) {
					/*Doze brightness queue work schedule delay
					 * And trigger once in next frame*/
					display->panel->mi_cfg.aod_to_normal_pending = true;
				} else {
					display->panel->mi_cfg.aod_to_normal_pending = false;
				}
				display->panel->mi_cfg.lhbm_gxzw= flags.gxzw_anim_present;
				mi_disp_update_0size_lhbm_layer(display, flags.gxzw_anim_present);
			}
		}
		if (flags.aod_changed)
			DISP_INFO("layer aod = %d\n", flags.aod_present);
	}

	return 0;
}

int mi_sde_connector_flat_fence(struct drm_connector *connector)
{
	int rc = 0;
	struct sde_connector *c_conn;
	struct dsi_display *dsi_display;
	struct mi_dsi_panel_cfg *mi_cfg;
	int flat_mode_val = FEATURE_OFF;

	if (!connector) {
		DISP_ERROR("invalid connector ptr\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI)
		return 0;

	dsi_display = (struct dsi_display *) c_conn->display;
	if (!dsi_display || !dsi_display->panel) {
		DISP_ERROR("invalid display/panel ptr\n");
		return -EINVAL;
	}

	if (mi_get_disp_id(dsi_display->display_type) != MI_DISP_PRIMARY)
		return -EINVAL;

	mi_cfg = &dsi_display->panel->mi_cfg;

	if (mi_cfg->flat_sync_te) {
		dsi_panel_acquire_panel_lock(dsi_display->panel);
		flat_mode_val = mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE];
		if (flat_mode_val != mi_cfg->flat_cfg.cur_flat_state) {
			if (flat_mode_val == FEATURE_ON) {
				if (mi_get_panel_id_by_dsi_panel(dsi_display->panel) == N3_PANEL_PA &&
					dsi_display->panel->id_config.build_id >= N3_PANEL_PA_P11)
					mi_dsi_update_flat_mode_on_cmd(dsi_display->panel, DSI_CMD_SET_MI_FLAT_MODE_ON);
				sde_encoder_wait_for_event(c_conn->encoder,MSM_ENC_VBLANK);
				dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_SET_MI_FLAT_MODE_ON);
				DISP_INFO("DSI_CMD_SET_MI_FLAT_MODE_ON sync with te");
			} else {
				sde_encoder_wait_for_event(c_conn->encoder,MSM_ENC_VBLANK);
				dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_SET_MI_FLAT_MODE_OFF);
				DISP_INFO("DSI_CMD_SET_MI_FLAT_MODE_OFF sync with te");
			}
			mi_disp_feature_event_notify_by_type(mi_get_disp_id(dsi_display->display_type),
				MI_DISP_EVENT_FLAT_MODE, sizeof(flat_mode_val), flat_mode_val);
			mi_cfg->flat_cfg.cur_flat_state = flat_mode_val;
		}
		dsi_panel_release_panel_lock(dsi_display->panel);
	}

	return rc;
}

int mi_sde_connector_lhbm_on_fence(struct drm_connector *connector)
{
	int rc = 0;
	struct sde_connector *c_conn;
	struct dsi_display *dsi_display;
	struct mi_dsi_panel_cfg *mi_cfg;
	enum dsi_cmd_set_type lhbm_type = DSI_CMD_SET_MAX;
	int lhbm_on_val = FEATURE_ON;

	if (!connector) {
		DISP_ERROR("invalid connector ptr\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI)
		return 0;

	dsi_display = (struct dsi_display *) c_conn->display;
	if (!dsi_display || !dsi_display->panel) {
		DISP_ERROR("invalid display/panel ptr\n");
		return -EINVAL;
	}

	if (mi_get_disp_id(dsi_display->display_type) != MI_DISP_PRIMARY)
		return -EINVAL;

	mi_cfg = &dsi_display->panel->mi_cfg;

	if (mi_cfg->lhbm_off_sync_te) {
		dsi_panel_acquire_panel_lock(dsi_display->panel);
		lhbm_on_val = mi_cfg->cur_lhbm_on_state;
		if (lhbm_on_val == FEATURE_OFF) {
			switch (mi_cfg->feature_val[DISP_FEATURE_LOCAL_HBM]) {
			case LOCAL_HBM_OFF_TO_NORMAL:
				mi_dsi_update_backlight_in_aod(dsi_display->panel, true);
				if (mi_cfg->last_bl_level > mi_cfg->lhbm_hbm_mode_threshold) {
					DISP_INFO("LOCAL_HBM_OFF_TO_HBM sync with te\n");
					mi_cfg->dimming_state = STATE_DIM_BLOCK;
					lhbm_type = DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_HBM;
					mi_dsi_update_51_mipi_cmd(dsi_display->panel, lhbm_type,
							mi_cfg->last_bl_level);
					sde_encoder_wait_for_event(c_conn->encoder, MSM_ENC_VBLANK);
					rc = dsi_panel_tx_cmd_set(dsi_display->panel, lhbm_type);
					mi_cfg->cur_lhbm_on_state = FEATURE_ON;
				} else {
					DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL sync with te\n");
					lhbm_type = DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL;
					mi_dsi_update_51_mipi_cmd(dsi_display->panel, lhbm_type,
						mi_cfg->last_bl_level);
					sde_encoder_wait_for_event(c_conn->encoder, MSM_ENC_VBLANK);
					rc = dsi_panel_tx_cmd_set(dsi_display->panel, lhbm_type);
					mi_cfg->cur_lhbm_on_state = FEATURE_ON;

					mi_cfg->dimming_state = STATE_DIM_RESTORE;
					mi_dsi_panel_update_last_bl_level(dsi_display->panel, mi_cfg->last_bl_level);
				}
				/*normal fod, restore dbi by dbv*/
				if (mi_get_panel_id(mi_cfg->mi_panel_id) == N11U_PANEL_PA) {
  					mi_cfg->dbi_bwg_type = DSI_CMD_SET_MAX;
					dsi_panel_update_backlight(dsi_display->panel, mi_cfg->last_bl_level);
  				}
				break;
			case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT:
				DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT sync with te\n");
				mi_dsi_update_backlight_in_aod(dsi_display->panel, false);
				lhbm_type = DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL;
				/* display backlight value should equal AOD brightness */
				if (is_aod_and_panel_initialized(dsi_display->panel)) {
					switch (mi_cfg->doze_brightness) {
					case DOZE_BRIGHTNESS_HBM:
						if (mi_cfg->last_no_zero_bl_level < mi_cfg->doze_hbm_dbv_level
							&& mi_cfg->feature_val[DISP_FEATURE_FP_STATUS] == AUTH_STOP) {
							mi_dsi_update_51_mipi_cmd(dsi_display->panel, lhbm_type,
								mi_cfg->last_no_zero_bl_level);
							dsi_panel_send_em_cycle_setting(dsi_display->panel, mi_cfg->last_no_zero_bl_level, true);
						} else {
							mi_dsi_update_51_mipi_cmd(dsi_display->panel, lhbm_type,
								mi_cfg->doze_hbm_dbv_level);
							dsi_panel_send_em_cycle_setting(dsi_display->panel, mi_cfg->doze_hbm_dbv_level, true);
						}
						DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT sync with tein doze_hbm_dbv_level\n");
						break;
					case DOZE_BRIGHTNESS_LBM:
						if (mi_cfg->last_no_zero_bl_level < mi_cfg->doze_lbm_dbv_level) {
							mi_dsi_update_51_mipi_cmd(dsi_display->panel, lhbm_type,
								mi_cfg->last_no_zero_bl_level);
							dsi_panel_send_em_cycle_setting(dsi_display->panel, mi_cfg->last_no_zero_bl_level, true);
						} else {
							mi_dsi_update_51_mipi_cmd(dsi_display->panel, lhbm_type,
								mi_cfg->doze_lbm_dbv_level);
							dsi_panel_send_em_cycle_setting(dsi_display->panel, mi_cfg->doze_lbm_dbv_level, true);
						}
						DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT sync with te in doze_lbm_dbv_level\n");
						break;
					default:
						DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT sync with te defaults\n");
						break;
					}
				} else {
					mi_dsi_update_51_mipi_cmd(dsi_display->panel, lhbm_type, mi_cfg->last_bl_level);
					dsi_panel_send_em_cycle_setting(dsi_display->panel, mi_cfg->last_bl_level, true);
				}

				DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL sync with te\n");
				sde_encoder_wait_for_event(c_conn->encoder, MSM_ENC_VBLANK);
				rc = dsi_panel_tx_cmd_set(dsi_display->panel, lhbm_type);
				mi_cfg->cur_lhbm_on_state = FEATURE_ON;

				if (mi_cfg->need_fod_animal_in_normal)
					dsi_display->panel->mi_cfg.aod_to_normal_statue = true;
				mi_cfg->dimming_state = STATE_DIM_RESTORE;
				break;
			case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE:
				/* display backlight value should equal unlock brightness */
				DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE sync with te\n");
				mi_dsi_update_backlight_in_aod(dsi_display->panel, true);
				mi_dsi_update_51_mipi_cmd(dsi_display->panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL,
							mi_cfg->last_bl_level);

				DISP_INFO("LOCAL_HBM_OFF_TO_NORMAL sync with te\n");
				sde_encoder_wait_for_event(c_conn->encoder, MSM_ENC_VBLANK);
				rc = dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_SET_MI_LOCAL_HBM_OFF_TO_NORMAL);
				mi_cfg->cur_lhbm_on_state = FEATURE_ON;

				/*mi_dsi_update_backlight_in_aod will change 51's value */
				if (mi_get_panel_id(dsi_display->panel->mi_cfg.mi_panel_id) == N11U_PANEL_PA) {
					dsi_panel_send_em_cycle_setting(dsi_display->panel, mi_cfg->last_bl_level, true);
				}

				mi_cfg->dimming_state = STATE_DIM_RESTORE;
				break;
			default:
				DISP_ERROR("invalid or don't need sync te local hbm value");
				mi_cfg->cur_lhbm_on_state = FEATURE_ON;
				break;
			}
		}
		dsi_panel_release_panel_lock(dsi_display->panel);
	}

	return rc;
}