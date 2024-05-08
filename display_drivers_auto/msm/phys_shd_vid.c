// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[drm-shd:%s:%d] " fmt, __func__, __LINE__

#include <linux/debugfs.h>
#include <drm/sde_drm.h>

#include "sde_encoder_phys.h"
#include "sde_formats.h"
#include "sde_hw_top.h"
#include "sde_hw_interrupts.h"
#include "sde_hw_dsc.h"
#include "sde_core_irq.h"
#include "sde_crtc.h"
#include "sde_trace.h"
#include "sde_plane.h"
#include "shd_drm.h"
#include "shd_hw.h"

#define SDE_ERROR_PHYS(phy_enc, fmt, ...) SDE_ERROR("enc%d intf%d " fmt,\
		(phy_enc) ? (phy_enc)->parent->base.id : -1, \
		(phy_enc) ? (phy_enc)->intf_idx - INTF_0 : -1, \
	##__VA_ARGS__)
/**
 * struct sde_encoder_phys_shd - sub-class of sde_encoder_phys to handle shared
 *	mode specific operations
 * @base:	Baseclass physical encoder structure
 * @obj: DRM private state object
 */
struct sde_encoder_phys_shd {
	struct sde_encoder_phys base;
	struct drm_private_obj obj;
};

#define to_sde_encoder_phys_shd(x) \
	container_of(x, struct sde_encoder_phys_shd, base)

/**
 * struct sde_enc_shd_state - SDE dynamic hardware resource manager state
 * @base: private state base
 * @shd_enc:	Shared encoder handle
 * @hw_lm:	HW LM blocks created by this shared encoder
 * @hw_ctl:	HW CTL blocks created by this shared encoder
 * @num_mixers:	Number of LM blocks
 * @num_ctls:	Number of CTL blocks
 */
struct sde_enc_shd_state {
	struct drm_private_state base;
	struct sde_encoder_phys_shd *shd_enc;
	struct sde_hw_mixer *hw_lm[MAX_MIXERS_PER_CRTC];
	struct sde_hw_ctl *hw_ctl[MAX_MIXERS_PER_CRTC];
	u32 num_mixers;
	u32 num_ctls;
};

#define to_sde_enc_shd_priv_state(x) \
		container_of((x), struct sde_enc_shd_state, base)

static void sde_enc_shd_destroy_state(struct drm_private_obj *obj,
		struct drm_private_state *base_state)
{
	struct sde_enc_shd_state *state = to_sde_enc_shd_priv_state(base_state);
	struct sde_shd_hw_ctl *hw_ctl;
	struct sde_shd_hw_mixer *hw_lm;
	int i;

	for (i = 0; i < MAX_MIXERS_PER_CRTC; i++) {
		if (state->hw_ctl[i]) {
			hw_ctl = container_of(state->hw_ctl[i], struct sde_shd_hw_ctl,
					base);
			kfree(hw_ctl);
		}
		if (state->hw_lm[i]) {
			hw_lm = container_of(state->hw_lm[i], struct sde_shd_hw_mixer,
					base);
			kfree(hw_lm);
		}
	}

	kfree(state);
}

static struct drm_private_state *sde_enc_shd_duplicate_state(
		struct drm_private_obj *obj)
{
	struct sde_enc_shd_state *state, *old_state =
			to_sde_enc_shd_priv_state(obj->state);

	state = kmemdup(old_state, sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &state->base);

	return &state->base;
}

static const struct drm_private_state_funcs sde_enc_shd_state_funcs = {
	.atomic_duplicate_state = sde_enc_shd_duplicate_state,
	.atomic_destroy_state = sde_enc_shd_destroy_state,
};

static struct sde_enc_shd_state *sde_enc_shd_get_atomic_state(
		struct drm_atomic_state *state, struct sde_encoder_phys_shd *shd_enc)
{
	struct drm_device *dev = shd_enc->base.parent->dev;

	WARN_ON(!drm_modeset_is_locked(&dev->mode_config.connection_mutex));

	return to_sde_enc_shd_priv_state(
			drm_atomic_get_private_obj_state(state, &shd_enc->obj));
}

