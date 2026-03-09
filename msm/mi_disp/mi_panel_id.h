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
#define P2_42_02_0A_PANEL_ID   0x000050325042020A
#define P2_42_06_0A_PANEL_ID   0x000050325342060A
#define P2_42_0D_0B_PANEL_ID   0x0000503250420D0B
#define P3_42_02_0A_PANEL_ID   0x000050330042020A
#define Q200_42_02_0A_PANEL_ID 0x513230305042020A
#define Q200_42_06_0A_PANEL_ID 0x513230305342060A
#define P11U_42_0D_0A_PANEL_ID   0x5031315500420D0A
#define P1_42_0D_0B_PANEL_ID   0x0000503100420D0B

/* PA: Primary display, First selection screen
 * PB: Primary display, Second selection screen
 * SA: Secondary display, First selection screen
 * SB: Secondary display, Second selection screen
 */
enum mi_project_panel_id {
	PANEL_ID_INVALID = 0,
	P2_PANEL_PA,
	P2_PANEL_PB,
	P2_PANEL_SA,
	P3_PANEL_PA,
	Q200_PANEL_PA,
	Q200_PANEL_SA,
	P11U_PANEL_PA,
	P1_PANEL_PB,
	PANEL_ID_MAX
};

#define P2_PANEL_PA_PANEL_INFO     "mdss_dsi_p2_42_02_0a_prim_dsc_cmd"
#define P2_PANEL_PB_PANEL_INFO     "mdss_dsi_p2_42_0d_0b_prim_dsc_cmd"
#define P2_PANEL_SA_PANEL_INFO     "mdss_dsi_p2_42_06_0a_sec_dsc_cmd"
#define P3_PANEL_PA_PANEL_INFO     "mdss_dsi_p3_42_02_0a_dsc_vhm"
#define Q200_PANEL_PA_PANEL_INFO   "mdss_dsi_q200_42_02_0a_prim_dsc_cmd"
#define Q200_PANEL_SA_PANEL_INFO   "mdss_dsi_q200_42_06_0a_sec_dsc_cmd"
#define P11U_PANEL_PA_PANEL_INFO   "mdss_dsi_p11u_42_0d_0a_dsc_cmd"
#define P1_PANEL_PB_PANEL_INFO     "mdss_dsi_p1_42_0d_0b_prim_dsc_cmd"

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

enum mi_project_panel_id mi_get_panel_id_by_dsi_panel(struct dsi_panel *panel);
enum mi_project_panel_id mi_get_panel_id_by_disp_id(int disp_id);

static inline enum mi_panel_build_id mi_get_panel_build_id(u64 mi_panel_id, u64 build_id)
{
	switch(mi_panel_id) {
	case P2_42_02_0A_PANEL_ID:
		switch (build_id) {
			case 0x10:
				return PANEL_P01;
			case 0x40:
				return PANEL_P10;
			case 0x41:
				return PANEL_P10;
			case 0x51:
				return PANEL_P11;
			case 0x61:
				return PANEL_P12;
			case 0x80:
				return PANEL_P20;
			case 0x81:
				return PANEL_P20;
			case 0x82:
				return PANEL_MP;
			default:
				return PANEL_MAX;
		}
		return build_id;
	case P2_42_06_0A_PANEL_ID:
		switch (build_id) {
			case 0x10:
				return PANEL_P01;
			case 0x40:
				return PANEL_P10;
			case 0x50:
				return PANEL_P11;
			default:
				return PANEL_MAX;
		}
		return build_id;
	case P2_42_0D_0B_PANEL_ID:
		switch (build_id) {
			case 0x00:
				return PANEL_P11;
			case 0x40:
				return PANEL_P12;
			case 0x80:
				return PANEL_P20;
			case 0x82:
				return PANEL_MP;
			case 0xC0:
				return PANEL_MP;
			default:
				return PANEL_MAX;
		}
		return build_id;
	case P3_42_02_0A_PANEL_ID:
		switch (build_id) {
			case 0x10:
				return PANEL_P01;
			case 0x40:
				return PANEL_P10;
			case 0x50:
				return PANEL_P11;
			case 0x60:
			case 0x70:
				return PANEL_P12;
			case 0x80:
				return PANEL_P20;
			case 0x90:
				return PANEL_P21;
			case 0xC0:
				return PANEL_MP;
			default:
				return PANEL_MAX;
		}
		return build_id;
	case Q200_42_02_0A_PANEL_ID:
		switch (build_id) {
			case 0x10:
				return PANEL_P01;
			case 0x40:
				return PANEL_P10;
			case 0x50:
				return PANEL_P11;
			case 0x60:
			case 0x70:
				return PANEL_P12;
			case 0x80:
			case 0x90:
				return PANEL_P20;
			default:
				return PANEL_MAX;
		}
		return build_id;
	case Q200_42_06_0A_PANEL_ID:
		return build_id;
	case P11U_42_0D_0A_PANEL_ID:
		return build_id;
	case P1_42_0D_0B_PANEL_ID:
		switch (build_id) {
			case 0x00:
				return PANEL_P01;
			default:
				return PANEL_MAX;
		}
		return build_id;
	default:
		return PANEL_MAX;
	}
}

