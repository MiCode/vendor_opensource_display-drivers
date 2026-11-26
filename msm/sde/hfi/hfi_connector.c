// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__

#include "hfi_catalog.h"
#include "hfi_connector.h"
#include "hfi_kms.h"
#include "hfi_wb.h"
#include "hfi_crtc.h"
#include "hfi_props.h"

#define HFI_CONNECTOR_ID(c) ((c)->sde_base->base.base.id)

#define HFI_DEBUG_CONN(c, fmt, ...) SDE_DEBUG("conn%d " fmt,\
		(c) ? HFI_CONNECTOR_ID(c) : -1, ##__VA_ARGS__)

#define HFI_ERROR_CONN(c, fmt, ...) SDE_ERROR("conn%d " fmt,\
		(c) ? HFI_CONNECTOR_ID(c) : -1, ##__VA_ARGS__)

#define to_hfi_connector(x) x->hfi_conn

#define HFI_CONNECTOR_MAX_PROPS 128
#define HFI_CONNECTOR_BASE_PROP_MAX_SIZE 1024

struct hfi_prop_map {
	u32 drm_prop;
	u32 hfi_prop;
	void (*add_hfi_prop)(u32 hfi_prop, struct sde_connector *conn,
			struct sde_connector_state *old_state,
			struct hfi_cmdbuf_t *cmd_buf);
};

/*
 * struct base_prop_lookup: tuple of drm property ID to HFI property ID
 */
struct base_prop_lookup {
	u32 drm_prop;
	u32 hfi_prop;
};

struct base_prop_lookup hfi_connector_base_props_map[] = {
	{ CONNECTOR_PROP_DYN_BIT_CLK, HFI_PROPERTY_DISPLAY_DYN_CLK_SUPPORT },
	{ CONNECTOR_PROP_QSYNC_MODE, HFI_PROPERTY_DISPLAY_QSYNC },
	{ CONNECTOR_PROP_AVR_STEP_STATE, HFI_PROPERTY_DISPLAY_AVR_STEP },

	// wb specific properties
	{ CONNECTOR_PROP_PP_CWB_DITHER, HFI_PROPERTY_DISPLAY_WB_CWB_DITHER },
	{ CONNECTOR_PROP_WB_ROT_TYPE, HFI_PROPERTY_DISPLAY_WB_LINEAR_ROTATION },
};

struct hfi_prop_map hfi_connector_kv_props_map[] = {
	{ CONNECTOR_PROP_ROI_V1, HFI_PROPERTY_LAYER_BLEND_ROI, sde_connector_add_roi_v1 },
};

void sde_connector_add_roi_v1(u32 hfi_prop, struct sde_connector *conn,
		struct sde_connector_state *old_state, struct hfi_cmdbuf_t *cmd_buf)
{
	struct msm_roi_list msm_roi = old_state->rois;
	struct hfi_connector *hfi_conn;
	u32 key;
	int ret = 0;

	if (!conn || !cmd_buf || !old_state)
		return;

	hfi_conn = to_hfi_connector(conn);
	if (!msm_property_is_dirty(&conn->property_info,
				&old_state->property_state,
				CONNECTOR_PROP_ROI_V1)) {
		HFI_DEBUG_CONN(hfi_conn, "not dirty %s : %d\n", __func__, __LINE__);
	}

	if (old_state->rois.num_rects == 0)
		HFI_DEBUG_CONN(hfi_conn, "num rects = 0 %s : %d\n", __func__, __LINE__);

	return;

	key = HFI_PACKKEY(HFI_PROPERTY_LAYER_BLEND_ROI, 0, sizeof(msm_roi));

	ret = hfi_util_kv_helper_add(hfi_conn->kv_props, key, (u32 *)&msm_roi);
	if (ret)
		HFI_ERROR_CONN(hfi_conn, "failed adding HFI KV prop:0x%x\n", hfi_prop);
}

void sde_connector_add_autorefresh(u32 hfi_prop, struct sde_connector *conn,
		struct sde_connector_state *old_state, struct hfi_cmdbuf_t *cmd_buf,
		bool is_cont_splash)
{
	struct hfi_connector *hfi_conn;
	struct hfi_display_autorefresh_cfg payload;
	u32 key;
	int ret = 0;

	if (!conn || !cmd_buf || !old_state)
		return;

	hfi_conn = to_hfi_connector(conn);

	/* For HFI cont-splash - disable autorefresh */
	if (is_cont_splash) {
		payload.enable = false;
		payload.frame_count = 0;
	}

	key = HFI_PACKKEY(HFI_PROPERTY_DISPLAY_AUTOREFRESH_CFG, 0, sizeof(payload));

	ret = hfi_util_kv_helper_add(hfi_conn->kv_props, key, (u32 *)&payload);
	if (ret)
		HFI_ERROR_CONN(hfi_conn, "failed adding HFI KV prop:0x%x\n", hfi_prop);
}

