// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hfi_connector.h"
#include "hfi_kms.h"
#include "hfi_crtc.h"
#include "hfi_props.h"
#include "sde_wb.h"
#include "hfi_plane.h"
#include "hfi_catalog.h"
#include "hfi_defs_display.h"
#include "hfi_wb.h"

static int _hfi_wb_add_roi_prop(struct sde_wb_device *wb_dev,
		struct sde_connector_state *cstate,
		struct hfi_util_u32_prop_helper *prop_collector,
		u32 disp_id)
{
	u32 prop_id;
	const struct drm_display_mode *mode = cstate->msm_mode.base;
	struct hfi_display_roi src_roi, dst_roi;
	struct sde_rect roi;
	int ret = 0;

	HFI_POPULATE_RECT(&src_roi, 0, 0, mode->hdisplay, mode->vdisplay, false);

	sde_wb_get_output_roi(wb_dev, &roi);
	HFI_POPULATE_RECT(&dst_roi, roi.x, roi.y, roi.w, roi.h, false);

	prop_id = HFI_PROPERTY_OUTPUT_LAYER_SRC_ROI;
	hfi_util_u32_prop_helper_add_prop_by_obj(prop_collector, prop_id, disp_id,
		HFI_VAL_U32_ARRAY, &src_roi, sizeof(struct hfi_display_roi));

	prop_id = HFI_PROPERTY_OUTPUT_LAYER_DST_ROI;
	hfi_util_u32_prop_helper_add_prop_by_obj(prop_collector, prop_id, disp_id,
		HFI_VAL_U32_ARRAY, &dst_roi, sizeof(struct hfi_display_roi));

	return ret;
}

static int _hfi_wb_add_drm_props(struct sde_wb_device *wb_dev,
		struct hfi_connector *hfi_conn,
		struct sde_connector_state *cstate,
		struct hfi_util_u32_prop_helper *prop_collector,
		u32 disp_id)
{
	u32 prop_id;
	int width, height;
	int ret = 0;
	u32 hfi_format;
	struct drm_framebuffer *fb;
	struct sde_hw_wb_cfg wb_cfg = {0,};
	struct sde_format_extended fmt = {0,};

	fb = sde_wb_get_output_fb(wb_dev);
	if (!fb) {
		SDE_ERROR("failed to get output buffer\n");
		return -EINVAL;
	}

	fmt.fourcc_format = fb->format->format;
	fmt.modifier = fb->modifier;

	hfi_format = hfi_catalog_get_hfi_format(&fmt);
	prop_id = HFI_PROPERTY_OUTPUT_LAYER_DST_FORMAT;
	hfi_util_u32_prop_helper_add_prop_by_obj(prop_collector, prop_id, disp_id,
		HFI_VAL_U32_ARRAY, &hfi_format, sizeof(u32));

	width = fb->width;
	prop_id = HFI_PROPERTY_OUTPUT_LAYER_SRC_IMG_SIZE_W;
	hfi_util_u32_prop_helper_add_prop_by_obj(prop_collector, prop_id, disp_id,
		HFI_VAL_U32, &width, sizeof(u32));

	height = fb->height;
	prop_id = HFI_PROPERTY_OUTPUT_LAYER_SRC_IMG_SIZE_H;
	hfi_util_u32_prop_helper_add_prop_by_obj(prop_collector, prop_id, disp_id,
		HFI_VAL_U32, &height, sizeof(u32));

	ret = sde_wb_get_scan_out_info(wb_dev, cstate, fb, &wb_cfg);
	if (ret) {
		SDE_ERROR("failed to get scan out info\n");
		return ret;
	}

	prop_id = HFI_PROPERTY_OUTPUT_LAYER_DST_ADDR;
	hfi_util_u32_prop_helper_add_prop_by_obj(prop_collector, prop_id, disp_id,
		HFI_VAL_U32_ARRAY, &wb_cfg.dest.plane_addr[0], (sizeof(u32) * SDE_MAX_PLANES));

	prop_id = HFI_PROPERTY_OUTPUT_LAYER_STRIDE;
	hfi_util_u32_prop_helper_add_prop_by_obj(prop_collector, prop_id, disp_id,
			HFI_VAL_U32_ARRAY, &wb_cfg.dest.plane_pitch[0],
			(sizeof(u32) * SDE_MAX_PLANES));

	_hfi_wb_add_roi_prop(wb_dev, cstate, hfi_conn->base_props, disp_id);

	SDE_DEBUG("Done adding hfi props for wb\n");

	return ret;
}

