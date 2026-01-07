/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"mi-dsi-display:[%s] " fmt, __func__

#include "msm_kms.h"
#include "sde_trace.h"
#include "sde_connector.h"
#include "dsi_display.h"
#include "dsi_panel.h"
#include "mi_disp_print.h"
#include "mi_sde_encoder.h"
#include "mi_dsi_display.h"
#include "mi_dsi_panel.h"
#include "mi_disp_feature.h"
#include "mi_panel_id.h"
#include "mi_dsi_panel_count.h"
#include "mi_disp_flatmode.h"
#include "drm/drm_mipi_dsi.h"
#include "sde_vm.h"
#include "dsi_phy_hw.h"

static char oled_wp_info_str[32] = {0};
static char sec_oled_wp_info_str[32] = {0};
static char cell_id_info_str[32] = {0};

#define MAX_DEBUG_POLICY_CMDLINE_LEN 64
static char display_debug_policy[MAX_DEBUG_POLICY_CMDLINE_LEN] = {0};

static struct dsi_read_info g_dsi_read_info;

char *get_display_power_mode_name(int power_mode)
{
	switch (power_mode) {
	case SDE_MODE_DPMS_ON:
		return "On";
	case SDE_MODE_DPMS_LP1:
		return "Doze";
	case SDE_MODE_DPMS_LP2:
		return "Doze_Suspend";
	case SDE_MODE_DPMS_STANDBY:
		return "Standby";
	case SDE_MODE_DPMS_SUSPEND:
		return "Suspend";
	case SDE_MODE_DPMS_OFF:
		return "Off";
	default:
		return "Unknown";
	}
}

int mi_get_disp_id(const char *display_type)
{
	if (!strncmp(display_type, "primary", 7))
		return MI_DISP_PRIMARY;
	else
		return MI_DISP_SECONDARY;
}

struct dsi_display * mi_get_primary_dsi_display(void)
{
	struct disp_feature *df = mi_get_disp_feature();
	struct disp_display *dd_ptr = NULL;
	struct dsi_display *dsi_display = NULL;

	if (df) {
		dd_ptr = &df->d_display[MI_DISP_PRIMARY];
		if (dd_ptr->display && dd_ptr->intf_type == MI_INTF_DSI) {
			dsi_display = (struct dsi_display *)dd_ptr->display;
			return dsi_display;
		} else {
			return NULL;
		}
	} else {
		return NULL;
	}
}

struct dsi_display * mi_get_secondary_dsi_display(void)
{
	struct disp_feature *df = mi_get_disp_feature();
	struct disp_display *dd_ptr = NULL;
	struct dsi_display *dsi_display = NULL;

	if (df) {
		dd_ptr = &df->d_display[MI_DISP_SECONDARY];
		if (dd_ptr->display && dd_ptr->intf_type == MI_INTF_DSI) {
			dsi_display = (struct dsi_display *)dd_ptr->display;
			return dsi_display;
		} else {
			return NULL;
		}
	} else {
		return NULL;
	}
}

int mi_dsi_display_set_disp_param(void *display,
			struct disp_feature_ctl *ctl)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	char trace_buf[64];
	int ret = 0;
	struct sde_kms *sde_kms = NULL;

	if (!dsi_display || !ctl) {
		DISP_ERROR("Invalid display or ctl ptr\n");
		return -EINVAL;
	}

	if (sde_kms_is_suspend_blocked(dsi_display->drm_dev) &&
		mi_dsi_panel_is_need_tx_cmd(ctl->feature_id)) {
		DISP_ERROR("sde_kms is suspended, skip to set disp_param\n");
		return -EBUSY;
	}

	sde_kms = dsi_display_get_kms(dsi_display);
	if (sde_kms && mi_dsi_panel_is_need_tx_cmd(ctl->feature_id)) {
		sde_vm_lock(sde_kms);
		if (!sde_vm_owns_hw(sde_kms)) {
			DISP_ERROR("op not supported due to HW unavailablity\n");
			ret = -EOPNOTSUPP;
			goto end;
		}
	}

	mi_dsi_acquire_wakelock(dsi_display->panel);
	snprintf(trace_buf, sizeof(trace_buf), "set_disp_param:%s",
			get_disp_feature_id_name(ctl->feature_id));
	SDE_ATRACE_BEGIN(trace_buf);
	ret = mi_dsi_panel_set_disp_param(dsi_display->panel, ctl);
	SDE_ATRACE_END(trace_buf);
	mi_dsi_release_wakelock(dsi_display->panel);

end:
	if (sde_kms && mi_dsi_panel_is_need_tx_cmd(ctl->feature_id))
		sde_vm_unlock(sde_kms);
	return ret;
}

int mi_dsi_display_get_disp_param(void *display,
			struct disp_feature_ctl *ctl)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_disp_param(dsi_display->panel, ctl);
}

ssize_t mi_dsi_display_show_disp_param(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_show_disp_param(dsi_display->panel, buf, size);
}