int _hfi_connector_add_base_prop_helper(u32 hfi_prop, struct sde_connector *conn,
		struct sde_connector_state *old_cstate,
		struct hfi_util_u32_prop_helper *prop_collector)
{
	u32 val;
	struct sde_connector_state *state;
	struct hfi_connector *hfi_conn;
	int ret = 0;

	if (!conn || !old_cstate || !prop_collector || conn->base.state)
		return -EINVAL;

	state = (struct sde_connector_state *)conn->base.state;
	hfi_conn = to_hfi_connector(conn);

	switch (hfi_prop) {
	case HFI_PROPERTY_DISPLAY_DYN_CLK_SUPPORT:
		val = sde_connector_get_property(&old_cstate->base, CONNECTOR_PROP_DYN_BIT_CLK);
		break;
	case HFI_PROPERTY_DISPLAY_QSYNC:
		val = sde_connector_get_property(&old_cstate->base, CONNECTOR_PROP_QSYNC_MODE);
		break;
	case HFI_PROPERTY_DISPLAY_AVR_STEP:
		val = sde_connector_get_property(&old_cstate->base, CONNECTOR_PROP_AVR_STEP_STATE);
		break;
	case HFI_PROPERTY_DISPLAY_WB_CWB_DITHER:
		val = sde_connector_get_property(&old_cstate->base, CONNECTOR_PROP_PP_CWB_DITHER);
		break;
	case HFI_PROPERTY_DISPLAY_WB_LINEAR_ROTATION:
		val = sde_connector_get_property(&old_cstate->base, CONNECTOR_PROP_WB_ROT_TYPE);
		break;
	default:
		HFI_ERROR_CONN(hfi_conn, "failed to send HFI commands\n");
		return -EINVAL;
	}

	ret = hfi_util_u32_prop_helper_add_prop_by_obj(prop_collector,
			hfi_prop, conn->base.base.id, HFI_VAL_U32, &val, sizeof(u32));
	if (ret)
		HFI_ERROR_CONN(hfi_conn, "failed adding HFI prop:0x%x\n", hfi_prop);

	HFI_DEBUG_CONN(hfi_conn, "done adding HFI prop:0x%x\n", hfi_prop);

	return 0;
}

/**
 * hfi_connector_populate_custom_kv_setter_props:  this is for all basic payloads.
 * Collects all listed props into a linear memory and voids memcopy of value by value at adapeter
 */
static int _hfi_connector_set_props_base(struct sde_connector *conn, u32 disp_id,
		struct sde_connector_state *old_cstate, struct hfi_cmdbuf_t *cmd_buf)
{
	u32 drm_prop, hfi_prop;
	int i, ret = 0;
	struct hfi_connector *hfi_conn = to_hfi_connector(conn);

	if (!hfi_conn || !hfi_conn->base_props) {
		SDE_ERROR("invalid connector\n");
		return -EINVAL;
	}

	mutex_lock(&hfi_conn->hfi_lock);
	hfi_util_u32_prop_helper_reset(hfi_conn->base_props);

	/* append msm properties */
	for (i = 0; i < ARRAY_SIZE(hfi_connector_base_props_map); i++) {
		drm_prop = hfi_connector_base_props_map[i].drm_prop;
		hfi_prop = hfi_connector_base_props_map[i].hfi_prop;
		if (!sde_connector_property_is_dirty(old_cstate, drm_prop))
			continue;

		_hfi_connector_add_base_prop_helper(hfi_prop, conn, old_cstate,
				 hfi_conn->base_props);
	}

	if (!hfi_util_u32_prop_helper_prop_count(hfi_conn->base_props))
		goto end;

	/*
	 * Once all the key value pairs of properties are collected invoke adapter api
	 * to add all these property array as a single HFI Packet
	 */
	ret = hfi_adapter_add_set_property(cmd_buf->ctx,
			cmd_buf,
			HFI_COMMAND_DISPLAY_SET_PROPERTY,
			disp_id,
			HFI_PAYLOAD_TYPE_U32_ARRAY,
			hfi_util_u32_prop_helper_get_payload_addr(hfi_conn->base_props),
			hfi_util_u32_prop_helper_get_size(hfi_conn->base_props),
			HFI_HOST_FLAGS_NON_DISCARDABLE);
	if (ret) {
		HFI_ERROR_CONN(hfi_conn, "failed to send HFI commands\n");
		goto end;
	}