static int _hfi_wb_set_props_base(struct sde_wb_device *wb_dev, u32 disp_id,
		struct sde_connector_state *cstate, struct hfi_cmdbuf_t *cmd_buf)
{
	int ret = 0;
	int flags = 0;
	struct sde_connector *sde_conn;
	struct hfi_connector *hfi_conn;

	if (!wb_dev || !wb_dev->connector || !cmd_buf) {
		SDE_ERROR("invalid wb device\n");
		return -EINVAL;
	}

	sde_conn = to_sde_connector(wb_dev->connector);
	hfi_conn = sde_conn->hfi_conn;

	if (!hfi_conn) {
		SDE_ERROR("failed to get hfi connector\n");
		return -EINVAL;
	}

	mutex_lock(&hfi_conn->hfi_lock);
	hfi_util_u32_prop_helper_reset(hfi_conn->base_props);

	ret = _hfi_wb_add_drm_props(wb_dev, hfi_conn, cstate, hfi_conn->base_props, disp_id);
	if (ret) {
		SDE_ERROR("failed to add drm-prop HFI prop\n");
		goto end;
	}

	if (!hfi_util_u32_prop_helper_prop_count(hfi_conn->base_props))
		goto end;

	ret = hfi_adapter_add_set_property(cmd_buf->ctx,
		cmd_buf,
		HFI_COMMAND_DISPLAY_SET_PROPERTY,
		disp_id,
		HFI_PAYLOAD_TYPE_U32_ARRAY,
		hfi_util_u32_prop_helper_get_payload_addr(hfi_conn->base_props),
		hfi_util_u32_prop_helper_get_size(hfi_conn->base_props),
		flags);
	if (ret) {
		SDE_ERROR("failed to send HFI commands\n");
		goto end;
	}

end:
	mutex_unlock(&hfi_conn->hfi_lock);

	return ret;
}

static int _hfi_wb_populate_props(struct hfi_cmdbuf_t *cmd_buf, u32 disp_id,
		struct sde_wb_device *wb_dev, struct sde_connector_state *cstate)
{
	return _hfi_wb_set_props_base(wb_dev, disp_id, cstate, cmd_buf);
}

static int hfi_wb_add_hfi_cmds(struct hfi_cmdbuf_t *cmd_buf, u32 disp_id,
		struct sde_wb_device *wb_dev, struct sde_connector_state *cstate)
{
	return _hfi_wb_populate_props(cmd_buf, disp_id, wb_dev, cstate);
}

int hfi_wb_display_prepare_commit(struct sde_wb_device *wb_dev,
		struct sde_connector_state *cstate)
{
	int ret = 0;
	struct hfi_cmdbuf_t *cmd_buf = NULL;
	struct drm_connector *drm_conn;
	u32 disp_id;

	if (!wb_dev) {
		SDE_ERROR("invalid connector\n");
		return -EINVAL;
	}

	drm_conn = wb_dev->connector;
	disp_id = sde_conn_get_display_obj_id(drm_conn);

	cmd_buf = hfi_connector_get_cmd_buf(drm_conn, HFI_CMDBUF_TYPE_ATOMIC_COMMIT);
	if (!cmd_buf) {
		SDE_ERROR("failed to get cmd-buf in commit path\n");
		return -EINVAL;
	}

	ret = hfi_wb_add_hfi_cmds(cmd_buf, disp_id, wb_dev, cstate);
	if (ret) {
		SDE_ERROR("failed to add hfi cmds\n");
		return ret;
	}

	return ret;
}

