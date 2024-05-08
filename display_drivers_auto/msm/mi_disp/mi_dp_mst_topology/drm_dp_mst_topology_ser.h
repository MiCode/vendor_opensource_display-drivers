/*
 * Copyright © 2014 Red Hat.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */
#ifndef _DRM_DP_MST_TOLOLOGY_SER_H_
#define _DRM_DP_MST_TOLOLOGY_SER_H_

#include <linux/types.h>
#include <drm/display/drm_dp_helper.h>
#include <drm/drm_atomic.h>

int mi_drm_dp_mst_topology_mgr_init(struct drm_dp_mst_topology_mgr *mgr,
				 struct drm_device *dev, struct drm_dp_aux *aux,
				 int max_dpcd_transaction_bytes,
				 int max_payloads, int conn_base_id);

void mi_drm_dp_mst_topology_mgr_destroy(struct drm_dp_mst_topology_mgr *mgr);

bool mi_drm_dp_read_mst_cap(struct drm_dp_aux *aux, const u8 dpcd[DP_RECEIVER_CAP_SIZE]);
int mi_drm_dp_mst_topology_mgr_set_mst(struct drm_dp_mst_topology_mgr *mgr, bool mst_state);

int mi_drm_dp_mst_hpd_irq(struct drm_dp_mst_topology_mgr *mgr, u8 *esi, bool *handled);


int
mi_drm_dp_mst_detect_port(struct drm_connector *connector,
		       struct drm_modeset_acquire_ctx *ctx,
		       struct drm_dp_mst_topology_mgr *mgr,
		       struct drm_dp_mst_port *port);

struct edid *mi_drm_dp_mst_get_edid(struct drm_connector *connector, struct drm_dp_mst_topology_mgr *mgr, struct drm_dp_mst_port *port);

int mi_drm_dp_get_vc_payload_bw(const struct drm_dp_mst_topology_mgr *mgr,
			     int link_rate, int link_lane_count);

int mi_drm_dp_calc_pbn_mode(int clock, int bpp, bool dsc);

void mi_drm_dp_mst_update_slots(struct drm_dp_mst_topology_state *mst_state, uint8_t link_encoding_cap);

int mi_drm_dp_add_payload_part1(struct drm_dp_mst_topology_mgr *mgr,
			     struct drm_dp_mst_topology_state *mst_state,
			     struct drm_dp_mst_atomic_payload *payload);
int mi_drm_dp_add_payload_part2(struct drm_dp_mst_topology_mgr *mgr,
			     struct drm_atomic_state *state,
			     struct drm_dp_mst_atomic_payload *payload);
void mi_drm_dp_remove_payload(struct drm_dp_mst_topology_mgr *mgr,
			   struct drm_dp_mst_topology_state *mst_state,
			   const struct drm_dp_mst_atomic_payload *old_payload,
			   struct drm_dp_mst_atomic_payload *new_payload);

int mi_drm_dp_check_act_status(struct drm_dp_mst_topology_mgr *mgr);

void mi_drm_dp_mst_dump_topology(struct seq_file *m,
			      struct drm_dp_mst_topology_mgr *mgr);

void mi_drm_dp_mst_topology_mgr_suspend(struct drm_dp_mst_topology_mgr *mgr);
int __must_check
mi_drm_dp_mst_topology_mgr_resume(struct drm_dp_mst_topology_mgr *mgr,
			       bool sync);

ssize_t drm_dp_mst_dpcd_read(struct drm_dp_aux *aux,
			     unsigned int offset, void *buffer, size_t size);
ssize_t drm_dp_mst_dpcd_write(struct drm_dp_aux *aux,
			      unsigned int offset, void *buffer, size_t size);

//int mi_drm_dp_mst_connector_late_register(struct drm_connector *connector,
				   //    struct drm_dp_mst_port *port);
//void mi_drm_dp_mst_connector_early_unregister(struct drm_connector *connector,
					   //struct drm_dp_mst_port *port);

struct drm_dp_mst_topology_state *
mi_drm_atomic_get_mst_topology_state(struct drm_atomic_state *state,
				  struct drm_dp_mst_topology_mgr *mgr);
struct drm_dp_mst_topology_state *
mi_drm_atomic_get_old_mst_topology_state(struct drm_atomic_state *state,
				      struct drm_dp_mst_topology_mgr *mgr);
struct drm_dp_mst_topology_state *
mi_drm_atomic_get_new_mst_topology_state(struct drm_atomic_state *state,
				      struct drm_dp_mst_topology_mgr *mgr);
struct drm_dp_mst_atomic_payload *
mi_drm_atomic_get_mst_payload_state(struct drm_dp_mst_topology_state *state,
				 struct drm_dp_mst_port *port);
int __must_check
mi_drm_dp_atomic_find_time_slots(struct drm_atomic_state *state,
			      struct drm_dp_mst_topology_mgr *mgr,
			      struct drm_dp_mst_port *port, int pbn);
int mi_drm_dp_mst_atomic_enable_dsc(struct drm_atomic_state *state,
				 struct drm_dp_mst_port *port,
				 int pbn, bool enable);
int __must_check
mi_drm_dp_mst_add_affected_dsc_crtcs(struct drm_atomic_state *state,
				  struct drm_dp_mst_topology_mgr *mgr);
int __must_check
mi_drm_dp_atomic_release_time_slots(struct drm_atomic_state *state,
				 struct drm_dp_mst_topology_mgr *mgr,
				 struct drm_dp_mst_port *port);
void mi_drm_dp_mst_atomic_wait_for_dependencies(struct drm_atomic_state *state);
int __must_check mi_drm_dp_mst_atomic_setup_commit(struct drm_atomic_state *state);
int mi_drm_dp_send_power_updown_phy(struct drm_dp_mst_topology_mgr *mgr,
				 struct drm_dp_mst_port *port, bool power_up);
