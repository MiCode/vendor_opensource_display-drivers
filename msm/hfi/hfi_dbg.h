/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef _HFI_DBG_H_
#define _HFI_DBG_H_

#include "sde_dbg.h"
#include "sde_trace.h"
#include "hfi_kms.h"
#include "hfi_adapter.h"

#if IS_ENABLED(CONFIG_MDSS_HFI)
/**
 * struct hfi_dbg_buf_data -  hfi debug buffer data base structure
 *  @reg_addr: pointer to MDSS registers dump address
 *  @evt_log_addr: pointer to event log  dump address
 *  @dbg_bus_addr: pointer to debug bus dump address
 *  @device_state_addr: pointer to device state variable dump address
 */
struct hfi_dbg_buf_data {
	struct hfi_shared_addr_map reg_addr;
	struct hfi_shared_addr_map evt_log_addr;
	struct hfi_shared_addr_map dbg_bus_addr;
	struct hfi_shared_addr_map device_state_addr;
};

/**
 * struct hfi_dbg_reg_base - register region base.
 * @off: cached offset of region for manual register dumping
 * @cnt: cached range of region for manual register dumping
 * @reg_addr: pointer to MDSS registers dump address
 */
struct hfi_dbg_reg_base {
	size_t off;
	size_t cnt;
	struct hfi_shared_addr_map *reg_addr;
};

/**
 * struct hfi_dbg - hfi implementation extension of sde_encoder object
 * @base:	base pointer of sde debug
 * @dev: device pointer
 * @debugfs_root:        Root for debugfs entries.
 * @hfi_dbg_printer: drm printer handle used to print hfi_dbg info in devcoredump device
 * @buff_map: stores buffer pointers mapped for reg, event log, debug bus and device
 *            state dumps
 * @base_props: prop helper object for intermediate property collection
 * @hfi_cb_obj: hfi listener call back object
 * @hfi_cb_obj: register read call back object
 * @reg_base: stores register read user configuration\
 * @dump_len: to track the length of regs in flight of dump
 * @base_buf_addr: stores full buffer pointer mapped for debug events
 */
struct hfi_dbg {
	struct sde_dbg_base *base;
	struct device *dev;
	struct dentry *debugfs_root;

	struct hfi_dbg_buf_data buff_map;
	struct hfi_util_u32_prop_helper *base_props;

	struct hfi_prop_listener hfi_cb_obj;
	struct hfi_prop_listener reg_read_cb_obj;
	struct hfi_dbg_reg_base *reg_base;
	struct hfi_shared_addr_map base_buf_addr;
};

/**
 * hfi_dbg_init - initialize hfi debug object
 * @dev:     Pointer to drm device structure
 * Returns:  Pointers to shared memory structure
 */
int hfi_dbg_init(struct device *dev, struct sde_dbg_base *dbg);

/**
 * hfi_dbg_destroy - destroy hfi debug object
 */
void hfi_dbg_destroy(void);
#else

static inline int hfi_dbg_init(struct device *dev, struct sde_dbg_base *dbg)
{
	return 0;
}

static inline void hfi_dbg_destroy(void)
{

}

#endif // IS_ENABLED(CONFIG_MDSS_HFI)

#endif  // _HFI_DBG_H_
