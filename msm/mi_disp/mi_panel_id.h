/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#ifndef _MI_PANEL_ID_H_
#define _MI_PANEL_ID_H_

#include <linux/types.h>
#include "dsi_panel.h"

/*
   Naming Rules,Wiki Dec .
   4Byte : Project ASCII Value .Exemple "L18" ASCII Is 004C3138
   1Byte : Prim Panel Is 'P' ASCII Value , Sec Panel Is 'S' Value
   1Byte : Panel Vendor
   1Byte : DDIC Vendor ,Samsung 0x0C Novatek 0x02
   1Byte : Production Batch Num
*/
#define O2_42_02_0A_PANEL_ID   0x00004E3200420200
#define P17_41_02_0A_PANEL_ID  0x0050313700410200
#define P17_35_0F_0B_PANEL_ID  0x0050313700350F00
#define P17_41_0F_0C_PANEL_ID  0x0050313700410F00

/* PA: Primary display, First selection screen
 * PB: Primary display, Second selection screen
 * SA: Secondary display, First selection screen
 * SB: Secondary display, Second selection screen
 */
enum mi_project_panel_id {
	PANEL_ID_INVALID = 0,
	O2_PANEL_PA,
	P17_PANEL_PA,
	PANEL_ID_MAX
};

static inline enum mi_project_panel_id mi_get_panel_id(u64 mi_panel_id)
{
	switch(mi_panel_id) {
	case O2_42_02_0A_PANEL_ID:
		return O2_PANEL_PA;
	case P17_41_02_0A_PANEL_ID:
		return P17_PANEL_PA;
	case P17_35_0F_0B_PANEL_ID:
		return P17_PANEL_PA;
	case P17_41_0F_0C_PANEL_ID:
		return P17_PANEL_PA;
	default:
		return PANEL_ID_INVALID;
	}
}

static inline const char *mi_get_panel_id_name(u64 mi_panel_id)
{
	switch (mi_get_panel_id(mi_panel_id)) {
	case O2_PANEL_PA:
		return "O2_PANEL_PA";
	case P17_PANEL_PA:
		return "P17_PANEL_PA";
	default:
		return "unknown";
	}
}

static inline bool is_use_nt37801_dsc_config(u64 mi_panel_id)
{
	switch(mi_panel_id) {
	case O2_42_02_0A_PANEL_ID:
		return true;
	default:
		return false;
	}
}

enum mi_project_panel_id mi_get_panel_id_by_dsi_panel(struct dsi_panel *panel);
enum mi_project_panel_id mi_get_panel_id_by_disp_id(int disp_id);

#endif /* _MI_PANEL_ID_H_ */