static inline bool sde_encoder_phys_shd_is_master(struct sde_encoder_phys *phys_enc)
{
	return true;
}

static void sde_encoder_phys_shd_underrun_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys *phys_enc = arg;

	if (!phys_enc)
		return;

	if (phys_enc->parent_ops.handle_underrun_virt)
		phys_enc->parent_ops.handle_underrun_virt(phys_enc->parent,
			phys_enc);
}

static void sde_encoder_phys_shd_vblank_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys *phys_enc = arg;
	struct sde_hw_ctl *hw_ctl;
	struct sde_shd_hw_ctl *shd_ctl;
	unsigned long lock_flags;
	u32 flush_register = ~0;
	int new_cnt = -1, old_cnt = -1;
	u32 event = 0;

	if (!phys_enc)
		return;

	hw_ctl = phys_enc->hw_ctl;
	if (!hw_ctl)
		return;

	SDE_ATRACE_BEGIN("shd_vblank_irq");

	/*
	 * only decrement the pending flush count if we've actually flushed
	 * hardware. due to sw irq latency, vblank may have already happened
	 * so we need to double-check with hw that it accepted the flush bits
	 */
	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);

	old_cnt = atomic_read(&phys_enc->pending_kickoff_cnt);

	if (hw_ctl && hw_ctl->ops.get_flush_register)
		flush_register = hw_ctl->ops.get_flush_register(hw_ctl);

	shd_ctl = container_of(hw_ctl, struct sde_shd_hw_ctl, base);

	if (flush_register)
		SDE_DEBUG("%d irq flush=0x%x mask=0x%x\n", DRMID(phys_enc->parent),
			  flush_register, shd_ctl->flush_mask);

	if (flush_register & shd_ctl->flush_mask)
		goto not_flushed;

	/*
	 * When flush_mask is changed to 0, we need additional vsync
	 * to make sure the detach flush is done
	 */
	if (flush_register && !shd_ctl->flush_mask && shd_ctl->old_mask) {
		shd_ctl->old_mask = 0;
		goto not_flushed;
	}

	new_cnt = atomic_add_unless(&phys_enc->pending_kickoff_cnt, -1, 0);

	if (atomic_add_unless(&phys_enc->pending_retire_fence_cnt, -1, 0))
		event |= SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE |
			SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE |
			SDE_ENCODER_FRAME_EVENT_DONE;

not_flushed:
	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

	if (event && phys_enc->parent_ops.handle_frame_done)
		phys_enc->parent_ops.handle_frame_done(phys_enc->parent,
			phys_enc, event);

	if (phys_enc->parent_ops.handle_vblank_virt)
		phys_enc->parent_ops.handle_vblank_virt(phys_enc->parent,
				phys_enc);

	SDE_EVT32_IRQ(DRMID(phys_enc->parent), phys_enc->hw_intf->idx - INTF_0, old_cnt, new_cnt,
		      flush_register, event);

	/* Signal any waiting atomic commit thread */
	wake_up_all(&phys_enc->pending_kickoff_wq);

	SDE_ATRACE_END("shd_vblank_irq");
}

static inline void _sde_encoder_phys_shd_setup_irq_hw_idx(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_irq *irq;

	irq = &phys_enc->irq[INTR_IDX_VSYNC];

	if (irq->irq_idx < 0)
		irq->hw_idx = phys_enc->intf_idx;

	irq = &phys_enc->irq[INTR_IDX_UNDERRUN];
	if (irq->irq_idx < 0)
		irq->hw_idx = phys_enc->intf_idx;
}

static int _sde_encoder_phys_shd_rm_reserve(struct sde_encoder_phys *phys_enc,
		struct drm_atomic_state *state,
		struct shd_display *display)
{
	struct sde_encoder_phys_shd *shd_enc;
	struct sde_rm *rm;
	struct sde_enc_shd_state *shd_enc_state;
	struct sde_rm_hw_iter ctl_iter, lm_iter, pp_iter, dsc_iter, ds_iter;
	struct drm_encoder *encoder;
	struct drm_connector_state *conn_state;
	struct sde_shd_hw_ctl *hw_ctl;
	struct sde_shd_hw_mixer *hw_lm;
	struct sde_hw_pingpong *hw_pp;
	struct sde_hw_mixer *sde_hw_lm;
	struct sde_hw_ctl *sde_hw_ctl;
	struct sde_hw_dsc *hw_dsc;

