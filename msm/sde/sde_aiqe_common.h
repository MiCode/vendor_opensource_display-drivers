/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _SDE_AIQE_COMMON_H
#define _SDE_AIQE_COMMON_H

#include "sde_crtc.h"

enum aiqe_merge_mode {
	SINGLE_MODE,
	DUAL_MODE,
	QUAD_MODE = 0x3,
	MERGE_MODE_MAX,
};

enum aiqe_features {
	FEATURE_MDNIE = 0x1,
	FEATURE_MDNIE_ART,
	FEATURE_ABC,
	FEATURE_SSRC,
	FEATURE_COPR,
	AIQE_FEATURE_MAX,
};

struct aiqe_reg_common {
	enum aiqe_merge_mode merge;
	u32 config;
	u32 height; // panel
	u32 width; // panel
};

void aiqe_init(u32 aiqe_version, struct sde_aiqe_top_level *aiqe_top);
void aiqe_register_client(enum aiqe_features feature_id, struct sde_aiqe_top_level *aiqe_top);
void aiqe_deregister_client(enum aiqe_features feature_id, struct sde_aiqe_top_level *aiqe_top);
void aiqe_get_common_values(struct sde_hw_cp_cfg *cfg,
			    struct sde_aiqe_top_level *aiqe_top, struct aiqe_reg_common *aiqe_cmn);
bool mdnie_art_in_progress(struct sde_aiqe_top_level *aiqe_top);
void aiqe_deinit(struct sde_aiqe_top_level *aiqe_top);

#endif /* _SDE_AIQE_COMMON_H */