	HFI_DEBUG_CONN(hfi_conn, "done adding all base props\n");

end:
	mutex_unlock(&hfi_conn->hfi_lock);

	return ret;
}

/**
 * hfi_connector_populate_custom_kv_setter_props:  this is for large payloads.
 * Collects all listed props to provide as key-value pairs and adapter does memcopy
 */
int hfi_connector_populate_custom_kv_setter_props(struct sde_connector *conn, u32 disp_id,
		struct sde_connector_state *old_cstate, struct hfi_cmdbuf_t *cmd_buf)
{
	struct hfi_prop_map *setter;
	int i, ret = 0;
	struct hfi_connector *hfi_conn = to_hfi_connector(conn);
	struct sde_kms *sde_kms;
	struct msm_kms *msm_kms;
	u32 kv_count;
	bool is_cont_splash = false;

	if (!hfi_conn || !old_cstate || !cmd_buf) {
		SDE_ERROR("invalid connector\n");
		return -EINVAL;
	}

	sde_kms = sde_connector_get_kms(&conn->base);
	if (!sde_kms)
		return -EINVAL;
	msm_kms = &sde_kms->base;
	if (!msm_kms)
		return -EINVAL;

	mutex_lock(&hfi_conn->hfi_lock);
	hfi_util_kv_helper_reset(hfi_conn->kv_props);

	for (i = 0; i < ARRAY_SIZE(hfi_connector_kv_props_map); i++) {
		setter = &hfi_connector_kv_props_map[i];
		if (!sde_connector_property_is_dirty(old_cstate, setter->drm_prop)) {
			HFI_DEBUG_CONN(hfi_conn, "conn prop is not dirty\n");
			continue;
		}

		if (setter->add_hfi_prop)
			setter->add_hfi_prop(setter->hfi_prop, conn, old_cstate, cmd_buf);
	}

	/* Check continuous splash HFI for autorefresh disable */
	if (msm_kms->funcs && msm_kms->funcs->check_for_splash)
		is_cont_splash = msm_kms->funcs->check_for_splash(msm_kms);

	if (is_cont_splash)
		sde_connector_add_autorefresh(HFI_PROPERTY_DISPLAY_AUTOREFRESH_CFG,
				conn, old_cstate, cmd_buf, is_cont_splash);

	kv_count = hfi_util_kv_helper_get_count(hfi_conn->kv_props);
	if (!kv_count)
		goto end;

	ret = hfi_adapter_add_prop_array(cmd_buf->ctx,
			cmd_buf,
			HFI_COMMAND_DISPLAY_SET_PROPERTY,
			disp_id,
			HFI_PAYLOAD_TYPE_U32_ARRAY,
			hfi_util_kv_helper_get_payload_addr(hfi_conn->kv_props),
			kv_count,
			kv_count * sizeof(struct hfi_kv_pairs));
	if (ret) {
		HFI_DEBUG_CONN(hfi_conn, "failed to send HFI commands\n");
		goto end;
	}

end:
	HFI_DEBUG_CONN(hfi_conn, "adding kv-props count%d\n", kv_count);
	mutex_unlock(&hfi_conn->hfi_lock);

	return ret;
}

int _hfi_connector_populate_props(struct hfi_cmdbuf_t *cmd_buf, u32 disp_id,
		struct sde_connector *conn, struct sde_connector_state *old_cstate)
{
	int ret = 0;
	struct hfi_connector *hfi_conn = to_hfi_connector(conn);

	if (!hfi_conn) {
		SDE_ERROR("invalid connector\n");
		return -EINVAL;
	}

	ret = _hfi_connector_set_props_base(conn, disp_id, old_cstate, cmd_buf);
	if (ret) {
		HFI_ERROR_CONN(hfi_conn, "failed to send hfis\n");
		return ret;
	}

	ret = hfi_connector_populate_custom_kv_setter_props(conn, disp_id, old_cstate, cmd_buf);
	if (ret) {
		HFI_ERROR_CONN(hfi_conn, "failed to send hfis\n");
		return ret;
	}

	return ret;
}