	int i, rc = 0;

	if (!state) {
		SDE_ERROR("invalid state\n");
		return -EINVAL;
	}

	conn_state = drm_atomic_get_new_connector_state(state, display->base->connector);
	if (conn_state)
		encoder = conn_state->best_encoder;
	else
		encoder = display->base->connector->state->best_encoder;

	if (!encoder) {
		SDE_ERROR("failed to find base encoder\n");
		return -EINVAL;
	}
	rm = &phys_enc->sde_kms->rm;
	shd_enc = to_sde_encoder_phys_shd(phys_enc);
	shd_enc_state = sde_enc_shd_get_atomic_state(state, shd_enc);
	if (IS_ERR(shd_enc_state))
		return PTR_ERR(shd_enc_state);

	sde_rm_init_hw_iter(&ctl_iter, DRMID(encoder), SDE_HW_BLK_CTL);
	sde_rm_init_hw_iter(&lm_iter, DRMID(encoder), SDE_HW_BLK_LM);
	sde_rm_init_hw_iter(&pp_iter, DRMID(encoder), SDE_HW_BLK_PINGPONG);
	sde_rm_init_hw_iter(&dsc_iter, DRMID(encoder), SDE_HW_BLK_DSC);
	sde_rm_init_hw_iter(&ds_iter, DRMID(encoder), SDE_HW_BLK_DS);

	// Allocate new HW blocks
	shd_enc_state->num_mixers = 0;
	shd_enc_state->num_ctls = 0;
	memset(shd_enc_state->hw_ctl, 0, sizeof(shd_enc_state->hw_ctl));
	memset(shd_enc_state->hw_lm, 0, sizeof(shd_enc_state->hw_lm));

	for (i = 0; i < MAX_MIXERS_PER_CRTC; i++) {
		hw_ctl = kzalloc(sizeof(*hw_ctl), GFP_KERNEL);
		if (!hw_ctl) {
			rc = -ENOMEM;
			goto failed;
		}
		shd_enc_state->hw_ctl[i] = &hw_ctl->base;

		hw_lm = kzalloc(sizeof(*hw_lm), GFP_KERNEL);
		if (!hw_lm) {
			rc = -ENOMEM;
			goto failed;
		}
		shd_enc_state->hw_lm[i] = &hw_lm->base;
	}

	for (i = 0; i < MAX_MIXERS_PER_CRTC; i++) {
		/* reserve lm */
		if (!(rc = sde_rm_atomic_get_hw(rm, state, &lm_iter)))
			break;

		hw_lm = container_of(shd_enc_state->hw_lm[i], struct sde_shd_hw_mixer, base);
		sde_hw_lm = to_sde_hw_mixer(lm_iter.hw);
		hw_lm->base = *sde_hw_lm;
		hw_lm->range = display->stage_range;
		hw_lm->roi = display->roi;
		hw_lm->orig = sde_hw_lm;
		sde_shd_hw_lm_init_op(&hw_lm->base);
		SDE_DEBUG("reserve LM%d %pK from enc %d to %d\n",
			hw_lm->base.idx, &hw_lm->base.hw,
			DRMID(encoder),
			DRMID(phys_enc->parent));

		rc = sde_rm_ext_blk_create_reserve_lm(rm, state, lm_iter.blk, phys_enc->parent,
						      &hw_lm->base);
		if (rc) {
			SDE_ERROR("failed to create & reserve lm\n");
			return rc;
		}
		shd_enc_state->num_mixers++;
	}