int mi_drm_dp_send_query_stream_enc_status(struct drm_dp_mst_topology_mgr *mgr,
		struct drm_dp_mst_port *port,
		struct drm_dp_query_stream_enc_status_ack_reply *status);
int __must_check mi_drm_dp_mst_atomic_check(struct drm_atomic_state *state);
int __must_check mi_drm_dp_mst_root_conn_atomic_check(struct drm_connector_state *new_conn_state,
						   struct drm_dp_mst_topology_mgr *mgr);

void mi_drm_dp_mst_get_port_malloc(struct drm_dp_mst_port *port);
void mi_drm_dp_mst_put_port_malloc(struct drm_dp_mst_port *port);

struct drm_dp_aux *mi_drm_dp_mst_dsc_aux_for_port(struct drm_dp_mst_port *port);

static inline struct drm_dp_mst_topology_state *
mi_to_drm_dp_mst_topology_state(struct drm_private_state *state)
{
	return container_of(state, struct drm_dp_mst_topology_state, base);
}

extern const struct drm_private_state_funcs mi_drm_dp_mst_topology_state_funcs;

/**
 * mi___drm_dp_mst_state_iter_get - private atomic state iterator function for
 * macro-internal use
 * @state: &struct drm_atomic_state pointer
 * @mgr: pointer to the &struct drm_dp_mst_topology_mgr iteration cursor
 * @old_state: optional pointer to the old &struct drm_dp_mst_topology_state
 * iteration cursor
 * @new_state: optional pointer to the new &struct drm_dp_mst_topology_state
 * iteration cursor
 * @i: int iteration cursor, for macro-internal use
 *
 * Used by mi_for_each_oldnew_mst_mgr_in_state(),
 * mi_for_each_old_mst_mgr_in_state(), and mi_for_each_new_mst_mgr_in_state(). Don't
 * call this directly.
 *
 * Returns:
 * True if the current &struct drm_private_obj is a &struct
 * drm_dp_mst_topology_mgr, false otherwise.
 */
static inline bool
mi___drm_dp_mst_state_iter_get(struct drm_atomic_state *state,
			    struct drm_dp_mst_topology_mgr **mgr,
			    struct drm_dp_mst_topology_state **old_state,
			    struct drm_dp_mst_topology_state **new_state,
			    int i)
{
	struct __drm_private_objs_state *objs_state = &state->private_objs[i];

	if (objs_state->ptr->funcs != &mi_drm_dp_mst_topology_state_funcs)
		return false;

	*mgr = to_dp_mst_topology_mgr(objs_state->ptr);
	if (old_state)
		*old_state = to_dp_mst_topology_state(objs_state->old_state);
	if (new_state)
		*new_state = to_dp_mst_topology_state(objs_state->new_state);

	return true;
}

/**
 * mi_for_each_oldnew_mst_mgr_in_state - iterate over all DP MST topology
 * managers in an atomic update
 * @__state: &struct drm_atomic_state pointer
 * @mgr: &struct drm_dp_mst_topology_mgr iteration cursor
 * @old_state: &struct drm_dp_mst_topology_state iteration cursor for the old
 * state
 * @new_state: &struct drm_dp_mst_topology_state iteration cursor for the new
 * state
 * @__i: int iteration cursor, for macro-internal use
 *
 * This iterates over all DRM DP MST topology managers in an atomic update,
 * tracking both old and new state. This is useful in places where the state
 * delta needs to be considered, for example in atomic check functions.
 */
#define mi_for_each_oldnew_mst_mgr_in_state(__state, mgr, old_state, new_state, __i) \
	for ((__i) = 0; (__i) < (__state)->num_private_objs; (__i)++) \
		for_each_if(mi___drm_dp_mst_state_iter_get((__state), &(mgr), &(old_state), &(new_state), (__i)))

/**
 * mi_for_each_old_mst_mgr_in_state - iterate over all DP MST topology managers
 * in an atomic update
 * @__state: &struct drm_atomic_state pointer
 * @mgr: &struct drm_dp_mst_topology_mgr iteration cursor
 * @old_state: &struct drm_dp_mst_topology_state iteration cursor for the old
 * state
 * @__i: int iteration cursor, for macro-internal use
 *
 * This iterates over all DRM DP MST topology managers in an atomic update,
 * tracking only the old state. This is useful in disable functions, where we
 * need the old state the hardware is still in.
 */
#define mi_for_each_old_mst_mgr_in_state(__state, mgr, old_state, __i) \
	for ((__i) = 0; (__i) < (__state)->num_private_objs; (__i)++) \
		for_each_if(mi___drm_dp_mst_state_iter_get((__state), &(mgr), &(old_state), NULL, (__i)))

/**
 * mi_for_each_new_mst_mgr_in_state - iterate over all DP MST topology managers
 * in an atomic update
 * @__state: &struct drm_atomic_state pointer
 * @mgr: &struct drm_dp_mst_topology_mgr iteration cursor
 * @new_state: &struct drm_dp_mst_topology_state iteration cursor for the new
 * state
 * @__i: int iteration cursor, for macro-internal use
 *
 * This iterates over all DRM DP MST topology managers in an atomic update,
 * tracking only the new state. This is useful in enable functions, where we
 * need the new state the hardware should be in when the atomic commit
 * operation has completed.
 */
#define mi_for_each_new_mst_mgr_in_state(__state, mgr, new_state, __i) \
	for ((__i) = 0; (__i) < (__state)->num_private_objs; (__i)++) \
		for_each_if(mi___drm_dp_mst_state_iter_get((__state), &(mgr), NULL, &(new_state), (__i)))

#endif