void hfi_connector_destroy(struct sde_connector *conn)
{
	struct hfi_connector *conn_hfi = (struct hfi_connector *)to_hfi_connector(conn);

	if (!conn_hfi) {
		SDE_ERROR("invalid connector\n");
		return;
	}

	kfree(conn_hfi->base_props);
	kfree(conn_hfi->kv_props);

	mutex_destroy(&conn_hfi->hfi_lock);

	kfree(conn_hfi);
}

static int hfi_conn_add_hfi_cmds(struct hfi_cmdbuf_t *cmd_buf, u32 disp_id,
		struct sde_connector *conn, struct sde_connector_state *cstate)
{
	int ret = 0;

	if (!conn || !cmd_buf || !cstate) {
		SDE_ERROR("invalid args\n");
		return -EINVAL;
	}

	ret = _hfi_connector_populate_props(cmd_buf, disp_id, conn, cstate);

	return ret;
}

static int _hfi_conn_add_init_caps_cmd(struct hfi_cmdbuf_t *cmd_buf,
		struct sde_connector *conn)
{
	int ret = 0;
	u32 num_modes = 0;
	u32 obj_id = 0;
	struct drm_connector *drm_conn;
	struct hfi_connector *hfi_conn;
	struct drm_display_mode *mode;

	if (!cmd_buf || !conn)
		return -EINVAL;

	hfi_conn = to_hfi_connector(conn);
	if (!hfi_conn->base_props)
		return -EINVAL;

	drm_conn = &conn->base;

	mutex_lock(&hfi_conn->hfi_lock);

	hfi_util_u32_prop_helper_reset(hfi_conn->base_props);

	if (conn->connector_type == DRM_MODE_CONNECTOR_VIRTUAL) {
		// Avoid populating noedid modes.
		if (!list_empty(&drm_conn->modes))
			num_modes = 1;
		else
			num_modes = 0;
	} else {
		list_for_each_entry(mode, &drm_conn->modes, head)
			num_modes++;
	}

	hfi_util_u32_prop_helper_add_prop(hfi_conn->base_props,
			HFI_PROPERTY_PANEL_TIMING_MODE_COUNT, HFI_VAL_U32, &num_modes, sizeof(u32));

	ret = hfi_adapter_add_set_property(cmd_buf->ctx,
			cmd_buf,
			HFI_COMMAND_PANEL_INIT_PANEL_CAPS,
			obj_id,
			HFI_PAYLOAD_TYPE_U32_ARRAY,
			hfi_util_u32_prop_helper_get_payload_addr(hfi_conn->base_props),
			hfi_util_u32_prop_helper_get_size(hfi_conn->base_props),
			HFI_HOST_FLAGS_NON_DISCARDABLE);
	if (ret) {
		SDE_ERROR("failed to add panel init caps command\n");
		goto end;
	}
end:
	mutex_unlock(&hfi_conn->hfi_lock);

	return ret;
}

static void _hfi_conn_get_mode_res_data(struct drm_display_mode *mode,
		struct hfi_panel_res_data *res_data)
{
	if (!mode || !res_data)
		return;

	res_data->active_width = mode->hdisplay;
	res_data->active_height = mode->vdisplay;
	res_data->h_front_porch = mode->hsync_start - mode->hdisplay;
	res_data->h_back_porch = mode->htotal - mode->hsync_end;
	res_data->h_sync_skew = mode->hskew;
	res_data->h_sync_pulse = mode->hsync_end - mode->hsync_start;
	res_data->v_front_porch =  mode->vsync_start - mode->vdisplay;
	res_data->v_back_porch = mode->vtotal - mode->vsync_end;
	res_data->v_pulse_width = mode->vsync_end - mode->vsync_start;
}

static int _hfi_conn_add_timing_caps_cmd(struct hfi_cmdbuf_t *cmd_buf,
		struct sde_connector *conn)
{
	int ret = 0;
	u32 mode_idx = 0;
	u64 mode_clock;
	u64 jitter  = ((u64)0x1 << 32);
	u32 refresh;
	u32 obj_id = 0;
	struct hfi_connector *hfi_conn;
	struct drm_connector *drm_conn;
	struct drm_display_mode *mode;
	struct hfi_panel_res_data res_data;
	struct hfi_panel_compression_params compression_params = {0,};

	if (!cmd_buf || !conn)
		return -EINVAL;

	hfi_conn = to_hfi_connector(conn);
	if (!hfi_conn->base_props)
		return -EINVAL;

	drm_conn = &conn->base;
	mutex_lock(&hfi_conn->hfi_lock);