	for (i = 0; i < shd_enc_state->num_mixers; i++) {
		/* reserve pingpong */
		if (!(rc = sde_rm_atomic_get_hw(rm, state, &pp_iter)))
			break;

		hw_pp = to_sde_hw_pingpong(pp_iter.hw);
		SDE_DEBUG("reserve PP%d %pK from enc %d to %d\n",
			hw_pp->idx, &hw_pp->hw,
			DRMID(encoder),
			DRMID(phys_enc->parent));

		rc = sde_rm_ext_blk_create_reserve(rm, state,
				pp_iter.blk, phys_enc->parent, &hw_pp->hw);
		if (rc) {
			SDE_ERROR("failed to create & reserve pingpong\n");
			break;
		}
	}

	for (i = 0; i < shd_enc_state->num_mixers; i++) {
		/* reserve dsc */
		if (!(rc = sde_rm_atomic_get_hw(rm, state, &dsc_iter)))
			break;
		hw_dsc = to_sde_hw_dsc(dsc_iter.hw);

		SDE_DEBUG("reserve DSC%d %pK from enc %d to %d\n",
			hw_dsc->idx, &hw_dsc->hw,
			DRMID(encoder),
			DRMID(phys_enc->parent));

		rc = sde_rm_ext_blk_create_reserve(rm, state,
				dsc_iter.blk, phys_enc->parent, &hw_dsc->hw);
		if (rc) {
			SDE_ERROR("failed to create & reserve dsc\n");
			break;
		}
	}

	for (i = 0; i < MAX_MIXERS_PER_CRTC; i++) {
		/* reserve ctl */
		if (!(rc = sde_rm_atomic_get_hw(rm, state, &ctl_iter)))
			break;

		hw_ctl = container_of(shd_enc_state->hw_ctl[i],
				struct sde_shd_hw_ctl, base);
		sde_hw_ctl = to_sde_hw_ctl(ctl_iter.hw);
		hw_ctl->base = *sde_hw_ctl;
		hw_ctl->range = display->stage_range;
		hw_ctl->orig = sde_hw_ctl;
		if (shd_enc_state->hw_ctl[i])
			hw_ctl->dsc_cfg = container_of(shd_enc_state->hw_ctl[i],
					struct sde_shd_hw_ctl, base)->dsc_cfg;
		sde_shd_hw_ctl_init_op(&hw_ctl->base);

		SDE_DEBUG("reserve CTL%d %pK from enc %d to %d\n",
			hw_ctl->base.idx, &hw_ctl->base.hw,
			DRMID(encoder),
			DRMID(phys_enc->parent));
		rc = sde_rm_ext_blk_create_reserve_ctl(rm, state,
				ctl_iter.blk, phys_enc->parent, &hw_ctl->base);
		if (rc) {
			SDE_ERROR("failed to create & reserve ctl\n");
			break;
		}
		shd_enc_state->num_ctls++;
	}
failed:
	return rc;
}

static void sde_encoder_phys_shd_mode_set(struct sde_encoder_phys *phys_enc,
					  struct drm_display_mode *mode,
					  struct drm_display_mode *adj_mode, bool *reinit_mixers)
{
	struct drm_connector *connector;
	struct shd_display *display;
	struct drm_encoder *encoder;
	struct sde_rm_hw_iter iter;
	struct sde_rm *rm;

	SDE_DEBUG("%d\n", phys_enc->parent->base.id);

	phys_enc->cached_mode = *adj_mode;

	connector = phys_enc->connector;
	if (!connector || connector->encoder != phys_enc->parent) {
		SDE_ERROR("failed to find connector\n");
		return;
	}

	display = sde_connector_get_display(connector);
	encoder = display->base->connector->encoder;
	if (!encoder)
		return;

	rm = &phys_enc->sde_kms->rm;

	sde_rm_init_hw_iter(&iter, DRMID(phys_enc->parent), SDE_HW_BLK_CTL);
	if (sde_rm_get_hw(rm, &iter))
		phys_enc->hw_ctl = to_sde_hw_ctl(iter.hw);

	if (IS_ERR_OR_NULL(phys_enc->hw_ctl)) {
		SDE_DEBUG("failed to init ctl, %ld\n", PTR_ERR(phys_enc->hw_ctl));

		phys_enc->hw_ctl = NULL;
		return;
	}

