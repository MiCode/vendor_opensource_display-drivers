/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _HFI_CRTC_H_
#define _HFI_CRTC_H_
#include <linux/types.h>

#include "sde_crtc.h"
#include "hfi_utils.h"
#include "hfi_adapter.h"

/**
 * struct hfi_crtc - virtualized hfi CRTC data structure
 * @sde_base: Pointer to base sde crtc structure
 * @hfi_lock: Mutex to protect hfi specific data
 * @base_props: prop helper object for intermediate property collection
 * @color_props: color prop helper object for intermediate property collection
 * @kv_props: kv pair helper object for intermediate property collection
 * @misr_read_listener: hfi listener for MISR
 * @hfi_buff_map_dither: hfi_buff map object for SPR dither
 * @prev_plane_mask: tracks the previous plane mask
 * @pending_enc_mask: encoder_mask that has pending commit on the drm_crtc
 */
struct hfi_crtc {
	struct sde_crtc *sde_base;
	struct mutex hfi_lock;
	struct hfi_util_u32_prop_helper *base_props;
	struct hfi_util_u32_prop_helper *color_props;
	struct hfi_util_kv_helper *kv_props;
	struct hfi_prop_listener misr_read_listener;
	struct hfi_shared_addr_map hfi_buff_map_dither;
	uint32_t prev_plane_mask;
	u32 pending_enc_mask;
};

/**
 * struct hfi_crtc_state - hfi container for atomic crtc state
 * @sde_base: Pointer to base sde crtc state structure
 */
struct hfi_crtc_state {
	struct sde_crtc_state *sde_base;
};

/**
 * hfi_crtc_set_pending_enc_mask - helper to set/reset the enc mask for caching
 * @sde_crtc: Pointer to sde crtc struct
 * @enc_mask: input encoder mask to cache
 */
void hfi_crtc_set_pending_enc_mask(struct sde_crtc *sde_crtc, u32 enc_mask);

#if IS_ENABLED(CONFIG_MDSS_HFI)
/**
 * hfi_crtc_init - create a new hfi crtc object
 * @sde_crtc: Pointer to sde crtc struct
 * @Returns: 0 on success, or error code on failure
 */
int hfi_crtc_init(struct sde_crtc *sde_crtc);
/**
 * hfi_crtc_get_display_id - Retrieve the display ID for a given CRTC
 * @crtc: Pointer to the DRM CRTC structure
 * @crtc_state: Pointer to the DRM CRTC state structure
 * Return: display ID
 */
u32 hfi_crtc_get_display_id(struct drm_crtc *crtc, struct drm_crtc_state *crtc_state);

/**
 * hfi_crtc_destroy_shared_map_buffers - Destroy shared buffers mapped to
 *                                       firmware for crtc
 * @crtc: Pointer to sde crtc struct
 * @Returns: 0 on success, or error code on failure
 */
int hfi_crtc_destroy_shared_map_buffers(struct sde_crtc *crtc);

/**
 * hfi_crtc_alloc_shared_map_buffers - Allocate shared buffers mapped to
 *                                     firmware for crtc
 * @crtc: Pointer to sde crtc struct
 * @Returns: 0 on success, or error code on failure
 */
int hfi_crtc_alloc_shared_map_buffers(struct sde_crtc *crtc);

#else
int hfi_crtc_init(struct sde_crtc *sde_crtc)
{
	return -HFI_ERROR;
}

u32 hfi_crtc_get_display_id(struct drm_crtc *crtc, struct drm_crtc_state *crtc_state)
{
	return U32_MAX;
}

void hfi_crtc_set_pending_enc_mask(struct sde_crtc *sde_crtc, u32 enc_mask)
{
}
int hfi_crtc_destroy_shared_map_buffers(struct sde_crtc *crtc)
{
	return 0;
}

int hfi_crtc_alloc_shared_map_buffers(struct sde_crtc *crtc)
{
	return 0;
}

#endif // IS_ENABLED(CONFIG_MDSS_HFI)

#endif  // _HFI_CRTC_H_
