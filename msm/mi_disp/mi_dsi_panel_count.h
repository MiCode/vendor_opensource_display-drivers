/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#ifndef _MI_DSI_PANEL_COUNT_H_
#define _MI_DSI_PANEL_COUNT_H_

#include "dsi_panel.h"

typedef enum panel_count_event {
	PANEL_ACTIVE,
	PANEL_DOZE_ACTIVE,
	PANEL_FPS,
	PANEL_POWERON_COST,
	PANEL_ESD,
	PANEL_TE_LOST,
	UNDERRUN,
	OVERLOW,
	PINGPONG_TIMEOUT,
	COMMIT_LONG,
	CMDQ_TIMEOUT,
	PANEL_EVENT_MAX,
} PANEL_COUNT_EVENT;

enum PANEL_FPS {
	FPS_1,
	FPS_10,
	FPS_24,
	FPS_30,
	FPS_40,
	FPS_50,
	FPS_60,
	FPS_90,
	FPS_120,
	FPS_144,
	FPS_MAX_NUM,
};

struct mi_dsi_panel_count {
	/* display count */
	bool panel_active_count_enable;
	bool panel_doze_active_count_enable;
	u64 boottime;
	u64 bootRTCtime;
	u64 bootdays;
	u64 panel_active;
	u64 panel_daily_active;
	u64 panel_doze_active;
	u64 panel_doze_daily_active;
	u64 kickoff_count;
	u64 fps_times[FPS_MAX_NUM];
	u64 panel_id;
	u64 poweron_cost_avg;
	u64 esd_times;
	u64 te_lost_times;
	u64 underrun_times;
	u64 overflow_times;
	u64 pingpong_timeout_times;
	u64 commit_long_times;
	u64 cmdq_timeout_times;
};

void mi_dsi_panel_state_count(struct dsi_panel *panel, PANEL_COUNT_EVENT event, int value);
void mi_dsi_panel_active_count(struct dsi_panel *panel, int enable);
void mi_dsi_panel_doze_active_count(struct dsi_panel *panel, int enable);
void mi_dsi_panel_fps_count(struct dsi_panel *panel, u32 fps);
void mi_dsi_panel_esd_count(struct dsi_panel *panel, int is_irq);
void mi_dsi_panel_set_build_version(struct dsi_panel *panel, char * build_verison, u32 size);
void mi_dsi_panel_clean_data(void);
void mi_dsi_panel_power_on_cost_count(struct dsi_panel *panel, int is_start);
void mi_dsi_panel_te_lost_count(struct dsi_panel *panel, u32 fps);
void mi_dsi_panel_underrun_count(struct dsi_panel *panel, u32 fps);
void mi_dsi_panel_overflow_count(struct dsi_panel *panel, u32 fps);
void mi_dsi_panel_pingpong_timeout_count(struct dsi_panel *panel, u32 fps);
void mi_dsi_panel_commit_long_count(struct dsi_panel *panel, u32 value);
void mi_dsi_panel_count_init(struct dsi_panel *panel);

int mi_dsi_panel_set_disp_count(struct dsi_panel *panel, const char *buf);
ssize_t mi_dsi_panel_get_disp_count(struct dsi_panel *panel, char *buf, size_t size);

#endif /* _MI_DSI_PANEL_COUNT_H_ */
