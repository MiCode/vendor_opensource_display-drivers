/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/soc/qcom/panel_event_notifier.h>


#include "mi_disp_print.h"
#include "mi_dsi_display.h"
#include "mi_dsi_panel.h"
#include "mi_kernel_timer.h"
#include "mi_disp_log.h"
#include "sde_trace.h"


struct mi_kernel_recoder_set *g_kernel_timer_set = NULL;

#define Get_recoder_time_start(__index, __i) \
	g_kernel_timer_set->recoder_list[__index].tv_start[__i]

#define Get_recoder_time_end(__index, __i) \
	g_kernel_timer_set->recoder_list[__index].tv_end[__i]

#define Get_recoder_member(__index, menmber_name) \
	g_kernel_timer_set->recoder_list[__index].menmber_name

#define Get_recoder(__index) \
	g_kernel_timer_set->recoder_list[__index]



const char *function_map[FUNCTION_NUM] = {
	"Complete_commit",
	"Crtc_flush",
	"Plane_wait_input_fence",
	"Crtc_commit",
	"Encoder_commit",
	"Wait_for_commit_done",
};

const char *sde_intr_idx_map[MI_RECODEER_IDX_MAX] = {
	"MI_RECODER_IDX_CTLDONE",
	"MI_RECODER_IDX_RDPTR",
	"MI_RECODER_IDX_WRPTR",
};