	list_for_each_entry(mode, &drm_conn->modes, head) {
		if (conn->connector_type == DRM_MODE_CONNECTOR_VIRTUAL &&
			!(mode->type & DRM_MODE_TYPE_PREFERRED))
			continue;
		hfi_util_u32_prop_helper_reset(hfi_conn->base_props);
		mode_clock = mode->clock * 1000;
		refresh = drm_mode_vrefresh(mode);

		hfi_util_u32_prop_helper_add_prop(hfi_conn->base_props, HFI_PROPERTY_PANEL_INDEX,
				HFI_VAL_U32, &mode_idx, sizeof(u32));
		hfi_util_u32_prop_helper_add_prop(hfi_conn->base_props,
				HFI_PROPERTY_PANEL_CLOCKRATE, HFI_VAL_U32_ARRAY,
				&mode_clock, 2 * sizeof(u32));
		hfi_util_u32_prop_helper_add_prop(hfi_conn->base_props,
				HFI_PROPERTY_PANEL_FRAMERATE, HFI_VAL_U32,
				&refresh, sizeof(u32));
		hfi_util_u32_prop_helper_add_prop(hfi_conn->base_props,
				HFI_PROPERTY_PANEL_JITTER, HFI_VAL_U32_ARRAY,
				&jitter, 2 * sizeof(u32));

		_hfi_conn_get_mode_res_data(mode, &res_data);
		hfi_util_u32_prop_helper_add_prop(hfi_conn->base_props,
				HFI_PROPERTY_PANEL_RESOLUTION_DATA, HFI_VAL_U32_ARRAY,
				&res_data, sizeof(struct hfi_panel_res_data));
		hfi_util_u32_prop_helper_add_prop(hfi_conn->base_props,
				HFI_PROPERTY_PANEL_COMPRESSION_DATA, HFI_VAL_U32_ARRAY,
				&compression_params, sizeof(struct hfi_panel_compression_params));

		ret = hfi_adapter_add_set_property(cmd_buf->ctx,
				cmd_buf,
				HFI_COMMAND_PANEL_INIT_TIMING_MODE_CAPS,
				obj_id,
				HFI_PAYLOAD_TYPE_U32_ARRAY,
				hfi_util_u32_prop_helper_get_payload_addr(hfi_conn->base_props),
				hfi_util_u32_prop_helper_get_size(hfi_conn->base_props),
				HFI_HOST_FLAGS_NON_DISCARDABLE);
		if (ret) {
			SDE_ERROR("failed to add panel init caps command\n");
			goto end;
		}
		mode_idx++;
	}
end:
	mutex_unlock(&hfi_conn->hfi_lock);

	return ret;
}

int hfi_conn_send_panel_init(struct drm_connector *conn)
{
	int ret = 0;
	struct sde_kms *sde_kms;
	struct hfi_kms *hfi_kms;
	struct sde_connector *c_conn;
	struct hfi_cmdbuf_t *cmd_buf;

	if (!conn) {
		SDE_ERROR("invalid args\n");
		return -EINVAL;
	}

	/**
	 * currently this function is handling only for WB to send client modes.
	 * If required, we could extend this to DSI
	 */
	c_conn = to_sde_connector(conn);
	if (c_conn->connector_type != DRM_MODE_CONNECTOR_VIRTUAL)
		return ret;

	sde_kms = sde_connector_get_kms(conn);
	if (!sde_kms) {
		SDE_ERROR("failed to get sde_kms\n");
		return -EINVAL;
	}

	hfi_kms = to_hfi_kms(sde_kms);

	cmd_buf = hfi_adapter_get_cmd_buf(&hfi_kms->hfi_client,
			MSM_DRV_HFI_ID, HFI_CMDBUF_TYPE_DEVICE_INFO);
	if (!cmd_buf) {
		SDE_ERROR("failed to get command buf\n");
		return -EINVAL;
	}

	ret = _hfi_conn_add_init_caps_cmd(cmd_buf, c_conn);
	if (ret) {
		SDE_ERROR("failed to add panel init caps command\n");
		goto end;
	}

	ret = _hfi_conn_add_timing_caps_cmd(cmd_buf, c_conn);
	if (ret) {
		SDE_ERROR("failed to add panel int timing caps command\n");
		goto end;
	}
end:
	ret = hfi_adapter_set_cmd_buf(&hfi_kms->hfi_client, cmd_buf);
	if (ret) {
		SDE_ERROR("failed to send panel int command\n");
		return ret;
	}

	return ret;
}