int mi_dsi_display_write_dsi_cmd(void *display,
			struct dsi_cmd_rw_ctl *ctl)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;
	struct sde_kms *sde_kms = NULL;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (sde_kms_is_suspend_blocked(dsi_display->drm_dev)) {
		DISP_ERROR("sde_kms is suspended, skip to write dsi cmd\n");
		return -EBUSY;
	}

	sde_kms = dsi_display_get_kms(dsi_display);
	if (!sde_kms) {
		DISP_ERROR("invalid kms\n");
		return -EINVAL;
	}

	sde_vm_lock(sde_kms);
	if (!sde_vm_owns_hw(sde_kms)) {
		DISP_ERROR("op not supported due to HW unavailablity\n");
		ret = -EOPNOTSUPP;
		goto end;
	}

	mi_dsi_acquire_wakelock(dsi_display->panel);
	ret = mi_dsi_panel_write_dsi_cmd(dsi_display->panel, ctl);
	mi_dsi_release_wakelock(dsi_display->panel);

end:
	sde_vm_unlock(sde_kms);
	return ret;
}

int mi_dsi_display_read_dsi_cmd(void *display,
			struct dsi_cmd_rw_ctl *ctl)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 16, 0))
	ktime_t ts = 0;
#endif
	struct sde_kms *sde_kms = NULL;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (sde_kms_is_suspend_blocked(dsi_display->drm_dev)) {
		DISP_ERROR("sde_kms is suspended, skip to read dsi cmd\n");
		return -EBUSY;
	}

	sde_kms = dsi_display_get_kms(dsi_display);
	if (!sde_kms) {
		DISP_ERROR("invalid kms\n");
		return -EINVAL;
	}

	sde_vm_lock(sde_kms);
	if (!sde_vm_owns_hw(sde_kms)) {
		DISP_ERROR("op not supported due to HW unavailablity\n");
		ret = -EOPNOTSUPP;
		goto end;
	}

	mi_dsi_acquire_wakelock(dsi_display->panel);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 16, 0))
	ret = dsi_display_cmd_receive(dsi_display,
			ctl->tx_ptr, ctl->tx_len, ctl->rx_ptr, ctl->rx_len, &ts);
#else
	ret = dsi_display_cmd_receive(dsi_display,
			ctl->tx_ptr, ctl->tx_len, ctl->rx_ptr, ctl->rx_len);
#endif
	mi_dsi_release_wakelock(dsi_display->panel);

end:
	sde_vm_unlock(sde_kms);
	return ret;
}

int mi_dsi_display_set_mipi_rw(void *display, char *buf)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	struct dsi_cmd_rw_ctl ctl;
	int ret = 0;
	char *token, *input_copy, *input_dup = NULL;
	const char *delim = " ";
	bool is_read = false;
	char *buffer = NULL;
	u32 buf_size = 0;
	u32 tmp_data = 0;
	u32 recv_len = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	memset(&ctl, 0, sizeof(struct dsi_cmd_rw_ctl));
	memset(&g_dsi_read_info, 0, sizeof(struct dsi_read_info));

	DISP_TIME_INFO("input buffer:{%s}\n", buf);

	input_copy = kstrdup(buf, GFP_KERNEL);
	if (!input_copy) {
		ret = -ENOMEM;
		goto exit;
	}

	input_dup = input_copy;
	/* removes leading and trailing whitespace from input_copy */
	input_copy = strim(input_copy);

	/* Split a string into token */
	token = strsep(&input_copy, delim);
	if (token) {
		ret = kstrtoint(token, 10, &tmp_data);
		if (ret) {
			DISP_ERROR("input buffer conversion failed\n");
			goto exit_free0;
		}
		is_read = !!tmp_data;
	}

	/* Removes leading whitespace from input_copy */
	if (input_copy)
		input_copy = skip_spaces(input_copy);
	else
		goto exit_free0;

	token = strsep(&input_copy, delim);
	if (token) {
		ret = kstrtoint(token, 10, &tmp_data);
		if (ret) {
			DISP_ERROR("input buffer conversion failed\n");
			goto exit_free0;
		}
		if (tmp_data > sizeof(g_dsi_read_info.rx_buf)) {
			DISP_ERROR("read size exceeding the limit %lu\n",
					sizeof(g_dsi_read_info.rx_buf));
			goto exit_free0;
		}
		ctl.rx_len = tmp_data;
		ctl.rx_ptr = g_dsi_read_info.rx_buf;
	}

	/* Removes leading whitespace from input_copy */
	if (input_copy)
		input_copy = skip_spaces(input_copy);
	else
		goto exit_free0;

	buffer = kzalloc(strlen(input_copy), GFP_KERNEL);
	if (!buffer) {
		ret = -ENOMEM;
		goto exit_free0;
	}

	token = strsep(&input_copy, delim);
	while (token) {
		ret = kstrtoint(token, 16, &tmp_data);
		if (ret) {
			DISP_ERROR("input buffer conversion failed\n");
			goto exit_free1;
		}
		DISP_DEBUG("buffer[%d] = 0x%02x\n", buf_size, tmp_data);
		buffer[buf_size++] = (tmp_data & 0xff);
		/* Removes leading whitespace from input_copy */
		if (input_copy) {
			input_copy = skip_spaces(input_copy);
			token = strsep(&input_copy, delim);
		} else {
			token = NULL;
		}
	}
	ctl.tx_len = buf_size;
	ctl.tx_ptr = buffer;

	if (is_read) {
		recv_len = mi_dsi_display_read_dsi_cmd(dsi_display, &ctl);
		if (recv_len <= 0 || recv_len != ctl.rx_len) {
			DISP_ERROR("read dsi cmd transfer failed rc = %d\n", ret);
			ret = -EAGAIN;
		} else {
			g_dsi_read_info.is_read_sucess = true;
			g_dsi_read_info.rx_len = recv_len;
			ret = 0;
		}
	} else {
		ret = mi_dsi_display_write_dsi_cmd(dsi_display, &ctl);
	}