void callback(struct timer_list *arg)
{
	struct mi_kernel_recoder *recoder = container_of(arg, struct mi_kernel_recoder, tm);
	if(!recoder) {
		DISP_ERROR("invalid params\n");
	}
	mi_kernel_recoder_dead_apear_dump(recoder);
}
int mi_kernel_recoder_init(int crtc_num, struct msm_drm_private *priv)
{
	int ret = 0 ;
	int i;
	if (g_kernel_timer_set) {
		DISP_INFO("kernel_timer_set init\n");
		goto end;
	}
	g_kernel_timer_set = kzalloc(sizeof(struct mi_kernel_recoder_set), GFP_KERNEL);
	if (!g_kernel_timer_set) {
		DISP_ERROR("g_kernel_timer_set kzalloc fail\n");
		ret = -EFAULT;
		goto end;
	}
	g_kernel_timer_set->recoder_support = false;
	g_kernel_timer_set->crtc_num = crtc_num;
	g_kernel_timer_set->recoder_list = kcalloc(crtc_num, sizeof(struct mi_kernel_recoder), GFP_KERNEL);
	if (!g_kernel_timer_set->recoder_list) {
		DISP_ERROR(" recoder_list kcalloc fail\n");
		g_kernel_timer_set->success_init = false;
		ret = -EFAULT;
		goto error;
	}
	g_kernel_timer_set->success_init = true;
	for (i= 0; i < crtc_num; ++i) {
		Get_recoder_member(i, timer_callback) = callback;
		Get_recoder_member(i, crtc_ponit) = priv->crtcs[i];
		Get_recoder_member(i, outtimer_ms) = 100;
	}
	spin_lock_init(&g_kernel_timer_set->framdrop_lock);
	spin_lock_init(&g_kernel_timer_set->deadapear_lock);
	goto end;
error:
	kfree(g_kernel_timer_set);
end:
	return ret;
}
int mi_kernel_recoder_function_start(struct drm_atomic_state *state, enum func_type FUN_ENUM) 
{
	int ret = 0;
	struct drm_crtc *crtc =  NULL;
	struct drm_crtc_state *crtc_state = NULL;
	int i = 0, j = 0;
	if (!g_kernel_timer_set || !g_kernel_timer_set->success_init) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	if (!g_kernel_timer_set->recoder_support) {
		return ret;
	}
	for_each_old_crtc_in_state(state, crtc, crtc_state, i) {
		for (j = 0; j < g_kernel_timer_set->crtc_num; ++j) {
			if (Get_recoder_member(j, crtc_ponit) && Get_recoder_member(j, crtc_ponit)->base.id == crtc->base.id) {
				ktime_get_real_ts64(&Get_recoder_time_start(j, FUN_ENUM));
				Get_recoder_member(j, start_flag) = Get_recoder_member(j, start_flag) | 1 << FUN_ENUM;
			}
		}
	}
	return ret;

}
int mi_kernel_recoder_function_end(struct drm_atomic_state *state, enum func_type FUN_ENUM)
{
	int ret = 0;
	struct drm_crtc *crtc = NULL;
	struct drm_crtc_state *crtc_state = NULL;
	int i = 0, j = 0;
	if (!g_kernel_timer_set || !g_kernel_timer_set->success_init) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	if (!g_kernel_timer_set->recoder_support) {
		return ret;
	}
	for_each_old_crtc_in_state(state, crtc, crtc_state, i) {
		for (j = 0; j < g_kernel_timer_set->crtc_num; ++j) {
			if (Get_recoder_member(j, crtc_ponit) && Get_recoder_member(j, crtc_ponit)->base.id == crtc->base.id) {
				ktime_get_real_ts64(&Get_recoder_time_end(j, FUN_ENUM));
 				Get_recoder_member(j, end_flag) = Get_recoder_member(j, end_flag) | 1 << FUN_ENUM;
			}
		}
	}
	return ret;
}
int mi_kernel_recoder_function_start_by_crtc(struct drm_crtc *crtc, enum func_type FUN_ENUM) 
{
	int ret = 0;
	int i = 0;
	if (!g_kernel_timer_set || !g_kernel_timer_set->success_init || !crtc) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	if (!g_kernel_timer_set->recoder_support) {
		return ret;
	}
	for (i=0; i < g_kernel_timer_set->crtc_num; ++i) {
		if (Get_recoder_member(i, crtc_ponit) && Get_recoder_member(i, crtc_ponit)->base.id == crtc->base.id) {
			ktime_get_real_ts64(&Get_recoder_time_start(i, FUN_ENUM));
			Get_recoder_member(i, start_flag) = Get_recoder_member(i, start_flag) | 1 << FUN_ENUM;
		}
	}
	return ret;
}
int mi_kernel_recoder_function_end_by_crtc(struct drm_crtc *crtc, enum func_type FUN_ENUM)
{
	int ret = 0;
	int i = 0;
	if (!g_kernel_timer_set || !g_kernel_timer_set->success_init || !crtc) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	if (!g_kernel_timer_set->recoder_support) {
		return ret;
	}
	for (i=0; i < g_kernel_timer_set->crtc_num; ++i) {
		if (Get_recoder_member(i, crtc_ponit) && Get_recoder_member(i, crtc_ponit)->base.id == crtc->base.id) {
			ktime_get_real_ts64(&Get_recoder_time_end(i, FUN_ENUM));
			Get_recoder_member(i, end_flag) = Get_recoder_member(i, end_flag) | 1 << FUN_ENUM;
		}
	}
	return ret;
}
int mi_kernel_recoder_irq_done(int crtc_id, enum kernel_qcom_idx irq_idx)
{
	int ret = 0,i = 0;

	if (!g_kernel_timer_set || !g_kernel_timer_set->success_init) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	if (!g_kernel_timer_set->recoder_support) {
		return ret;
	}
	for (i=0; i < g_kernel_timer_set->crtc_num; ++i) {
		if (Get_recoder_member(i, crtc_ponit) && Get_recoder_member(i, crtc_ponit)->base.id == crtc_id) {
			Get_recoder_member(i,sde_intr_idx_flag) = Get_recoder_member(i,sde_intr_idx_flag) | 1 << irq_idx;
		}
	}
	return ret;
}

void mi_kernel_recoder_is_drop_frame(struct drm_atomic_state *state)
{
	struct timespec64 ts_delta;
	struct drm_crtc *crtc = 0;
	struct drm_crtc_state *crtc_state = NULL;
	int i = 0, j = 0;
	if (!g_kernel_timer_set || !g_kernel_timer_set->success_init) {
		DISP_ERROR("invalid params\n");
		return ;
	}
	if (!g_kernel_timer_set->recoder_support) {
		return ;
	}
	for_each_old_crtc_in_state(state, crtc, crtc_state, i) {
		for (j = 0; j < g_kernel_timer_set->crtc_num; ++j) {
			if (Get_recoder_member(j, crtc_ponit) && Get_recoder_member(j, crtc_ponit)->base.id == crtc->base.id &&
				Get_recoder_member(j, timer_setup_flag)) {
				ts_delta = timespec64_sub(Get_recoder_time_end(j, Complete_commit),
					Get_recoder_time_start(j, Complete_commit));
				if(Get_recoder_member(j, type) == Refresh &&
					timespec64_to_ns(&ts_delta) > Get_recoder_member(j, drop_fram_time)) {
					mi_kernel_recoder_drop_frame_dump_by_index(j);
				}
 			}
	    }
	}

}