	sde_rm_init_hw_iter(&iter, DRMID(encoder), SDE_HW_BLK_INTF);
	if (sde_rm_get_hw(rm, &iter))
		phys_enc->hw_intf = to_sde_hw_intf(iter.hw);
	if (IS_ERR_OR_NULL(phys_enc->hw_intf)) {
		SDE_DEBUG("failed to init intf: %ld\n", PTR_ERR(phys_enc->hw_intf));

		phys_enc->hw_intf = NULL;
		return;
	}

	sde_rm_init_hw_iter(&iter, DRMID(encoder), SDE_HW_BLK_PINGPONG);
	if (sde_rm_get_hw(rm, &iter))
		phys_enc->hw_pp =  to_sde_hw_pingpong(iter.hw);
	if (IS_ERR_OR_NULL(phys_enc->hw_pp)) {
		SDE_DEBUG("failed to init pingpong: %ld\n", PTR_ERR(phys_enc->hw_pp));

		phys_enc->hw_pp = NULL;
		return;
	}

	_sde_encoder_phys_shd_setup_irq_hw_idx(phys_enc);
	phys_enc->kickoff_timeout_ms = sde_encoder_helper_get_kickoff_timeout_ms(phys_enc->parent);
}

static int _sde_encoder_phys_shd_wait_for_vblank(struct sde_encoder_phys *phys_enc, bool notify)
{
	struct sde_encoder_wait_info wait_info;
	int ret = 0;
	u32 event = 0;
	u32 event_helper = 0;
	enum sde_intr_idx intr_idx;
	struct drm_connector *conn;

	if (!phys_enc) {
		pr_err("invalid encoder\n");
		return -EINVAL;
	}

	conn = phys_enc->connector;

	wait_info.wq = &phys_enc->pending_kickoff_wq;
	wait_info.atomic_cnt = &phys_enc->pending_kickoff_cnt;
	wait_info.timeout_ms = phys_enc->kickoff_timeout_ms;

	intr_idx = sde_encoder_check_ctl_done_support(phys_enc->parent) ?
					INTR_IDX_CTL_DONE : INTR_IDX_PINGPONG;

	/* Wait for kickoff to complete */
	ret = sde_encoder_helper_wait_for_irq(phys_enc, INTR_IDX_VSYNC, &wait_info);

	event_helper = SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE
			| SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE;

	if (notify) {
		if (ret == -ETIMEDOUT) {
			event = SDE_ENCODER_FRAME_EVENT_ERROR;
			if (atomic_add_unless(&phys_enc->pending_retire_fence_cnt, -1, 0))
				event |= event_helper;
		}
	}

	SDE_EVT32(DRMID(phys_enc->parent), event, notify, ret, ret ? SDE_EVTLOG_FATAL : 0);
	if (phys_enc->parent_ops.handle_frame_done && event)
		phys_enc->parent_ops.handle_frame_done(phys_enc->parent, phys_enc, event);

	return ret;
}

static inline int sde_encoder_phys_shd_wait_for_vblank(struct sde_encoder_phys *phys_enc)
{
	return _sde_encoder_phys_shd_wait_for_vblank(phys_enc, true);
}

static inline int sde_encoder_phys_shd_wait_for_vblank_no_notify(struct sde_encoder_phys *phys_enc)
{
	return _sde_encoder_phys_shd_wait_for_vblank(phys_enc, false);
}

static inline int sde_encoder_phys_shd_prepare_for_kickoff(
		struct sde_encoder_phys *phys_enc,
		struct sde_encoder_kickoff_params *params)
{
	return 0;
}

static inline void sde_encoder_phys_shd_handle_post_kickoff(struct sde_encoder_phys *phys_enc)
{
	if (!phys_enc || !phys_enc->hw_intf) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	if (phys_enc->enable_state == SDE_ENC_ENABLING) {
		SDE_EVT32(DRMID(phys_enc->parent), phys_enc->hw_intf->idx - INTF_0);
		phys_enc->enable_state = SDE_ENC_ENABLED;
	}
}

