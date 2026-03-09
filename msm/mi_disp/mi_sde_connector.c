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
#include "mi_panel_id.h"
#ifdef CONFIG_VIS_DISPLAY
#include "vis_display.h"
#endif

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

		DISP_INFO("%s-%s display esd irq trigging \n", display->display_type, display->name);

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
#ifdef CONFIG_VIS_DISPLAY
			if (is_mi_dev_support_nova()) {
				DISP_INFO("[ESD]call vis_mi_esd_resetAB\n");
				vis_mi_esd_resetAB();
			}
#endif
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
#ifdef CONFIG_VIS_DISPLAY
			if (is_mi_dev_support_nova()) {
				vis_mi_esd_recover_handle_notify(mi_esd_err_irq_handle);
			}
#endif
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


	event.type = DRM_EVENT_PANEL_DEAD;
	event.length = sizeof(bool);
	msm_mode_object_event_notify(&c_conn->base.base,
			c_conn->base.dev, &event, (u8 *)&c_conn->panel_dead);

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

	if (mi_get_disp_id(dsi_display->display_type) != MI_DISP_PRIMARY && mi_get_disp_id(dsi_display->display_type) != MI_DISP_SECONDARY)
		return -EINVAL;

	mi_cfg = &dsi_display->panel->mi_cfg;

	if (mi_cfg->flat_sync_te) {
		dsi_panel_acquire_panel_lock(dsi_display->panel);
		flat_mode_val = mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE];
		if (flat_mode_val != mi_cfg->flat_cfg.cur_flat_state) {
			if (flat_mode_val == FEATURE_ON) {
				sde_encoder_wait_for_event(c_conn->encoder,MSM_ENC_VBLANK);
				dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_SET_MI_FLAT_MODE_ON, false);
				DISP_INFO("DSI_CMD_SET_MI_FLAT_MODE_ON sync with te");
			} else {
				sde_encoder_wait_for_event(c_conn->encoder,MSM_ENC_VBLANK);
				dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_SET_MI_FLAT_MODE_OFF, false);
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