void mi_kernel_recoder_drop_frame_dump_by_index(int recoder_index)
{
	struct timespec64 ts_delta;
	static char buf[1024];
	int buf_len = 0;
	int j = 0;
	unsigned long flags;
	if (!g_kernel_timer_set || !g_kernel_timer_set->success_init || !Get_recoder_member(recoder_index, crtc_ponit)) {
		DISP_ERROR("invalid params\n");
		return ;
	}
	if (Get_recoder_member(recoder_index, mode) == DRM_PANEL_EVENT_BLANK_LP)
		return;
	spin_lock_irqsave(&g_kernel_timer_set->framdrop_lock,flags);
	memset(buf, 0, sizeof(buf));
	buf_len = snprintf(buf, sizeof(buf), "Fram drop: crtc:%d,fps:%d\n",
		Get_recoder_member(recoder_index, crtc_ponit)->base.id, Get_recoder_member(recoder_index, fps));
	for (j = 0; j < FUNCTION_NUM; ++j) {
		ts_delta = timespec64_sub(Get_recoder_time_end(recoder_index, j),
			Get_recoder_time_start(recoder_index, j));
		buf_len += snprintf(buf + buf_len, sizeof(buf) - buf_len, "%40s:%15d us\n",
			function_map[j], (int)(timespec64_to_ns(&ts_delta) /1000));
	}
	DISPLOG_TIME("%s",buf);
	spin_unlock_irqrestore(&g_kernel_timer_set->framdrop_lock, flags);

}
void mi_kernel_recoder_dead_apear_dump(struct mi_kernel_recoder *recoder)
{
	int i = 0;
	static char buf[1024];
	int buf_len = 0;
	unsigned long flags;
	if (!recoder || !g_kernel_timer_set || !g_kernel_timer_set->success_init || !recoder->crtc_ponit) {
		DISP_ERROR("invalid params\n");
		return ;
	}
	if (recoder->mode == DRM_PANEL_EVENT_BLANK_LP)
		return;
	spin_lock_irqsave(&g_kernel_timer_set->deadapear_lock,flags);
	memset(buf, 0, sizeof(buf));
	buf_len = snprintf(buf, sizeof(buf), "Fram timeout: crtc:%d,fps:%d\n",
		recoder->crtc_ponit->base.id, recoder->fps);
	for (i = 0; i < FUNCTION_NUM; ++i) {
		buf_len += snprintf(buf + buf_len, sizeof(buf) - buf_len, "%40s: start_flag %d,end_flag %d\n",
			function_map[i], (recoder->start_flag & (1<<i))? 1 : 0, (recoder->end_flag & (1<<i))? 1 : 0);
	}
	for (i = 0; i < MI_RECODEER_IDX_MAX; ++i) {
		buf_len += snprintf(buf + buf_len, sizeof(buf) - buf_len, "%40s: irq_flag %d\n", sde_intr_idx_map[i],
			 (recoder->sde_intr_idx_flag & (1<<i))? 1 : 0);

	}
	DISPLOG_TIME("%s",buf);
	spin_unlock_irqrestore(&g_kernel_timer_set->deadapear_lock, flags);

}

