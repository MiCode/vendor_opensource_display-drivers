/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __HFI_WB_H__
#define __HFI_WB_H__

#include "hfi_utils.h"
#include "sde_wb.h"

#define HFI_WB_BASE_PROP_MAX_SIZE 1024

struct hfi_wb_device {
	struct sde_wb_device sde_wb_base;

	struct mutex hfi_lock;
	struct hfi_util_u32_prop_helper *base_props;
};

#define to_hfi_wb(x) container_of(x, struct hfi_wb_device, sde_wb_base)

int hfi_wb_display_prepare_commit(struct sde_wb_device *wb_dev,
		struct sde_connector_state *cstate);

#endif //  __HFI_WB_H__