exit_free1:
	kfree(buffer);
exit_free0:
	kfree(input_dup);
exit:
	return ret;
}

ssize_t mi_dsi_display_show_mipi_rw(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	ssize_t count = 0;
	int i = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (g_dsi_read_info.is_read_sucess) {
		for (i = 0; i < g_dsi_read_info.rx_len; i++) {
			if (i == g_dsi_read_info.rx_len - 1) {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02X\n",
					 g_dsi_read_info.rx_buf[i]);
			} else {
				count += snprintf(buf + count, PAGE_SIZE - count, "0x%02X,",
					 g_dsi_read_info.rx_buf[i]);
			}
		}
	}

	return count;
}

ssize_t mi_dsi_display_read_panel_info(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	char *pname = NULL;
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	pname = mi_dsi_display_get_cmdline_panel_info(dsi_display);
	if (pname) {
		ret = snprintf(buf, size, "panel_name=%s\n", pname);
		kfree(pname);
	} else {
		if (dsi_display->name) {
			/* find the last occurrence of a character in a string */
			pname = strrchr(dsi_display->name, ',');
			if (pname && *pname)
				ret = snprintf(buf, size, "panel_name=%s\n", ++pname);
			else
				ret = snprintf(buf, size, "panel_name=%s\n", dsi_display->name);
		} else {
			ret = snprintf(buf, size, "panel_name=%s\n", "null");
		}
	}

	return ret;
}

int mi_dsi_display_read_panel_build_id(struct dsi_display *display)
{
	int rc = 0;
	struct drm_panel_build_id_config *config;
	struct dsi_cmd_desc cmd;
	struct dsi_panel *panel;
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!display || !display->panel)
		return -EINVAL;

	panel = display->panel;
	mi_cfg = &panel->mi_cfg;
	if (!mi_cfg || !mi_cfg->panel_build_id_read_needed) {
		DISP_INFO("panel build id do not have to read\n");
		return 0;
	}

	config = &(panel->id_config);

	cmd = config->id_cmd.cmds[0];

	rc = mi_dsi_display_cmd_read_locked(display, cmd, &config->build_id, config->id_cmds_rlen);

	if (rc <= 0)
		DISP_ERROR("[DSI] Display command receive failed, rc=%d\n", rc);
	else
		mi_cfg->panel_build_id_read_needed = false;

	return rc;
}

ssize_t mi_dsi_display_read_panel_build_id_info(void *display,
                        char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	struct dsi_panel *panel;
	int ret = 0;

	if (!dsi_display || !dsi_display->panel) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	panel = dsi_display->panel;
	if (panel->id_config.build_id) {
		ret = snprintf(buf, size, "0x%02X\n", panel->id_config.build_id);
	} else {
		ret = snprintf(buf, size, "%s\n", "Unsupported");
	}
	return ret;
}

static int mi_dsi_display_re_read_wp_info(struct dsi_display *display)
{
	int rc = 0;
	struct drm_panel_wp_config *config;
	struct dsi_cmd_desc cmd;
	struct dsi_panel_cmd_set *cmd_sets = NULL;

	if (!display || !display->panel)
		return -EINVAL;

	config = &(display->panel->wp_config);
	if (config->wp_cmd.count == 0 || config->wp_cmds_rlen == 0) {
		return -EINVAL;
	}

	cmd_sets = &config->pre_tx_cmd;
	if (cmd_sets->count != 0) {
		rc = mi_dsi_display_cmd_write(display, cmd_sets);
		if (rc) {
			DISP_ERROR("send cell id pre cmd failed!");
			return rc;
		}
	}

	cmd = config->wp_cmd.cmds[0];
	if (!config->return_buf) {
		DISP_ERROR("[%s] wp_info return buffer is null, rc=%d\n",
			display->name, rc);
		return -ENOMEM;
	}

	memset(config->return_buf, 0x0, sizeof(*config->return_buf));

	rc = mi_dsi_display_cmd_read(display, cmd, config->return_buf, config->wp_cmds_rlen);
	if (rc <= 0)
		DISP_ERROR("[DSI] Display command receive failed, rc=%d\n", rc);
	return rc;
}