static inline void sde_encoder_phys_shd_trigger_flush(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_shd *shd_enc;
	struct sde_enc_shd_state *shd_enc_state;

	shd_enc = container_of(phys_enc, struct sde_encoder_phys_shd, base);
	shd_enc_state = to_sde_enc_shd_priv_state(shd_enc->obj.state);

	SDE_EVT32(phys_enc->intf_idx - INTF_0);

	sde_shd_hw_flush(phys_enc->hw_ctl, shd_enc_state->hw_lm, shd_enc_state->num_mixers);
}

static int sde_encoder_phys_shd_control_vblank_irq(struct sde_encoder_phys *phys_enc, bool enable)
{
	int ret = 0;
	struct sde_encoder_phys_shd *shd_enc;
	int refcount;

	if (!phys_enc || !phys_enc->hw_intf) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	refcount = atomic_read(&phys_enc->vblank_refcount);
	shd_enc = to_sde_encoder_phys_shd(phys_enc);

	/* protect against negative */
	if (!enable && refcount == 0) {
		ret = -EINVAL;
		goto end;
	}

	SDE_DEBUG("[%pS] %d enable=%d/%d\n", __builtin_return_address(0), DRMID(phys_enc->parent),
		  enable, atomic_read(&phys_enc->vblank_refcount));

	SDE_EVT32(DRMID(phys_enc->parent), enable, atomic_read(&phys_enc->vblank_refcount));
	mutex_lock(phys_enc->vblank_ctl_lock);

	if (enable && atomic_inc_return(&phys_enc->vblank_refcount) == 1) {
		ret = sde_encoder_helper_register_irq(phys_enc, INTR_IDX_VSYNC);
		if (ret)
			atomic_dec_return(&phys_enc->vblank_refcount);
	} else if (!enable &&
			atomic_dec_return(&phys_enc->vblank_refcount) == 0) {
		ret = sde_encoder_helper_unregister_irq(phys_enc, INTR_IDX_VSYNC);
		if (ret)
			atomic_inc_return(&phys_enc->vblank_refcount);
	}
	mutex_unlock(phys_enc->vblank_ctl_lock);

end:
	if (ret) {
		SDE_DEBUG("control vblank irq error %d, enable %d\n", ret, enable);

		SDE_EVT32(DRMID(phys_enc->parent), phys_enc->hw_intf->idx - INTF_0,
			  enable, refcount, SDE_EVTLOG_ERROR);
	}
	return ret;
}

static void sde_encoder_phys_shd_enable(struct sde_encoder_phys *phys_enc)
{
	struct drm_connector *connector;

	SDE_DEBUG("%d\n", phys_enc->parent->base.id);

	if (!phys_enc->parent || !phys_enc->parent->dev) {
		SDE_ERROR("invalid drm device\n");
		return;
	}

	connector = phys_enc->connector;
	if (!connector || connector->encoder != phys_enc->parent) {
		SDE_ERROR("failed to find connector\n");
		return;
	}

	if (phys_enc->enable_state == SDE_ENC_DISABLED)
		phys_enc->enable_state = SDE_ENC_ENABLING;

	SDE_EVT32(DRMID(phys_enc->parent), atomic_read(&phys_enc->pending_retire_fence_cnt));
}

static void sde_encoder_phys_shd_single_vblank_wait(struct sde_encoder_phys *phys_enc)
{
	int ret;

	ret = sde_encoder_phys_shd_control_vblank_irq(phys_enc, true);
	if (ret) {
		SDE_ERROR_PHYS(phys_enc, "failed to enable vblank irq: %d\n", ret);
		SDE_EVT32(DRMID(phys_enc->parent), phys_enc->hw_intf->idx - INTF_0, ret,
			  SDE_EVTLOG_FUNC_CASE1, SDE_EVTLOG_ERROR);
	} else {
		ret = _sde_encoder_phys_shd_wait_for_vblank(phys_enc, false);
		if (ret) {
			atomic_set(&phys_enc->pending_kickoff_cnt, 0);
			SDE_ERROR_PHYS(phys_enc, "failure waiting for disable: %d\n", ret);
			SDE_EVT32(DRMID(phys_enc->parent), phys_enc->hw_intf->idx - INTF_0, ret,
				  SDE_EVTLOG_FUNC_CASE2, SDE_EVTLOG_ERROR);
		}
		sde_encoder_phys_shd_control_vblank_irq(phys_enc, false);
	}
}