int mi_kernel_timer_setup(struct drm_atomic_state *state)
{
	struct drm_crtc *crtc = NULL;
	struct drm_crtc_state *crtc_state = NULL;
	int i = 0,j = 0,ret = 0;

	if (!g_kernel_timer_set || !g_kernel_timer_set->success_init) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	if (!g_kernel_timer_set->recoder_support) {
		return ret;
	}
	for_each_old_crtc_in_state(state, crtc, crtc_state, i) {
		for (j = 0; j < g_kernel_timer_set->crtc_num; ++j) {
			if (Get_recoder_member(j, crtc_ponit) && Get_recoder_member(j, crtc_ponit)->base.id == crtc->base.id) {
				timer_setup(&(Get_recoder_member(j, tm)), Get_recoder_member(j, timer_callback), 0);
				Get_recoder_member(j, timer_setup_flag) = true;
				Get_recoder_member(j, tm).expires = jiffies +
					msecs_to_jiffies(Get_recoder_member(j, outtimer_ms));
				add_timer(&(Get_recoder_member(j, tm)));
				Get_recoder_member(j, start_flag) = 0;
				Get_recoder_member(j, end_flag)  = 0;
				Get_recoder_member(j, sde_intr_idx_flag)  = 0;
				Get_recoder_member(j, type) = Refresh;
	        }
	    }
	}
    return ret;

}
int mi_kernel_timer_destory(struct drm_atomic_state *state) {
	int ret = 0;
	struct drm_crtc *crtc = NULL;
	struct drm_crtc_state *crtc_state = NULL;
	int i = 0, j = 0;
	if (!g_kernel_timer_set || !g_kernel_timer_set->success_init) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	if (!g_kernel_timer_set->recoder_support) {
		return ret;
	}
	for_each_old_crtc_in_state(state, crtc, crtc_state, i) {
		for (j = 0; j < g_kernel_timer_set->crtc_num; ++j) {
			if (Get_recoder_member(j, crtc_ponit) && Get_recoder_member(j, crtc_ponit)->base.id == crtc->base.id) {
				if (Get_recoder_member(j, timer_setup_flag)) {
					ret |= del_timer(&(Get_recoder_member(j, tm)));
					Get_recoder_member(j, timer_setup_flag) = false;
				}
	        }
	    }
	}
	return ret;
}

int mi_kernel_timer_change_fps(int fps, struct drm_crtc *crtc,enum commit_type current_commit_type, int newmode) {
	int i = 0, ret = 0;
	if (!g_kernel_timer_set || !g_kernel_timer_set->success_init || !crtc) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	for (i = 0; i < g_kernel_timer_set->crtc_num; ++i) {
		if (Get_recoder_member(i, crtc_ponit) && Get_recoder_member(i, crtc_ponit)->base.id == crtc->base.id ) {
			if (Get_recoder_member(i, fps) != fps) {
				Get_recoder_member(i, fps) = fps;
				Get_recoder_member(i, drop_fram_time) = (1000/fps + 1)*1000000;
				Get_recoder_member(i, outtimer_ms) = Get_recoder_member(i, drop_fram_time) /1000000 * 3;
			}
			if (Get_recoder_member(i, timer_setup_flag)) {
				ret = del_timer(&(Get_recoder_member(i, tm)));
				Get_recoder_member(i, timer_setup_flag) = false;
			}
			Get_recoder_member(i, type) = current_commit_type;
			if (current_commit_type == Mode_change) {
				Get_recoder_member(i, mode) = newmode;
                        }
		}
	}
	return ret;
}

void mi_kernel_recoder_destroy(void)
{
	if (!g_kernel_timer_set) {
		DISP_ERROR("invalid params\n");
		return ;
	}
	if (!g_kernel_timer_set->success_init)
	{
		DISP_ERROR("success_init invalid params\n");
		kfree(g_kernel_timer_set);
		return;
	}
	kfree(g_kernel_timer_set->recoder_list);
	kfree(g_kernel_timer_set);
	return;

}

int mi_kernel_set_recoder_suport(bool flag) {
	int i = 0, ret = 0;
	if (!g_kernel_timer_set || !g_kernel_timer_set->success_init) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}
	g_kernel_timer_set->recoder_support = flag;
	if (!flag) {
		for (i = 0; i < g_kernel_timer_set->crtc_num; ++i) {
			if (Get_recoder_member(i, timer_setup_flag)) {
				del_timer(&(Get_recoder_member(i, tm)));
				Get_recoder_member(i, timer_setup_flag) = false;
			}
		}
	}
	return ret;
}