int hfi_connector_prepare_commit(struct drm_connector *conn, struct sde_connector_state *cstate)
{
	int ret = 0;
	struct sde_kms *sde_kms;
	struct hfi_kms *hfi_kms;
	struct hfi_cmdbuf_t *cmd_buf;
	struct sde_connector *sde_conn;
	u32 disp_id;

	if (!conn) {
		SDE_ERROR("invalid args\n");
		return -EINVAL;
	}

	sde_conn = to_sde_connector(conn);
	if (sde_conn->connector_type == DRM_MODE_CONNECTOR_VIRTUAL)
		return hfi_wb_display_prepare_commit((struct sde_wb_device *) sde_conn->display,
				cstate);

	sde_kms = sde_connector_get_kms(conn);
	hfi_kms = to_hfi_kms(sde_kms);

	disp_id = sde_conn_get_display_obj_id(conn);
	cmd_buf = hfi_kms_get_cmd_buf(hfi_kms, disp_id, HFI_CMDBUF_TYPE_ATOMIC_COMMIT);
	if (!cmd_buf) {
		SDE_ERROR("conn:%d failed to get cmd-buf in commit path\n", DRMID(conn));
		return -EINVAL;
	}

	ret = hfi_conn_add_hfi_cmds(cmd_buf, disp_id, sde_conn, cstate);
	if (ret) {
		SDE_ERROR("failed to add hfi cmds\n");
		return ret;
	}

	return ret;
}

static void _sde_connector_hal_funcs_install(struct sde_connector *conn)
{
	if (!conn) {
		SDE_ERROR("invalid args\n");
		return;
	}

	conn->hal_ops.destroy[MSM_DISP_OP_HFI] = hfi_connector_destroy;
	conn->hal_ops.prepare_commit[MSM_DISP_OP_HFI] = hfi_connector_prepare_commit;
}

struct hfi_cmdbuf_t *hfi_connector_get_cmd_buf(struct drm_connector *drm_conn,
		u32 cmd_buf_type)
{
	struct sde_kms *sde_kms = sde_connector_get_kms(drm_conn);
	struct sde_connector *sde_conn = to_sde_connector(drm_conn);
	struct hfi_kms *hfi_kms;

	if (!sde_kms || !sde_conn) {
		SDE_ERROR("invalid connector\n");
		return NULL;
	}

	hfi_kms = to_hfi_kms(sde_kms);

	return hfi_kms_get_cmd_buf(hfi_kms, sde_conn->conn_id, cmd_buf_type);
}

int hfi_connector_init(int connector_type, struct sde_connector *c_conn)
{
	struct hfi_connector *hfi_conn = NULL;
	struct msm_display_info display_info;
	int rc = 0;
	struct sde_kms *sde_kms = sde_connector_get_kms(&c_conn->base);
	struct hfi_kms *hfi_kms;

	if (!sde_kms)
		return -EINVAL;

	hfi_kms = sde_kms->hfi_kms;
	hfi_conn = kvzalloc(sizeof(*hfi_conn), GFP_KERNEL);
	if (!hfi_conn) {
		SDE_ERROR("[%u] failed to allocate memory for hfi connector\n", connector_type);
		return -ENOMEM;
	}

	mutex_init(&hfi_conn->hfi_lock);

	_sde_connector_hal_funcs_install(c_conn);

	hfi_conn->kv_props = hfi_util_kv_helper_alloc(HFI_CONNECTOR_MAX_PROPS);
	if (IS_ERR(hfi_conn->kv_props)) {
		SDE_ERROR("failed to allocate memory for base property collector\n");
		goto free_conn;
	}

	hfi_conn->base_props = hfi_util_u32_prop_helper_alloc(HFI_CONNECTOR_BASE_PROP_MAX_SIZE);
	if (IS_ERR(hfi_conn->base_props)) {
		SDE_ERROR("failed to allocate memory for base prop collector\n");
		goto free_kv;
	}

	rc = sde_connector_get_info(&c_conn->base, &display_info);
	if (rc) {
		SDE_ERROR("failed to get display info %d\n", rc);
		goto free_kv;
	}

	if (display_info.display_type == SDE_CONNECTOR_PRIMARY)
		hfi_kms->primary_connector = hfi_conn;

	hfi_conn->sde_base = c_conn;
	c_conn->hfi_conn = hfi_conn;

	return rc;

free_kv:
	kfree(hfi_conn->base_props);
free_conn:
	mutex_destroy(&hfi_conn->hfi_lock);
	kfree(hfi_conn);

	return -ENOMEM;
}
