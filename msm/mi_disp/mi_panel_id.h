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
   1Byte : Display screen supplier ID (0x0A/0x0B/0x0C ...)
*/
#define O1_42_02_0A_PANEL_ID   0x00004F310042020A
#define O1_38_0C_0B_PANEL_ID   0x00004F3100380C0B
#define O2_42_02_0A_PANEL_ID   0x00004F320042020A
#define O3_42_0D_0A_PANEL_ID   0x00004F3300420D0A
#define O3_36_02_0B_PANEL_ID   0x00004F330036020B
#define O11U_42_02_0A_PANEL_ID 0x4F3131550042020A
#define O10U_42_02_0A_PANEL_ID    0x4F3130550042020A
#define O10U_36_02_0B_PANEL_ID    0x4F3130550036020B
/* PA: Primary display, First selection screen
 * PB: Primary display, Second selection screen
 * SA: Secondary display, First selection screen
 * SB: Secondary display, Second selection screen
 */
enum mi_project_panel_id {
	PANEL_ID_INVALID = 0,
	O1_PANEL_PA,
	O1_PANEL_PB,
	O2_PANEL_PA,
	O3_PANEL_PA,
	O3_PANEL_PB,
	O11U_PANEL_PA,
	O10U_PANEL_PA,
	O10U_PANEL_PB,
	PANEL_ID_MAX
};

enum mi_panel_build_id {
	PANEL_P01 = 0x01,
	PANEL_P10 = 0x10,
	PANEL_P11 = 0x11,
	PANEL_P12 = 0x12,
	PANEL_P20 = 0x20,
	PANEL_P21 = 0x21,
	PANEL_MP = 0xAA,
	PANEL_MAX =0xFF
};

static inline enum mi_panel_build_id mi_get_panel_build_id(u64 mi_panel_id, u64 build_id)
{
	switch(mi_panel_id) {
	case O1_42_02_0A_PANEL_ID:
	case O1_38_0C_0B_PANEL_ID:
	case O2_42_02_0A_PANEL_ID:
	case O10U_42_02_0A_PANEL_ID:
	case O10U_36_02_0B_PANEL_ID:
		return build_id;
	case O3_42_0D_0A_PANEL_ID:
		switch (build_id) {
		case 0x10:
			 return PANEL_P01;
		case 0x40:
			return PANEL_P10;
		case 0x50:
			return PANEL_P11;
		case 0x80:
			return PANEL_P20;
		default:
			return PANEL_MP;
		}
		return PANEL_MP;
	case O3_36_02_0B_PANEL_ID:
		switch (build_id) {
		case 0x11:
			 return PANEL_P01;
		case 0x42:
			 return PANEL_P10;
		default:
			return PANEL_MP;
		}
		return PANEL_MP;
	case O11U_42_02_0A_PANEL_ID:
		switch (build_id) {
		case 0x40:
			return PANEL_P10;
		case 0x50:
			return PANEL_P11;
		case 0x80:
			return PANEL_P20;
		case 0x90:
			return PANEL_P21;
		default:
			return PANEL_MP;
		}
	default:
		return PANEL_MAX;
	}
}

static inline enum mi_project_panel_id mi_get_panel_id(u64 mi_panel_id)
{
	switch(mi_panel_id) {
	case O1_42_02_0A_PANEL_ID:
		return O1_PANEL_PA;
	case O1_38_0C_0B_PANEL_ID:
		return O1_PANEL_PB;
	case O2_42_02_0A_PANEL_ID:
		return O2_PANEL_PA;
	case O3_42_0D_0A_PANEL_ID:
		return O3_PANEL_PA;
	case O3_36_02_0B_PANEL_ID:
		return O3_PANEL_PB;
	case O11U_42_02_0A_PANEL_ID:
		return O11U_PANEL_PA;
	case O10U_42_02_0A_PANEL_ID:
		return O10U_PANEL_PA;
	case O10U_36_02_0B_PANEL_ID:
		return O10U_PANEL_PB;
	default:
		return PANEL_ID_INVALID;
	}
}

static inline const char *mi_get_panel_id_name(u64 mi_panel_id)
{
	switch (mi_get_panel_id(mi_panel_id)) {
	case O1_PANEL_PA:
		return "O1_PANEL_PA";
	case O1_PANEL_PB:
		return "O1_PANEL_PB";
	case O2_PANEL_PA:
		return "O2_PANEL_PA";
	case O3_PANEL_PA:
		return "O3_PANEL_PA";
	case O3_PANEL_PB:
		return "O3_PANEL_PB";
	case O11U_PANEL_PA:
		return "O11U_PANEL_PA";
	case O10U_PANEL_PA:
		return "O10U_PANEL_PA";
	case O10U_PANEL_PB:
		return "O10U_PANEL_PB";
	default:
		return "unknown";
	}
}

static inline bool is_use_nt37802_dsc_config(u64 mi_panel_id)
{
	switch(mi_panel_id) {
	case O2_42_02_0A_PANEL_ID:
		return true;
	default:
		return false;
	}
}

static inline bool is_use_nt37802_dsc_config_o11u(u64 mi_panel_id)
{
	switch(mi_panel_id) {
	case O1_42_02_0A_PANEL_ID:
		return true;
	case O11U_42_02_0A_PANEL_ID:
		return true;
	default:
		return false;
	}
}

static inline bool is_use_nvt_dsc_config(u64 mi_panel_id)
{
	switch(mi_panel_id) {
	case O10U_42_02_0A_PANEL_ID:
	case O10U_36_02_0B_PANEL_ID:
		return true;
	default:
		return false;
	}
}

enum mi_project_panel_id mi_get_panel_id_by_dsi_panel(struct dsi_panel *panel);
enum mi_project_panel_id mi_get_panel_id_by_disp_id(int disp_id);

#endif /* _MI_PANEL_ID_H_ */