int mi_sde_connector_send_emv_queue_work(struct drm_connector *connector)
{

	struct dsi_display *display;
	struct dsi_panel *panel;
	struct mi_dsi_panel_cfg *mi_cfg;
	int disp_id = 0;
	struct disp_feature *df = mi_get_disp_feature();
	struct disp_display *dd_ptr;
	struct disp_work *send_emv_work;
	struct sde_connector *c_conn;
	if (!connector) {
		DISP_ERROR("invalid connector ptr\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI)
		return 0;

	display = (struct dsi_display *)c_conn->display;
	if (!display || !display->panel) {
		DISP_ERROR("invalid display/panel ptr\n");
		return -EINVAL;
	}
	panel = display->panel;
	mi_cfg = &panel->mi_cfg;

	mutex_lock(&mi_cfg->emv_info_lock);
	if (mi_cfg->emv_cfg.emv_enable_state == 0) {
		mutex_unlock(&mi_cfg->emv_info_lock);
		return -EINVAL;
	}
	mutex_unlock(&mi_cfg->emv_info_lock);

	send_emv_work = kzalloc(sizeof(*send_emv_work), GFP_KERNEL);
	if (!send_emv_work) {
		DISP_ERROR("failed to allocate delayed_work buffer\n");
		return -ENOMEM;
	}

	disp_id = mi_get_disp_id(panel->type);
	dd_ptr = &df->d_display[disp_id];

	kthread_init_work(&send_emv_work->work, mi_sde_connector_send_emv_work_handler);
	send_emv_work->dd_ptr = dd_ptr;
	send_emv_work->wq = &dd_ptr->pending_wq;
	send_emv_work->data = connector;
	return kthread_queue_work(dd_ptr->worker, &send_emv_work->work);
}

void mi_sde_connector_send_emv_work_handler(struct kthread_work *work) {
	struct disp_work *emv_work = container_of(work, struct disp_work, work);
	struct drm_connector *connector = emv_work->data;
	struct sde_connector *c_conn;
	struct dsi_display *dsi_display;
	struct mi_dsi_panel_cfg *mi_cfg;
	struct dsi_panel_cmd_set tmp_cmd_sets;
	struct mipi_dsi_msg mipi_cmd;
	struct dsi_cmd_desc tmp_cmds;
	int rc = 0;
	bool test_pattern;
	// Max buffer size must less than (4096-4)byte for single transfer
	u32 mv_cmd_buf_size = 3072;
	u32 mv_package_size = 1024;
	u32 mv_payload_size = mv_package_size - 4;
	u32 meta_size = 0;
	u32 mv_size = 0;
	u32 tx_size = 0;
	u32 buf_pos = 0;
	u32 mv_pos = 0;
	u32 copy_size  = 0;
	u32 remain_size = 0;
	u32 tx_total_bytes = 0;
	u32 sent_bytes = 0;
	u32 current_chunk = 0;
	u32 *mv_src = NULL;
	u32 *tx_buffer = NULL;

	if (!connector) {
		DISP_ERROR("invalid connector ptr\n");
		goto end;
	}

	c_conn = to_sde_connector(connector);
	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI)
		goto end;

	dsi_display = (struct dsi_display *)c_conn->display;
	if (!dsi_display || !dsi_display->panel) {
		DISP_ERROR("invalid display/panel ptr\n");
		goto end;
	}

	mi_cfg = &dsi_display->panel->mi_cfg;

	mutex_lock(&mi_cfg->emv_info_lock);

	if (mi_cfg->emv_cfg.emv_enable_state == 0) {
		DISP_DEBUG("[EXTMV] EMV disabled, skip sending\n");
		mutex_unlock(&mi_cfg->emv_info_lock);
		goto end;
	}
	mi_cfg->emv_cfg.emv_enable_state = 0;
	SDE_ATRACE_BEGIN("mi_sde_connector_send_emv_work_handler");

	// Check EMV data is valid
	meta_size = mi_cfg->emv_cfg.mvInfo[0] & 0xFFFF;
	test_pattern = !!(mi_cfg->emv_cfg.mvInfo[0] & 0xF0000);
	if (meta_size >= 40000 && test_pattern == false) {
		DISP_DEBUG("[EXTMV] Skip ext mv transfer meta_size(%u) test_pattern(%d)", meta_size, test_pattern);
		mutex_unlock(&mi_cfg->emv_info_lock);
		goto error_data_invalid;
	}

	mv_size = mi_cfg->emv_cfg.mvInfo[13] * 4;
	tx_size = (((24 + mv_size + 8) / mv_payload_size) * mv_package_size) + (((24 + mv_size + 8) % mv_payload_size) + 4);

	tx_buffer = kmalloc(tx_size, GFP_KERNEL);
	if (!tx_buffer){
		DISP_ERROR("failed to allocate tx_buffer\n");
		mutex_unlock(&mi_cfg->emv_info_lock);
		goto error_data_invalid;
	}

	// header data
	tx_buffer[0] = 0x0854564E;
	tx_buffer[1] = 0x4154454D;
	tx_buffer[2] = (mi_cfg->emv_cfg.mvInfo[4] & 0xFFFF) | (mi_cfg->emv_cfg.mvInfo[3] << 16);
	tx_buffer[3] = 0x10000801;
	tx_buffer[4] = (mi_cfg->emv_cfg.mvInfo[8] & 0xFFFF) | (mi_cfg->emv_cfg.mvInfo[7] << 16);
	tx_buffer[5] = (mi_cfg->emv_cfg.mvInfo[10] & 0xFFFF) | (mi_cfg->emv_cfg.mvInfo[9] << 16);
	tx_buffer[6] = (mi_cfg->emv_cfg.mvInfo[12] & 0xFFFF) | (mi_cfg->emv_cfg.mvInfo[11] << 16);

	buf_pos = 7;
	mv_pos = 14;
	mv_src = &(mi_cfg->emv_cfg.mvInfo[0]);
	remain_size = mv_size;

	copy_size = min(mv_size, (mv_package_size - 28));
	while (remain_size > 0) {
		DISP_DEBUG("[EXTMV] buf_pos: %u mv_pos: %u remain_size: %u copy_size: %u tx_size: %u\n", buf_pos, mv_pos, remain_size, copy_size, tx_size);
		if ((buf_pos * 4 + copy_size) > tx_size) {
			DISP_ERROR("[EXTMV] buffer overflow detected\n");
			mutex_unlock(&mi_cfg->emv_info_lock);
			goto error_send_fail;
		}
		memcpy(tx_buffer + buf_pos, mv_src + mv_pos, copy_size);
		remain_size -= copy_size;
		mv_pos += (copy_size / 4);
		buf_pos += (copy_size / 4);

		if (remain_size <= 0) {
			break;
		}

		tx_buffer[buf_pos++] = 0x0954564E;
		copy_size = min(remain_size, mv_payload_size);
	}

	if (buf_pos % (mv_package_size / 4) == 0) {
		tx_buffer[buf_pos++] = 0x0954564E;
	}
	tx_buffer[buf_pos++] = (mi_cfg->emv_cfg.mvInfo[(mv_size / 4) + 15] & 0x0000FFFF) | (mi_cfg->emv_cfg.mvInfo[(mv_size / 4) + 14] << 16);
	tx_buffer[buf_pos++] = (mi_cfg->emv_cfg.mvInfo[(mv_size / 4) + 17] & 0x0000FFFF) | (mi_cfg->emv_cfg.mvInfo[(mv_size / 4) + 16] << 16);

	tx_total_bytes = buf_pos * sizeof(u32);
	mutex_unlock(&mi_cfg->emv_info_lock);
	while (sent_bytes < tx_total_bytes) {
		current_chunk = min(mv_cmd_buf_size, tx_total_bytes - sent_bytes);
		DISP_DEBUG("[EXTMV] current_chunk: %u sent_bytes: %u tx_total_bytes: %u\n", current_chunk, sent_bytes, tx_total_bytes);
		mipi_cmd.tx_len = current_chunk;
		mipi_cmd.tx_buf = (u8 *)tx_buffer + sent_bytes;
		mipi_cmd.type = 0x39;
		mipi_cmd.channel = 0x00;
		memcpy(&(tmp_cmds.msg), &mipi_cmd, sizeof(struct mipi_dsi_msg));
		tmp_cmd_sets.cmds = &tmp_cmds;
		tmp_cmd_sets.count = 1;
		tmp_cmd_sets.state = DSI_CMD_SET_STATE_HS;
		rc = mi_dsi_display_cmd_write(dsi_display, &tmp_cmd_sets);
		if (rc) {
			DISP_ERROR("[EXTMV] It is failed to send mv[%d],rc=%d\n", mv_pos, rc);
			goto error_send_fail;
		}
		sent_bytes += current_chunk;
	}

error_send_fail:
	kfree(tx_buffer);
error_data_invalid:
	SDE_ATRACE_END("mi_sde_connector_send_emv_work_handler");
end:
	return;
}


int mi_sde_connector_get_dc_status(struct drm_connector *connector)
{
	struct sde_connector *c_conn;
	struct dsi_display *dsi_display;
	struct mi_dsi_panel_cfg *mi_cfg;

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

	mi_cfg = &dsi_display->panel->mi_cfg;

	return mi_cfg->feature_val[DISP_FEATURE_DC];
}


int mi_sde_connector_state_get_mi_mode_info(struct drm_connector_state *conn_state,
	struct mi_mode_info *mode_info)
{
	struct sde_connector_state *sde_conn_state = NULL;

	if (!conn_state || !mode_info) {
		SDE_ERROR("Invalid arguments\n");
		return -EINVAL;
	}

	sde_conn_state = to_sde_connector_state(conn_state);
	memcpy(mode_info, &sde_conn_state->mode_info.mi_mode_info,
		sizeof(struct mi_mode_info));

	return 0;
}

static int mi_sde_connector_populate_mi_mode_info(struct drm_connector *conn,
	struct sde_kms_info *info)
{
	struct sde_connector *c_conn = NULL;
	struct drm_display_mode *drm_mode;
	struct msm_mode_info mode_info;
	int rc = 0;

	c_conn = to_sde_connector(conn);
	if (!c_conn->ops.get_mode_info) {
		DISP_ERROR("conn%d get_mode_info not defined\n", c_conn->base.base.id);
		return -EINVAL;
	}

	list_for_each_entry(drm_mode, &conn->modes, head) {

		memset(&mode_info, 0, sizeof(mode_info));

		rc = sde_connector_get_mode_info(&c_conn->base, drm_mode, NULL,
				&mode_info);
		if (rc) {
			DISP_ERROR("conn%d failed to get mode info for mode %s\n",
				c_conn->base.base.id, drm_mode->name);
			continue;
		}

		sde_kms_info_add_keystr(info, "mode_name", drm_mode->name);

		sde_kms_info_add_keyint(info, "timing_refresh_rate",
			mode_info.mi_mode_info.timing_refresh_rate);
		sde_kms_info_add_keyint(info, "ddic_mode",
			mode_info.mi_mode_info.ddic_mode);
		sde_kms_info_add_keyint(info, "sf_refresh_rate",
			mode_info.mi_mode_info.sf_refresh_rate);
		sde_kms_info_add_keyint(info, "ddic_min_refresh_rate",
			mode_info.mi_mode_info.ddic_min_refresh_rate);
	}

	return rc;
}

static int mi_sde_connector_set_blob_data(struct drm_connector *conn,
		enum msm_mdp_conn_property prop_id)
{
	struct sde_kms_info *info;
	struct sde_connector *c_conn = NULL;
	struct drm_property_blob **blob = NULL;
	int rc = 0;

	c_conn = to_sde_connector(conn);
	if (!c_conn) {
		DISP_ERROR("invalid argument\n");
		return -EINVAL;
	}

	info = vzalloc(sizeof(*info));
	if (!info)
		return -ENOMEM;

	sde_kms_info_reset(info);

	switch (prop_id) {
	case CONNECTOR_PROP_MI_MODE_INFO:
		rc = mi_sde_connector_populate_mi_mode_info(conn, info);
		if (rc) {
			DISP_ERROR("conn%d mode info population failed, %d\n",
					c_conn->base.base.id, rc);
			goto exit;
		}
		blob = &c_conn->blob_mi_mode_info;
	break;
	default:
		DISP_ERROR("conn%d invalid prop_id: %d\n",
				c_conn->base.base.id, prop_id);
		goto exit;
	}

	msm_property_set_blob(&c_conn->property_info,
			blob,
			SDE_KMS_INFO_DATA(info),
			SDE_KMS_INFO_DATALEN(info),
			prop_id);
exit:
	vfree(info);

	return rc;
}
int mi_sde_connector_install_properties(struct sde_connector *c_conn)
{
	int rc = 0;

	if (c_conn->connector_type == DRM_MODE_CONNECTOR_DSI) {
		msm_property_install_blob(&c_conn->property_info, "mi_mode_info",
			DRM_MODE_PROP_IMMUTABLE, CONNECTOR_PROP_MI_MODE_INFO);

		msm_property_install_volatile_range(&c_conn->property_info,
			"mi_emv_info", 0x0, 0, ~0, 0, CONNECTOR_PROP_MI_EMV_INFO);

		msm_property_install_range(&c_conn->property_info, "mi_avr_step_fps",
			0x0, 0, U32_MAX, 0, CONNECTOR_PROP_MI_AVR_SETP_FPS);

		rc = mi_sde_connector_set_blob_data(&c_conn->base, CONNECTOR_PROP_MI_MODE_INFO);
		if (rc) {
			DISP_ERROR("conn%d failed to setup connector info, rc = %d\n",
					c_conn->base.base.id, rc);
			return rc;
		}
	}

	return rc;
}

int mi_sde_connector_set_emv_buffer(struct sde_connector *c_conn, void __user *usr_ptr)
{
	struct dsi_display *display;
	struct mi_dsi_panel_cfg *mi_cfg;
	int rc = 0;

	if (!c_conn || !c_conn->display) {
		DISP_ERROR("invalid c_conn/display\n");
		return -EINVAL;
	}

	if (!usr_ptr) {
		DISP_ERROR("conn%d emv config invalid\n", c_conn->base.base.id);
		return -EINVAL;
	}

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI)
		return -EINVAL;

	display = (struct dsi_display *)c_conn->display;
	if (!display || !display->panel) {
		DISP_ERROR("invalid display/panel\n");
		return -EINVAL;
	}

	mi_cfg = &display->panel->mi_cfg;
	mutex_lock(&mi_cfg->emv_info_lock);

	if (copy_from_user(&mi_cfg->emv_cfg, usr_ptr, sizeof(mi_cfg->emv_cfg))) {
		DISP_ERROR("conn%d failed to copy emv config\n", c_conn->base.base.id);
		mi_cfg->emv_cfg.emv_enable_state = 0;
		rc = -EFAULT;
		goto unlock_out;
	}

	if (mi_cfg->emv_cfg.mv_info_size <= 0 ||
		mi_cfg->emv_cfg.mv_info_size > MI_EMV_MAX_SIZE) {
		DISP_ERROR("conn%d invalid mv_info_size: %d\n", c_conn->base.base.id, mi_cfg->emv_cfg.mv_info_size);
		mi_cfg->emv_cfg.emv_enable_state = 0;
		rc = -EINVAL;
		goto unlock_out;
	}

unlock_out:
	mutex_unlock(&mi_cfg->emv_info_lock);
	return rc;
}