ssize_t mi_dsi_display_read_wp_info(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	struct dsi_panel *panel;
	int display_id = 0;
	int ret = 0, i = 0;
	char* wp_info_str;

	if (!dsi_display || !dsi_display->panel) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	display_id = mi_get_disp_id(dsi_display->display_type);
	panel = dsi_display->panel;

	if (display_id == MI_DISP_PRIMARY)
		wp_info_str = oled_wp_info_str;
	else if (display_id == MI_DISP_SECONDARY)
		wp_info_str = sec_oled_wp_info_str;
	else {
		ret = snprintf(buf, size, "%s\n", "Unsupported display");
		return ret;
	}

	if (!strlen(wp_info_str)) {
		DISP_WARN("[%s-%d]read oled_wp_info_str() failed from cmdline\n",
				dsi_display->name, display_id);
		ret = mi_dsi_display_re_read_wp_info(display);
		if (ret <= 0) {
			wp_info_str = NULL;
			DISP_ERROR("[%s-%d] read wp_info failed, rc=%d\n",
					dsi_display->name, display_id, ret);
			return ret;
		}

		for (i = 0; i < panel->wp_config.wp_cmds_rlen-panel->wp_config.wp_read_info_index; i++) {
			snprintf(wp_info_str + i * 2, size - (i * 2), "%02x", panel->wp_config.return_buf[i+panel->wp_config.wp_read_info_index]);
		}
	}

	DISP_TIME_INFO("%s  display wp info is %s,index is %d\n", dsi_display->display_type, wp_info_str,panel->wp_config.wp_read_info_index);
	ret = snprintf(buf, size, "%s\n", wp_info_str);
	return ret;
}

int mi_dsi_display_get_fps(void *display, struct disp_fps_info *fps_info)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	struct dsi_display_mode *cur_mode = NULL;
	int ret = 0;

	if (!dsi_display || !dsi_display->panel || !fps_info) {
		DISP_ERROR("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_display->display_lock);
	cur_mode = dsi_display->panel->cur_mode;
	if (cur_mode) {
		fps_info->fps =  cur_mode->timing.refresh_rate;
	} else {
		ret = -EINVAL;
	}
	mutex_unlock(&dsi_display->display_lock);

	return ret;
}

int mi_dsi_display_set_doze_brightness(void *display,
			u32 doze_brightness)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int disp_id = MI_DISP_PRIMARY;
	int ret = 0;
	struct sde_kms *sde_kms = NULL;

	if (!dsi_display || !dsi_display->panel) {
		DISP_ERROR("invalid display/panel\n");
		return -EINVAL;
	}

	if (sde_kms_is_suspend_blocked(dsi_display->drm_dev)) {
		DISP_ERROR("sde_kms is suspended, skip to set doze brightness\n");
		return -EBUSY;
	}

	sde_kms = dsi_display_get_kms(dsi_display);
	if (!sde_kms) {
		DISP_ERROR("invalid kms\n");
		return -EINVAL;
	}

	sde_vm_lock(sde_kms);
	if (!sde_vm_owns_hw(sde_kms)) {
		DISP_ERROR("op not supported due to HW unavailablity\n");
		ret = -EOPNOTSUPP;
		goto end;
	}

	mi_dsi_acquire_wakelock(dsi_display->panel);
	mutex_lock(&dsi_display->panel->mi_cfg.doze_lock);
	SDE_ATRACE_BEGIN("set_doze_brightness");
	ret = mi_dsi_panel_set_doze_brightness(dsi_display->panel,
				doze_brightness);
	SDE_ATRACE_END("set_doze_brightness");
	mutex_unlock(&dsi_display->panel->mi_cfg.doze_lock);
	mi_dsi_release_wakelock(dsi_display->panel);

	disp_id = mi_get_disp_id(dsi_display->display_type);
	mi_disp_feature_event_notify_by_type(disp_id, MI_DISP_EVENT_DOZE,
			sizeof(doze_brightness), doze_brightness);

end:
	sde_vm_unlock(sde_kms);
	return ret;

}

int mi_dsi_display_get_doze_brightness(void *display,
			u32 *doze_brightness)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_doze_brightness(dsi_display->panel,
				doze_brightness);
}

int mi_dsi_display_get_brightness(void *display,
			u32 *brightness)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_brightness(dsi_display->panel,
				brightness);
}

int mi_dsi_display_write_dsi_cmd_set(void *display,
			int type)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (sde_kms_is_suspend_blocked(dsi_display->drm_dev)) {
		DISP_ERROR("sde_kms is suspended, skip to write dsi cmd_set\n");
		return -EBUSY;
	}

	mi_dsi_acquire_wakelock(dsi_display->panel);
	ret = mi_dsi_panel_write_dsi_cmd_set(dsi_display->panel, type);
	mi_dsi_release_wakelock(dsi_display->panel);

	return ret;
}

ssize_t mi_dsi_display_show_dsi_cmd_set_type(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_show_dsi_cmd_set_type(dsi_display->panel, buf, size);
}

int mi_dsi_display_set_brightness_clone(void *display,
			u32 brightness_clone)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	ret = mi_dsi_panel_set_brightness_clone(dsi_display->panel,
				brightness_clone);

	return ret;
}

int mi_dsi_display_get_brightness_clone(void *display,
			u32 *brightness_clone)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_brightness_clone(dsi_display->panel,
				brightness_clone);
}

int mi_dsi_display_get_max_brightness_clone(void *display,
			u32 *max_brightness_clone)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_max_brightness_clone(dsi_display->panel,
				max_brightness_clone);
}