static inline enum mi_project_panel_id mi_get_panel_id(u64 mi_panel_id)
{
	switch(mi_panel_id) {
	case P2_42_02_0A_PANEL_ID:
		return P2_PANEL_PA;
	case P2_42_06_0A_PANEL_ID:
		return P2_PANEL_SA;
	case P2_42_0D_0B_PANEL_ID:
		return P2_PANEL_PB;
	case P3_42_02_0A_PANEL_ID:
		return P3_PANEL_PA;
	case Q200_42_02_0A_PANEL_ID:
		return Q200_PANEL_PA;
	case Q200_42_06_0A_PANEL_ID:
		return Q200_PANEL_SA;
	case P11U_42_0D_0A_PANEL_ID:
		return P11U_PANEL_PA;
	case P1_42_0D_0B_PANEL_ID:
		return P1_PANEL_PB;
	default:
		return PANEL_ID_INVALID;
	}
}

static inline const char *mi_get_panel_id_name(u64 mi_panel_id)
{
	switch (mi_get_panel_id(mi_panel_id)) {
	case P2_PANEL_PA:
		return "P2_PANEL_PA";
	case P2_PANEL_SA:
		return "P2_PANEL_SA";
	case P2_PANEL_PB:
		return "P2_PANEL_PB";
	case P3_PANEL_PA:
		return "P3_PANEL_PA";
	case Q200_PANEL_PA:
		return "Q200_PANEL_PA";
	case Q200_PANEL_SA:
		return "Q200_PANEL_SA";
	case P11U_PANEL_PA:
		return "P11U_PANEL_PA";
	case P1_PANEL_PB:
		return "P1_PANEL_PB";
	default:
		return "unknown";
	}
}

static inline const char *mi_get_panel_info(struct dsi_panel *panel)
{
	switch (mi_get_panel_id_by_dsi_panel(panel)) {
	case P2_PANEL_PA:
		return P2_PANEL_PA_PANEL_INFO;
	case P2_PANEL_PB:
		return P2_PANEL_PB_PANEL_INFO;
	case P2_PANEL_SA:
		return P2_PANEL_SA_PANEL_INFO;
	case P3_PANEL_PA:
		return P3_PANEL_PA_PANEL_INFO;
	case Q200_PANEL_PA:
		return Q200_PANEL_PA_PANEL_INFO;
	case Q200_PANEL_SA:
		return Q200_PANEL_SA_PANEL_INFO;
	case P11U_PANEL_PA:
		return P11U_PANEL_PA_PANEL_INFO;
	case P1_PANEL_PB:
		return P1_PANEL_PB_PANEL_INFO;
	default:
		return NULL;
	}
}

static inline bool is_use_nt37707a_dsc_config_apspr(u64 mi_panel_id)
{
	switch(mi_panel_id) {
	case P3_42_02_0A_PANEL_ID:
	case Q200_42_02_0A_PANEL_ID:
		return true;
	default:
		return false;
	}
}

static inline bool is_use_nt38773_dsc_config_apspr(u64 mi_panel_id)
{
	switch(mi_panel_id) {
	default:
		return false;
	}
}

static inline bool is_use_nvt_dsc_config(u64 mi_panel_id)
{
	switch(mi_panel_id) {
	case P2_42_02_0A_PANEL_ID:
		return true;
	default:
		return false;
	}
}

static inline bool is_use_nt37707a_dsc_config(u64 mi_panel_id)
{
	switch(mi_panel_id) {
	default:
		return false;
	}
}


#endif /* _MI_PANEL_ID_H_ */