static void sde_encoder_phys_shd_disable(struct sde_encoder_phys *phys_enc)
{
	struct shd_display *display;
	struct sde_encoder_phys_shd *shd_enc;
	unsigned long lock_flags;

	SDE_DEBUG("%d\n", phys_enc->parent->base.id);

	if (!phys_enc || !phys_enc->parent || !phys_enc->parent->dev ||
	    !phys_enc->parent->dev->dev_private) {
		SDE_ERROR("invalid encoder/device\n");
		return;
	}

	if (!phys_enc->hw_intf || !phys_enc->hw_ctl) {
		SDE_ERROR("invalid hw_intf %d hw_ctl %d\n", phys_enc->hw_intf, phys_enc->hw_ctl);
		return;
	}

	if (phys_enc->enable_state == SDE_ENC_DISABLED) {
		SDE_ERROR("already disabled\n");
		return;
	}

	shd_enc = to_sde_encoder_phys_shd(phys_enc);
	sde_encoder_helper_reset_mixers(phys_enc, NULL);

	display = sde_connector_get_display(phys_enc->connector);
	if (!display)
		goto next;

	/* if base display is already disabled, skip vsync check */
	if (!display->base->crtc->state->active)
		goto next;

	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
	sde_encoder_phys_shd_trigger_flush(phys_enc);
	sde_encoder_phys_inc_pending(phys_enc);
	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

	sde_encoder_phys_shd_single_vblank_wait(phys_enc);

next:
	phys_enc->enable_state = SDE_ENC_DISABLED;

	SDE_EVT32(DRMID(phys_enc->parent), atomic_read(&phys_enc->pending_retire_fence_cnt));
}

static inline void sde_encoder_phys_shd_destroy(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_shd *shd_enc =
		to_sde_encoder_phys_shd(phys_enc);

	if (!phys_enc)
		return;

	kfree(shd_enc);
}

static inline void sde_encoder_phys_shd_irq_ctrl(struct sde_encoder_phys *phys_enc, bool enable)
{
	sde_encoder_phys_shd_control_vblank_irq(phys_enc, enable);

	if (enable)
		sde_encoder_helper_register_irq(phys_enc, INTR_IDX_UNDERRUN);
	else
		sde_encoder_helper_unregister_irq(phys_enc, INTR_IDX_UNDERRUN);
}

static inline int sde_encoder_phys_shd_get_line_count(struct sde_encoder_phys *phys)
{
	return 0;
}

static int sde_encoder_phys_shd_atomic_check(struct sde_encoder_phys *phys_enc,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state)
{
	struct shd_display *display;

	if (!phys_enc || !crtc_state || !conn_state || !conn_state->state ||
	    !conn_state->connector) {
		SDE_ERROR("invalid encoder/device\n");
		return -EINVAL;
	}

	/*
	 * Only reserve the resources when mode changes and CRTC is active.
	 * For connectors_change, i.e. CWB case, HW resources should remain
	 * same, and no modeset will be called to the encoder to update.
	 */
	if ((!crtc_state->mode_changed && !crtc_state->active_changed)
		|| !crtc_state->active)
		return 0;

	display = sde_connector_get_display(conn_state->connector);

	return _sde_encoder_phys_shd_rm_reserve(phys_enc,
			conn_state->state, display);
}

/**
 * sde_encoder_phys_shd_init_ops - initialize writeback operations
 * @ops:	Pointer to encoder operation table
 */