ssize_t mi_dsi_display_get_hw_vsync_info(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_sde_encoder_calc_hw_vsync_info(dsi_display, buf, size);
}

static ssize_t compose_ddic_cell_id(char *outbuf, u32 outbuf_len,
		const char *inbuf, u32 inbuf_len)
{
	int i = 0;
	int idx = 0;
	ssize_t count =0;
	const char ddic_cell_id_dictionary[36] =
	{
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B',
		'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
		'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	};

	if (!outbuf || !inbuf) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	for (i = 0; i < inbuf_len; i++) {
		idx = inbuf[i] > 35 ? 35 : inbuf[i];
		if (i == inbuf_len - 1)
			count += snprintf(outbuf + count, outbuf_len - count, "%c\n",
				ddic_cell_id_dictionary[idx]);
		else
			count += snprintf(outbuf + count, outbuf_len - count, "%c",
				ddic_cell_id_dictionary[idx]);
		DISP_DEBUG("cell_id[%d] = 0x%02X, ch=%c\n", i, idx,
				ddic_cell_id_dictionary[idx]);
	}

	return count;
}

int mi_dsi_panel_read_lhbm_gamma_param_locked(struct dsi_display *display)
{
	int rc = 0;
	u8 rgb_1200nits_buf[19] = {0x31, 0x16, 0xCD, 0xB3, 0x23, 0x16, 0xCF, 0xB6, 0x26, 0x16, 0xD2, 0xB9, 0x29, 0x16, 0xD7, 0xC0, 0x2F, 0x07, 0x51};
	u8 rgb_250nits_buf[19] = {0x31, 0x2A, 0x54, 0x30, 0xDB, 0x2A, 0x54, 0x30, 0xDB, 0x2A, 0x58, 0x33, 0xD0, 0x2A, 0x60, 0x36, 0xD6, 0x06, 0x5E};
	u8 rgb_1200nits_rx_buffer[] = {0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xEC};
	u8 rgb_250nits_rx_buffer[] = {0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xEA};;
	int rdlength = 0;
	struct  dsi_panel *panel = display->panel;
	struct mi_dsi_panel_cfg *mi_cfg = &panel->mi_cfg;
	int j=0, rx_len;
	struct dsi_cmd_rw_ctl ctl;
	ktime_t ts = 0;


	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (!mi_cfg->lhbm_w1000_update_flag && !mi_cfg->lhbm_w110_update_flag) {
		DISP_DEBUG("[%s] don't need read back white rgb config\n", panel->type);
		return 0;
	}

	if (mi_cfg->uefi_read_lhbm_success ||
		(mi_cfg->lhbm_w1000_readbackdone && mi_cfg->lhbm_w110_readbackdone)) {
		DISP_DEBUG("lhbm white rgb read back done\n");
		rc = 0;
		goto exit;
	}

	DISP_INFO("[%s]  LHBM white read cmd start\n", panel->type);

	memset(&ctl, 0, sizeof(struct dsi_cmd_rw_ctl));

	if (mi_cfg->lhbm_w1000_update_flag) {
		rdlength = 19;
		rc = dsi_panel_tx_cmd_set(panel,
			DSI_CMD_SET_MI_LOCAL_HBM_WHITE_GAMMA_PRE_READ);
		if (rc) {
			DISP_ERROR("[%s] failed to send DSI_CMD_SET_MI_LOCAL_HBM_WHITE_GAMMA_PRE_READ cmds, rc=%d\n",
				panel->type, rc);
			rc = -EINVAL;
			goto exit;
		}

		rx_len = dsi_display_cmd_receive(display, rgb_1200nits_rx_buffer,
			ARRAY_SIZE(rgb_1200nits_rx_buffer), rgb_1200nits_buf, ARRAY_SIZE(rgb_1200nits_buf), &ts);
		if (rc < 0) {
			DISP_ERROR("[%s] read back 1200nits  gamma failed, rc=%d\n", panel->type, rc);
			goto exit;
		}

		for (j = 0; j < sizeof(mi_cfg->lhbm_1200nits_gamma); j++) {
			mi_cfg->lhbm_1200nits_gamma[j]=rgb_1200nits_buf[j];
			DISP_DEBUG("read from 0xEA lhbm_1200nits_gamma[%d] = 0x%02X\n", j, mi_cfg->lhbm_1200nits_gamma[j]);
		}
		mi_cfg->lhbm_w1000_readbackdone = true;
	} else {
		mi_cfg->lhbm_w1000_readbackdone = true;
	}

	if (mi_cfg->lhbm_w110_update_flag) {
		rdlength = 19;

		rc = dsi_panel_tx_cmd_set(panel,
			DSI_CMD_SET_MI_LOCAL_HBM_WHITE_GAMMA_PRE_READ);
		if (rc) {
			DISP_ERROR("[%s] failed to send DSI_CMD_SET_MI_LOCAL_HBM_WHITE_GAMMA_PRE_READ cmds, rc=%d\n",
				panel->type, rc);
			rc = -EINVAL;
			goto exit;
		}

		rx_len = dsi_display_cmd_receive(display, rgb_250nits_rx_buffer,
			ARRAY_SIZE(rgb_250nits_rx_buffer), rgb_250nits_buf, ARRAY_SIZE(rgb_250nits_buf), &ts);
		if (rx_len < 0) {
			DISP_ERROR("[%s] read 250nits gamma failed, rc=%d\n", panel->type, rc);
			goto exit;
		}

		for (j = 0; j < sizeof(mi_cfg->lhbm_250nits_gamma); j++) {
			mi_cfg->lhbm_250nits_gamma[j]=rgb_250nits_buf[j];
			DISP_DEBUG("read from lhbm_250nits_gamma[%d] = 0x%02X\n", j, mi_cfg->lhbm_250nits_gamma[j]);
		}
		mi_cfg->lhbm_w110_readbackdone = true;
	} else {
		mi_cfg->lhbm_w110_readbackdone = true;
	}
	DISP_INFO("[%s] LHBM white param read back end\n", panel->type);

exit:
	return rc;
}

static ssize_t mi_dsi_display_read_ddic_cell_id(struct dsi_display *display,
		char *buf, size_t size)
{
	int rc = 0;
	struct drm_panel_cell_id_config *config;
	struct dsi_cmd_desc cmd;
	struct dsi_panel_cmd_set *cmd_sets = NULL;

	if (!display || !display->panel)
		return -EINVAL;

	config = &(display->panel->cell_id_config);
	if (config->cell_id_cmd.count == 0 || config->cell_id_cmds_rlen == 0) {
		return -EINVAL;
	}

	cmd_sets = &config->pre_tx_cmd;
	if (cmd_sets->count != 0) {
		rc = mi_dsi_display_cmd_write(display, cmd_sets);
		if (rc) {
			DISP_ERROR("send cell id pre cmd failed!");
			return rc;
		}
	}

	cmd = config->cell_id_cmd.cmds[0];
	if (!config->return_buf) {
		DISP_ERROR("[%s] cell_id_info return buffer is null, rc=%d\n",
			display->name, rc);
		return -ENOMEM;
	}

	memset(config->return_buf, 0x0, sizeof(*config->return_buf));
	rc = mi_dsi_display_cmd_read(display, cmd, config->return_buf, config->cell_id_cmds_rlen);
	if (rc == config->cell_id_cmds_rlen) {
		rc = compose_ddic_cell_id(buf, size, config->return_buf, config->cell_id_cmds_rlen);
		DISP_INFO("cell_id = %s\n", buf);
	} else {
		DISP_ERROR("failed to read panel cell id, rc = %d\n", rc);
		return -EINVAL;
	}

	cmd_sets = &config->after_tx_cmd;
	if (cmd_sets->count != 0) {
		rc = mi_dsi_display_cmd_write(display, cmd_sets);
		if (rc) {
			DISP_ERROR("send cell id post cmd failed!");
			return rc;
		}
	}

	return rc;
}

ssize_t mi_dsi_display_read_cell_id(void *display,
			char *buf, size_t size)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (!strlen(cell_id_info_str)) {
		mi_dsi_acquire_wakelock(dsi_display->panel);
		ret = mi_dsi_display_read_ddic_cell_id(dsi_display, buf, size);
		mi_dsi_release_wakelock(dsi_display->panel);
	} else {
		ret = snprintf(buf, size, "%s\n", cell_id_info_str);
	}

	return ret;
}

