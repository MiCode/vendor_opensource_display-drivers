/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#ifndef __MI_KERNEL_TIMER_H__
#define __MI_KERNEL_TIMER_H__

#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/rtc.h>

#include "sde_connector.h"
#include "sde_encoder_phys.h"
#include "dsi_panel.h"

enum func_type{
	Complete_commit = 0,
	Crtc_flush,
	Plane_wait_input_fence,
	Crtc_commit,
	Encoder_commit,
	Wait_for_commit_done,
	FUNCTION_NUM,
};

enum kernel_qcom_idx {
	MI_RECODER_IDX_CTLDONE,
	MI_RECODER_IDX_RDPTR,
	MI_RECODER_IDX_WRPTR,
	MI_RECODEER_IDX_MAX,
};
enum commit_type {
	Refresh = 0,
	Mode_change,
	Fps_change,
};


struct mi_kernel_recoder{
	struct timer_list tm;
	struct timespec64 tv_start[FUNCTION_NUM];
	struct timespec64 tv_end[FUNCTION_NUM];
	void (*timer_callback)(struct timer_list *arg);
	struct drm_crtc *crtc_ponit;
	int fps;
	int outtimer_ms;
	int drop_fram_time;
	u32 end_flag;
	u32 start_flag;
	u32 sde_intr_idx_flag;
	enum commit_type type;
	int mode;
	bool timer_setup_flag;
};
struct mi_kernel_recoder_set{
	struct mi_kernel_recoder * recoder_list;
	int crtc_num;
	bool success_init;
	struct spinlock framdrop_lock;
	struct spinlock deadapear_lock;
	bool recoder_support;
};


void callback(struct timer_list *arg);

int mi_kernel_recoder_init(int crtc_num, struct msm_drm_private *priv) ;
int mi_kernel_recoder_function_start(struct drm_atomic_state *state, enum func_type FUN_ENUM) ;
int mi_kernel_recoder_function_end(struct drm_atomic_state *state, enum func_type FUN_ENUM) ;
int mi_kernel_recoder_function_start_by_crtc(struct drm_crtc *crtc, enum func_type FUN_ENUM) ;
int mi_kernel_recoder_function_end_by_crtc(struct drm_crtc *crtc, enum func_type FUN_ENUM) ;




void mi_kernel_recoder_is_drop_frame(struct drm_atomic_state *state) ;
void mi_kernel_recoder_drop_frame_dump_by_index(int recoder_index) ;
void mi_kernel_recoder_dead_apear_dump(struct mi_kernel_recoder *recoder) ;
int mi_kernel_recoder_irq_done(int crtc_id, enum kernel_qcom_idx irq_idx) ;


int mi_kernel_timer_setup(struct drm_atomic_state *state) ;
int mi_kernel_timer_start(struct drm_atomic_state *state) ;
int mi_kernel_timer_destory(struct drm_atomic_state *state) ;
int mi_kernel_timer_change_fps(int fps, struct drm_crtc *crtc, enum commit_type curent_commit_type, int newmode) ;
void mi_kernel_recoder_destroy(void) ;
int mi_kernel_set_recoder_suport(bool flag) ;







#endif /* _MI_DISP_CONFIG_H_ */