static void sde_encoder_phys_shd_init_ops(struct sde_encoder_phys_ops *ops)
{
	ops->is_master = sde_encoder_phys_shd_is_master;
	ops->mode_set = sde_encoder_phys_shd_mode_set;
	ops->enable = sde_encoder_phys_shd_enable;
	ops->disable = sde_encoder_phys_shd_disable;
	ops->destroy = sde_encoder_phys_shd_destroy;
	ops->wait_for_commit_done = sde_encoder_phys_shd_wait_for_vblank;
	ops->wait_for_vblank = sde_encoder_phys_shd_wait_for_vblank_no_notify;
	ops->prepare_for_kickoff = sde_encoder_phys_shd_prepare_for_kickoff;
	ops->handle_post_kickoff = sde_encoder_phys_shd_handle_post_kickoff;
	ops->trigger_flush = sde_encoder_phys_shd_trigger_flush;
	ops->control_vblank_irq = sde_encoder_phys_shd_control_vblank_irq;
	ops->wait_for_tx_complete = sde_encoder_phys_shd_wait_for_vblank;
	ops->irq_control = sde_encoder_phys_shd_irq_ctrl;
	ops->get_line_count = sde_encoder_phys_shd_get_line_count;
	ops->atomic_check = sde_encoder_phys_shd_atomic_check;
}

void *sde_encoder_phys_shd_init(enum sde_intf_type type, u32 controller_id, void *phys_init_params)
{
	struct sde_enc_phys_init_params *p = phys_init_params;
	struct sde_encoder_phys *phys_enc;
	struct sde_encoder_phys_shd *shd_enc;
	struct sde_encoder_irq *irq;
	struct sde_enc_shd_state *state;
	int ret = 0, i;

	SDE_DEBUG("\n");

	shd_enc = kzalloc(sizeof(*shd_enc), GFP_KERNEL);
	if (!shd_enc) {
		ret = -ENOMEM;
		goto fail_alloc;
	}

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (state == NULL) {
		ret = -ENOMEM;
		goto fail_state;
	}

	drm_atomic_private_obj_init(p->sde_kms->dev,
				    &shd_enc->obj,
				    &state->base,
				    &sde_enc_shd_state_funcs);

	state->shd_enc = shd_enc;

	phys_enc = &shd_enc->base;

	sde_encoder_phys_shd_init_ops(&phys_enc->ops);
	phys_enc->parent = p->parent;
	phys_enc->parent_ops = p->parent_ops;
	phys_enc->sde_kms = p->sde_kms;
	phys_enc->split_role = p->split_role;
	phys_enc->intf_mode = INTF_MODE_NONE;
	phys_enc->intf_idx = INTF_0 + controller_id;

	phys_enc->enc_spinlock = p->enc_spinlock;
	phys_enc->vblank_ctl_lock = p->vblank_ctl_lock;
	atomic_set(&phys_enc->pending_retire_fence_cnt, 0);

	irq = &phys_enc->irq[INTR_IDX_VSYNC];
	irq->name = "vsync_irq";
	irq->intr_type = SDE_IRQ_TYPE_INTF_VSYNC;
	irq->intr_idx = INTR_IDX_VSYNC;
	irq->cb.func = sde_encoder_phys_shd_vblank_irq;

	irq = &phys_enc->irq[INTR_IDX_UNDERRUN];
	irq->name = "underrun";
	irq->intr_type = SDE_IRQ_TYPE_INTF_UNDER_RUN;
	irq->intr_idx = INTR_IDX_UNDERRUN;
	irq->cb.func = sde_encoder_phys_shd_underrun_irq;

	for (i = 0; i < INTR_IDX_MAX; i++) {
		irq = &phys_enc->irq[i];
		INIT_LIST_HEAD(&irq->cb.list);
		irq->irq_idx = -EINVAL;
		irq->hw_idx = -EINVAL;
		irq->cb.arg = phys_enc;
	}

	atomic_set(&phys_enc->vblank_refcount, 0);
	atomic_set(&phys_enc->pending_kickoff_cnt, 0);
	atomic_set(&phys_enc->pending_retire_fence_cnt, 0);
	init_waitqueue_head(&phys_enc->pending_kickoff_wq);
	phys_enc->enable_state = SDE_ENC_DISABLED;
	phys_enc->kickoff_timeout_ms = DEFAULT_KICKOFF_TIMEOUT_MS;

	return phys_enc;

fail_state:
	kfree(shd_enc);
fail_alloc:

	return ERR_PTR(ret);
}