int mi_dsi_display_set_disp_count(void *display, char *buf)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_set_disp_count(dsi_display->panel, buf);
}

int mi_dsi_display_get_disp_count(void *display,
			char *buf, size_t size )
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_disp_count(dsi_display->panel, buf, size);
}

int mi_dsi_display_esd_irq_ctrl(struct dsi_display *display,
			bool enable)
{
	int ret = 0;

	if (!display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	ret = mi_dsi_panel_esd_irq_ctrl(display->panel, enable);
	if (ret)
		DISP_ERROR("[%s] failed to set esd irq, rc=%d\n",
				display->name, ret);

	mutex_unlock(&display->display_lock);

	return ret;
}

int mi_dsi_display_read_gamma_ctrl(struct dsi_display *display)
{
	int ret = 0;

	if (!display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	mi_dsi_acquire_wakelock(display->panel);
	ret = mi_dsi_panel_read_lhbm_gamma_param_locked(display);
	if (ret)
		DISP_ERROR("[%s] failed to read gamma, rc=%d\n",
				display->name, ret);
	mi_dsi_release_wakelock(display->panel);

	return ret;
}

void mi_dsi_display_wakeup_pending_doze_work(struct dsi_display *display)
{
	int disp_id = 0;
	struct disp_display *dd_ptr;
	struct disp_feature *df = mi_get_disp_feature();

	if (!display) {
		DISP_ERROR("Invalid display ptr\n");
		return;
	}

	disp_id = mi_get_disp_id(display->display_type);
	dd_ptr = &df->d_display[disp_id];
	DISP_DEBUG("%s pending_doze_cnt = %d\n",
			display->display_type, atomic_read(&dd_ptr->pending_doze_cnt));
	if (atomic_read(&dd_ptr->pending_doze_cnt)) {
		DISP_INFO("%s display wake up pending doze work, pending_doze_cnt = %d\n",
			display->display_type, atomic_read(&dd_ptr->pending_doze_cnt));
		wake_up_interruptible_all(&dd_ptr->pending_wq);
	}
}

int mi_dsi_display_cmd_read_locked(struct dsi_display *display,
			      struct dsi_cmd_desc cmd, u8 *rx_buf, u32 rx_len)
{
	int rc = 0;
	bool state = false;
	cmd.msg.flags |= MIPI_DSI_MSG_UNICAST_COMMAND;
	cmd.msg.rx_buf = rx_buf;
	cmd.msg.rx_len = rx_len;
	cmd.ctrl_flags = DSI_CTRL_CMD_READ;

	rc = dsi_display_ctrl_get_host_init_state(display, &state);

	if (!rc && !state) {
		DISP_ERROR("Command xfer attempted while device is in suspend state\n");
		rc = -EPERM;
		goto end;
	}
	if (rc || !state) {
		DISP_ERROR("[DSI] Invalid host state %d rc %d\n",
				state, rc);
		rc = -EPERM;
		goto end;
	}

	rc = dsi_display_cmd_rx(display, &cmd);
	if (rc <= 0) 
		DISP_ERROR("[DSI] Display command receive failed, rc=%d\n", rc);

end:
	return rc;
}

int mi_dsi_display_cmd_read(struct dsi_display *display,
			      struct dsi_cmd_desc cmd, u8 *rx_buf, u32 rx_len)
{
	int rc = 0;

	if (!display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	rc = mi_dsi_display_cmd_read_locked(display, cmd, rx_buf, rx_len);
	mutex_unlock(&display->display_lock);
	return rc;
}

int mi_dsi_display_cmd_write_locked(struct dsi_display *display, struct dsi_panel_cmd_set *cmd_sets)
{
	int rc = 0;
	bool state = false;

	if (!display || !cmd_sets) {
		DISP_ERROR("Invalid param!\n");
		return -EINVAL;
	}

	rc = dsi_display_ctrl_get_host_init_state(display, &state);

	if (!rc && !state) {
		DISP_ERROR("Command xfer attempted while device is in suspend state\n");
		return -EPERM;
	}
	if (rc || !state) {
		DISP_ERROR("[DSI] Invalid host state %d rc %d\n", state, rc);
		return -EPERM;
	}

	rc =  mi_dsi_panel_write_cmd_set(display->panel, cmd_sets);
	if (rc != 0) {
		DISP_ERROR("[DSI] Display command send failed, rc=%d\n", rc);
	}

	return rc;
}

int mi_dsi_display_cmd_write(struct dsi_display *display, struct dsi_panel_cmd_set *cmd_sets)
{
	int rc = 0;

	if (!display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	dsi_panel_acquire_panel_lock(display->panel);
	rc = mi_dsi_display_cmd_write_locked(display, cmd_sets);
	dsi_panel_release_panel_lock(display->panel);
	mutex_unlock(&display->display_lock);

	return rc;
}

int mi_dsi_display_check_flatmode_status(void *display, bool *status)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int rc = 0;

	if (!display || !status) {
		DISP_ERROR("Invalid display/status ptr\n");
		return -EINVAL;
	}

	*status = false;

	if (sde_kms_is_suspend_blocked(dsi_display->drm_dev)) {
		DISP_ERROR("sde_kms is suspended, skip to write dsi cmd\n");
		return -EBUSY;
	}

	mi_dsi_acquire_wakelock(dsi_display->panel);
	rc = mi_dsi_panel_flatmode_validate_status(dsi_display, status);
	mi_dsi_release_wakelock(dsi_display->panel);

	return rc;
}

bool mi_dsi_display_ramdump_support(void)
{
	/* when debug policy is 0x0 or 0x20, full dump not supported */
	if (strcmp(display_debug_policy, "0x0") != 0 && strcmp(display_debug_policy, "0x20") != 0)
		return true;
	return false;
}

static void mi_display_pm_suspend_delayed_work_handler(struct kthread_work *work)
{
	struct disp_delayed_work *delayed_work = container_of(work,
					struct disp_delayed_work, delayed_work.work);
	struct dsi_panel *dsi_panel = (struct dsi_panel *)(delayed_work->data);
	struct mi_dsi_panel_cfg *mi_cfg = &dsi_panel->mi_cfg;
#ifdef LONG_PRESS_TO_FORCE_FASTBOOT
	struct dsi_panel_reset_config *r_config = &dsi_panel->reset_config;
#endif
	unsigned long mode_flags_backup = 0;
	int pmic_pwrkey_status = mi_cfg->pmic_pwrkey_status;
	int rc = 0;

	mi_dsi_acquire_wakelock(dsi_panel);

	dsi_panel_acquire_panel_lock(dsi_panel);
	if (dsi_panel->panel_initialized && pmic_pwrkey_status == PMIC_PWRKEY_RESIN_BARK_TRIGGER) {
		/* First disable esd irq, then send panel off cmd */
		if (mi_cfg->esd_err_enabled) {
			mi_dsi_panel_esd_irq_ctrl_locked(dsi_panel, false);
		}

		mode_flags_backup = dsi_panel->mipi_device.mode_flags;
		dsi_panel->mipi_device.mode_flags |= MIPI_DSI_MODE_LPM;
#ifdef LONG_PRESS_TO_FORCE_FASTBOOT
		dsi_panel_set_backlight(dsi_panel, 0);
#endif
		rc = mipi_dsi_dcs_set_display_off(&dsi_panel->mipi_device);
		dsi_panel->mipi_device.mode_flags = mode_flags_backup;
		if (rc < 0){
			DISP_ERROR("failed to send MIPI_DCS_SET_DISPLAY_OFF\n");
		} else {
			DISP_INFO("panel send MIPI_DCS_SET_DISPLAY_OFF\n");
		}
#ifdef LONG_PRESS_TO_FORCE_FASTBOOT
		rc = mipi_dsi_dcs_enter_sleep_mode(&dsi_panel->mipi_device);
		if (rc < 0){
			DISP_ERROR("ailed to send MIPI_DCS_SET_SLEEP_MODE\n");
		} else {
			DISP_INFO("panel send MIPI_DCS_SET_SLEEP_MODE\n");
		}
		if (gpio_is_valid(r_config->reset_gpio)) {
			rc = gpio_direction_output(r_config->reset_gpio, 1);
			if (rc) {
				DSI_ERR("unable to set dir for disp gpio rc=%d\n", rc);
			}
			gpio_set_value(r_config->reset_gpio,0);
		}
		dsi_panel_set_pinctrl_state(dsi_panel, false);
		dsi_panel->mipi_device.mode_flags = mode_flags_backup;
#endif
	} else {
		DISP_INFO("panel does not need to be off in advance\n");
	}
	dsi_panel_release_panel_lock(dsi_panel);

	mi_dsi_release_wakelock(dsi_panel);

	kfree(delayed_work);
}

int mi_display_pm_suspend_delayed_work(struct dsi_display *display)
{
	int disp_id = 0;
	struct disp_feature *df = mi_get_disp_feature();
	struct dsi_panel *panel = display->panel;
	struct disp_display *dd_ptr;
	struct disp_delayed_work *suspend_delayed_work;

	if (!panel) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	suspend_delayed_work = kzalloc(sizeof(*suspend_delayed_work), GFP_KERNEL);
	if (!suspend_delayed_work) {
		DISP_ERROR("failed to allocate delayed_work buffer\n");
		return -ENOMEM;
	}

	disp_id = mi_get_disp_id(panel->type);
	dd_ptr = &df->d_display[disp_id];

	kthread_init_delayed_work(&suspend_delayed_work->delayed_work,
			mi_display_pm_suspend_delayed_work_handler);
	suspend_delayed_work->dd_ptr = dd_ptr;
	suspend_delayed_work->wq = &dd_ptr->pending_wq;
	suspend_delayed_work->data = panel;
	return kthread_queue_delayed_work(dd_ptr->worker, &suspend_delayed_work->delayed_work,
				msecs_to_jiffies(DISPLAY_DELAY_SHUTDOWN_TIME_MS));
}

int mi_display_powerkey_callback(int status)
{
	struct dsi_display *dsi_display = mi_get_primary_dsi_display();
	struct dsi_panel *panel;
	struct mi_dsi_panel_cfg *mi_cfg;

	if (!dsi_display || !dsi_display->panel){
		DISP_ERROR("invalid dsi_display or dsi_panel ptr\n");
		return -EINVAL;
	}

	panel = dsi_display->panel;
	mi_cfg = &panel->mi_cfg;
	mi_cfg->pmic_pwrkey_status = status;

	if(status == PMIC_PWRKEY_RESIN_BARK_TRIGGER){
		return mi_display_pm_suspend_delayed_work(dsi_display);
	}
	return 0;
}

int mi_dsi_display_set_count_info(void *display,
			struct disp_count_info *count_info)
{
	struct dsi_display *dsi_display = (struct dsi_display *)display;
	int ret = 0;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	ret = mi_dsi_panel_set_count_info(dsi_display->panel,
				count_info);

	return ret;
}

module_param_string(oled_wp, oled_wp_info_str, MAX_CMDLINE_PARAM_LEN, 0600);
MODULE_PARM_DESC(oled_wp, "msm_drm.oled_wp=<wp info> while <wp info> is 'white point info' ");

module_param_string(sec_oled_wp, sec_oled_wp_info_str, MAX_CMDLINE_PARAM_LEN, 0600);
MODULE_PARM_DESC(sec_oled_wp, "msm_drm.sec_oled_wp=<wp info> while <wp info> is 'white point info' ");

module_param_string(cell_id, cell_id_info_str, MAX_CMDLINE_PARAM_LEN, 0600);
MODULE_PARM_DESC(cell_id, "msm_drm.cell_id=<cell id> while <cell id> is 'cell id info' ");

module_param_string(debugpolicy, display_debug_policy, MAX_DEBUG_POLICY_CMDLINE_LEN, 0600);
MODULE_PARM_DESC(debugpolicy, "msm_drm.debugpolicy=<debug policy> to indicate supporting ramdump or not ");

