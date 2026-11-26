// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hfi_adapter.h"
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#if IS_ENABLED(CONFIG_QTI_HFI_CORE)
#include "hfi_interface.h"
#endif

#define HFI_AD_INFO(fmt, ...)  \
	pr_info("[hfi_ad_info] %s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define HFI_AD_WARN(fmt, ...)  \
	pr_warn("[hfi_ad_warn] %s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define HFI_AD_ERROR(fmt, ...)  \
	pr_err("[hfi_ad_error] %s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define HFI_AD_DEBUG(fmt, ...)  \
	pr_debug("[hfi_ad_debug] %s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define HFI_APADTER_STEP_US 10000
#define MAX_TRY_COUNT 200
#define MAX_BUFFERS 10
#define MAX_U32 0xFFFFFFFF
#define MAX_POOL_SIZE 8
#define GET_BUF_RETRY 35
#define MIN_USLEEP_RANGE 30000
#define MAX_USLEEP_RANGE 40000

/* Buffer id specific macros */
#define CLIENT_ID_MASK 0xFF //LSB 8 bits are for storing client_id
#define UNIQUE_BUFF_MASK 0xFFFFFF00 //MSB 24 bits are for storing unique buffer id
#define GET_CLIENT_ID(data)           \
	(data & CLIENT_ID_MASK)

#if IS_ENABLED(CONFIG_QTI_HFI_CORE)
static u32 unique_id_counter = 1;
static atomic_t work_queue_pos_wr = ATOMIC_INIT(0);
static DECLARE_BITMAP(wq_inuse_bitmap, HFI_ADAPTER_WORK_QUEUE_SIZE);

static u32 hfi_cmd_type_map[HFI_CMDBUF_TYPE_MAX] = {
	[HFI_CMDBUF_TYPE_ATOMIC_CHECK] = HFI_CMD_BUFF_DISPLAY,
	[HFI_CMDBUF_TYPE_ATOMIC_COMMIT] = HFI_CMD_BUFF_DISPLAY,
	[HFI_CMDBUF_TYPE_DISPLAY_INFO_NO_BLOCK] = HFI_CMD_BUFF_DISPLAY,
	[HFI_CMDBUF_TYPE_DISPLAY_INFO_BLOCKING] = HFI_CMD_BUFF_DISPLAY,
	[HFI_CMDBUF_TYPE_DEVICE_INFO] = HFI_CMD_BUFF_DEVICE,
	[HFI_CMDBUF_TYPE_GET_DEBUG_DATA] = HFI_CMD_BUFF_DEBUG,
};

static atomic_t id_counter = ATOMIC_INIT(0);

static u32 _create_buffer_id(u32 ctx_id)
{
	u32 unique_id;

	if (unique_id_counter == 0xFFFFFF)
		unique_id_counter = 1;

	/* Take MSB 24 bits */
	unique_id = (unique_id_counter++ & 0xFFFFFF) << 8;

	return unique_id | (ctx_id & CLIENT_ID_MASK);
}

static int _generate_sequential_packet_id(void)
{
	u32 id;

	do {
		id = atomic_read(&id_counter);
		if (id == MAX_U32) {
			if (atomic_cmpxchg(&id_counter, id, 0) == id)
				HFI_AD_ERROR("failed to reset packet_id counter\n");
		}

	} while ((id != MAX_U32) && atomic_cmpxchg(&id_counter, id, id + 1) != id);

	return id;
}

static int release_rx_buffer_fail(struct hfi_cmdbuf_t *cmd_buf, struct hfi_adapter_t *host)
{
	struct hfi_core_cmds_buf_desc *buff_arr[MAX_BUFFERS];
	int rc;

	if (!cmd_buf || !host)
		return -EINVAL;

	buff_arr[0] = &cmd_buf->buf;
	rc = hfi_core_release_rx_buffer(host->session, buff_arr, 1);
	if (rc)
		HFI_AD_ERROR("failed to release emergency rx buffer\n");

	atomic_set(&cmd_buf->pool->available, 1);

	return rc;
}

static struct hfi_buffer_pool *get_avail_buffer(struct hfi_adapter_t *host)
{
	struct list_head *pos;
	struct hfi_buffer_pool *pool = NULL;
	struct hfi_buffer_pool *ret_pool = NULL;

	if (!host) {
		HFI_AD_ERROR("invalid params\n");
		return NULL;
	}

	mutex_lock(&host->hfi_adapter_cmd_buf_list_lock);
	list_for_each(pos, &host->pool->node) {
		pool = list_entry(pos, struct hfi_buffer_pool, node);
		if (!pool) {
			mutex_unlock(&host->hfi_adapter_cmd_buf_list_lock);
			HFI_AD_ERROR("pool is not initialized\n");
			return NULL;
		}

		if (atomic_read(&pool->available)) {
			HFI_AD_DEBUG("found available buffer %p\n", &pool->buffer_t);
			atomic_set(&pool->available, 0);
			ret_pool = pool;
			break;
		}
	}
	mutex_unlock(&host->hfi_adapter_cmd_buf_list_lock);

	if (!ret_pool)
		HFI_AD_ERROR("could not get buffer\n");

	return ret_pool;
}

static void _process_cb_cmd_buf_work(struct kthread_work *work)
{
	struct list_head *ctx_pos;
	struct hfi_client_t *ctx;
	struct hfi_cmdbuf_t *hfi_buff;
	u32 obj_id_rx = MAX_U32;
	int client_id;
	struct hfi_adapter_t *host;
	struct callback_work *cb_cmd_buf_work;
	struct hfi_core_cmds_buf_desc *rx_buffer;
	struct hfi_header *virtio_hdr;
	struct hfi_buffer_pool *pool;
	bool client_found = false;
	int index;
	int i = 0;

	if (!work) {
		HFI_AD_ERROR("%s null work\n", __func__);
		return;
	}

	cb_cmd_buf_work = container_of(work, struct callback_work, work);
	host = cb_cmd_buf_work->host;
	index = cb_cmd_buf_work->index;
	if (!host) {
		HFI_AD_ERROR("thread %d could not match host\n", cb_cmd_buf_work->index);
		return;
	}

	do {
		pool = get_avail_buffer(host);
		if (!pool) {
			HFI_AD_ERROR("failed to get buffer pool\n");
			return;
		}

		rx_buffer = &pool->buffer_t.buf;
		if (!rx_buffer) {
			HFI_AD_ERROR("buffer descriptor is null\n");
			atomic_set(&pool->available, 1);
			return;
		}

		if (hfi_core_cmds_rx_buf_get(host->session, rx_buffer)) {
			atomic_set(&pool->available, 1);
			clear_bit(index, wq_inuse_bitmap);
			return;
		}

		hfi_buff = &pool->buffer_t;
		virtio_hdr = (struct hfi_header *)rx_buffer->pbuf_vaddr;
		if (!virtio_hdr)
			return;

		obj_id_rx = virtio_hdr->object_id;

		hfi_buff->unique_id = virtio_hdr->header_id;

		client_id = GET_CLIENT_ID(virtio_hdr->header_id);

		list_for_each(ctx_pos, &host->client_list) {
			/* Try to match buffer based on unique OBJ ID */
			ctx = list_entry(ctx_pos, struct hfi_client_t, node);
			if (!ctx || ctx->client_id == client_id) {
				client_found = true;
				break;
			}
			continue;
		}

		if (!client_found) {
			HFI_AD_ERROR("could not match buffer client id to a client\n");
			release_rx_buffer_fail(hfi_buff, host);
			return;
		}

		INIT_LIST_HEAD(&hfi_buff->cmd_buf_chain);
		if (ctx && hfi_buff) {
			hfi_buff->obj_id = virtio_hdr->object_id;
			hfi_buff->ctx = ctx;
			hfi_buff->virtq_type = HFI_VIRTQUEUE_TYPE_RX;
			ctx->process_cmd_buf(ctx, hfi_buff);
		}  else {
			HFI_AD_ERROR("could not match buffer to a client\n");
			release_rx_buffer_fail(hfi_buff, host);
		}
	} while (i++ <= MAX_TRY_COUNT);

	clear_bit(index, wq_inuse_bitmap);

	if (i >= MAX_TRY_COUNT)
		HFI_AD_ERROR("max retries exceeded: %d\n", i);

}

static void handle_ssr_start(struct hfi_adapter_t *adapter)
{
	struct list_head *client_pos;
	struct hfi_client_t *client;

	if (!adapter)
		return;

	HFI_AD_DEBUG("handling SSR start\n");
	list_for_each(client_pos, &adapter->client_list) {
		/* Try to match buffer based on unique OBJ ID */
		client = list_entry(client_pos, struct hfi_client_t, node);
		if (client) {
			client->process_event(client, HFI_ADAPTER_EVENT_SSR_START,
				adapter->blocking);
		}
	}
}

static void handle_ssr_end(struct hfi_adapter_t *adapter)
{
	int ret = 0;
	struct hfi_client_t *client;

	if (!adapter) {
		HFI_AD_ERROR("invalid adapter\n");
		return;
	}

	HFI_AD_DEBUG("handling SSR end\n");

	ret = hfi_core_close_session(adapter->session);
	if (ret) {
		HFI_AD_ERROR("hfi_core_close_session failed, ret: %d\n", ret);
		return;
	}
	struct hfi_core_open_params open_params = {
		HFI_CORE_CLIENT_ID_0, adapter->cb_ops, HFI_CORE_HOST};

	adapter->session = hfi_core_open_session(&open_params);
	if (!adapter->session) {
		HFI_AD_ERROR("failed to open hfi core session\n");
		return;
	}

	list_for_each_entry_reverse(client, &adapter->client_list, node) {
		client->process_event(client, HFI_ADAPTER_EVENT_SSR_END,
			adapter->blocking);
	}
}

static void _process_cb_ssr_work(struct kthread_work *work)
{
	struct hfi_adapter_t *adapter;

	if (!work)
		return;

	adapter = container_of(work, struct hfi_adapter_t, cb_ssr_work);

	switch (adapter->event_type) {
	case HFI_ADAPTER_EVENT_SSR_START:
		handle_ssr_start(adapter);
		break;
	case HFI_ADAPTER_EVENT_SSR_END:
		handle_ssr_end(adapter);
		break;
	default:
		break;
	}
}

int32_t callback_function_hfi(struct hfi_core_session *hfi_session,
		const void *cb_data, enum hfi_core_event_type event_type, bool blocking)
{
	struct hfi_adapter_t *adapter = (struct hfi_adapter_t *)cb_data;
	struct callback_work *cb_cmd_buf_work;
	int ret;
	int tries, slot_found = -1;
	int work_queue_idx;

	if (!cb_data)
		return -EINVAL;

	HFI_AD_DEBUG("hfi callback called\n");

	switch (event_type) {
	case HFI_CORE_EVENT_DCP_RESPONSE:
		if (atomic_read(&adapter->ssr_in_progress))
			break;

		work_queue_idx = atomic_fetch_inc(&work_queue_pos_wr) &
				HFI_ADAPTER_WORK_QUEUE_MASK;
		/* Try to claim a free slot */
		for (tries = 0; tries < HFI_ADAPTER_WORK_QUEUE_SIZE; tries++) {
			int try_index = (work_queue_idx + tries) & HFI_ADAPTER_WORK_QUEUE_MASK;
			/* test_and_set_bit returns prev value: 0 means we successfully set it */
			if (!test_and_set_bit(try_index, wq_inuse_bitmap)) {
				slot_found = try_index;
				break;
			}
		}

		if (slot_found < 0) {
			HFI_AD_WARN("failed to find a free slot to queue work\n");
			return -EINVAL;
		}

		cb_cmd_buf_work = &adapter->cb_cmd_buf_work[slot_found];

		ret = kthread_queue_work(&adapter->cb_event_worker, &cb_cmd_buf_work->work);
		if (!ret)
			HFI_AD_WARN("failed to queue work at index:%d\n", slot_found);
		break;
	case HFI_CORE_EVENT_SSR_START:
		HFI_AD_DEBUG("SSR has been initiated\n");
		atomic_set(&adapter->ssr_in_progress, 1);
		/* finish processing all buffers sent by dcp */
		kthread_flush_worker(&adapter->cb_event_worker);
		adapter->event_type = HFI_ADAPTER_EVENT_SSR_START;
		adapter->blocking = blocking;
		ret = kthread_queue_work(&adapter->cb_event_ssr_worker, &adapter->cb_ssr_work);
		if (!ret)
			HFI_AD_WARN("failed to queue ssr start work\n");

		/* This event callback is in non ISR context so blocing is fine */
		if (blocking)
			kthread_flush_work(&adapter->cb_ssr_work);
		break;
	case HFI_CORE_EVENT_SSR_END:
		adapter->event_type = HFI_ADAPTER_EVENT_SSR_END;
		adapter->blocking = blocking;
		ret = kthread_queue_work(&adapter->cb_event_ssr_worker, &adapter->cb_ssr_work);
		if (!ret)
			HFI_AD_WARN("failed to queue ssr end work\n");

		/* This event callback is in non ISR context so blocing is fine */
		if (blocking)
			kthread_flush_work(&adapter->cb_ssr_work);

		atomic_set(&adapter->ssr_in_progress, 0);
		HFI_AD_DEBUG("SSR completed successfully\n");
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

void _hfi_clear_buffer(struct hfi_cmdbuf_t *buffer)
{
	if (!buffer) {
		HFI_AD_ERROR("invalid params\n");
		return;
	}

	if (!buffer->pool) {
		HFI_AD_ERROR("invalid buffer pool\n");
		return;
	}

	buffer->cmd_type = 0;
	buffer->obj_id = 0;
	buffer->size = 0;
	atomic_set(&buffer->pool->available, 1);
	atomic_set(&buffer->buffer_send_done, 0);
	buffer->virtq_type = HFI_VIRTQUEUE_TYPE_MAX;

	HFI_AD_DEBUG("done clearing buffer -- requested by %pS buff:%p\n",
		__builtin_return_address(0), buffer);
}

struct hfi_adapter_t *hfi_adapter_init(int instance)
{
	struct hfi_adapter_t *hfi_host;
	struct hfi_core_open_params open_params;
	struct hfi_core_cb_ops *cb_ops;
	struct hfi_core_session *hfi_handle;
	struct hfi_buffer_pool *pool, *link;
	int i;

	hfi_host = kmalloc(sizeof(struct hfi_adapter_t), GFP_KERNEL);
	if (!hfi_host) {
		HFI_AD_ERROR("failed to allocate memory for adapter\n");
		return NULL;
	}

	cb_ops = kmalloc(sizeof(struct hfi_core_cb_ops), GFP_KERNEL);
	if (!cb_ops) {
		HFI_AD_ERROR("failed to allocate memory for cb_ops\n");
		kfree(hfi_host);
		return NULL;
	}

	cb_ops->hfi_cb_fn = &callback_function_hfi;
	cb_ops->cb_data = hfi_host;

	open_params.client_id = HFI_CORE_CLIENT_ID_0;
	open_params.ops = cb_ops;

	hfi_handle = hfi_core_open_session(&open_params);
	if (!hfi_handle) {
		HFI_AD_ERROR("failed to open hfi core session\n");
		goto fail;
	}

	/* Initialize hfi_adapter_t after core session is created */
	hfi_host->sde_or_vm_instance = instance;
	INIT_LIST_HEAD(&hfi_host->client_list);
	hfi_host->cb_ops = cb_ops;
	hfi_host->session = hfi_handle;

	/* Pre initialize work queues */
	for (i = 0; i < HFI_ADAPTER_WORK_QUEUE_SIZE; i++) {
		kthread_init_work(&hfi_host->cb_cmd_buf_work[i].work, _process_cb_cmd_buf_work);
		hfi_host->cb_cmd_buf_work[i].host = hfi_host;
		hfi_host->cb_cmd_buf_work[i].index = i;
	}
	kthread_init_worker(&hfi_host->cb_event_worker);
	hfi_host->cb_event_worker_thread = kthread_run(kthread_worker_fn,
			&hfi_host->cb_event_worker, "adapter_cb_event_thread");

	if (IS_ERR(hfi_host->cb_event_worker_thread)) {
		HFI_AD_ERROR("failed to create adapter_cb_thread\n");
		goto fail;
	}

	kthread_init_work(&hfi_host->cb_ssr_work, _process_cb_ssr_work);
	kthread_init_worker(&hfi_host->cb_event_ssr_worker);
	hfi_host->cb_event_worker_ssr_thread = kthread_run(kthread_worker_fn,
			&hfi_host->cb_event_ssr_worker, "adapter_cb_event_ssr_thread");

	if (IS_ERR(hfi_host->cb_event_worker_ssr_thread)) {
		HFI_AD_ERROR("failed to create adapter_cb_thread\n");
		goto fail;
	}

	idr_init(&hfi_host->client_ids);
	spin_lock_init(&hfi_host->packet_id_lock);
	mutex_init(&hfi_host->hfi_adapter_cmd_buf_list_lock);

	atomic_set(&hfi_host->ssr_in_progress, 0);

	/* Initialize buffers */
	pool = kmalloc(sizeof(struct hfi_buffer_pool), GFP_KERNEL);
	if (!pool) {
		HFI_AD_ERROR("failed to allocate memory for buffer pool\n");
		goto fail;
	}

	INIT_LIST_HEAD(&pool->node);
	INIT_LIST_HEAD(&pool->buffer_t.node);
	mutex_init(&pool->lock);
	mutex_init(&pool->buffer_t.lock);
	pool->buffer_t.pool = pool;
	_hfi_clear_buffer(&pool->buffer_t);

	for (i = 0; i < MAX_POOL_SIZE; i++) {
		link = kmalloc(sizeof(struct hfi_buffer_pool), GFP_KERNEL);
		if (!link) {
			HFI_AD_ERROR("failed to allocate memory for buffer pool\n");
			goto pool_fail;
		}
		INIT_LIST_HEAD(&link->buffer_t.node);
		list_add_tail(&link->node, &pool->node);
		atomic_set(&link->available, 1);
		mutex_init(&link->lock);
		mutex_init(&link->buffer_t.lock);
		link->buffer_t.pool = link;
		_hfi_clear_buffer(&link->buffer_t);

		/* Debug information */
		 HFI_AD_DEBUG("hfi buffer address = %p\n", &link->buffer_t);
	}

	hfi_host->pool = pool;

	return hfi_host;

pool_fail:
	kfree(pool);
fail:
	kfree(cb_ops);
	kfree(hfi_host);
	return NULL;
}

int hfi_adapter_client_register(struct hfi_adapter_t *host, struct hfi_client_t *ctx)
{
	if (!ctx->process_cmd_buf) {
		HFI_AD_ERROR("invalid client callback function pointer\n");
		return -EINVAL;
	}

	if (!host) {
		HFI_AD_ERROR("invalid host pointer\n");
		return -EINVAL;
	}

	/* Client can have multiple command buffers of different unique ID's */
	INIT_LIST_HEAD(&ctx->cmd_buf_list);

	/* Initialize client's packet litener list */
	INIT_LIST_HEAD(&ctx->packet_listeners.list_ptr);

	ctx->host = host;
	list_add_tail(&ctx->node, &host->client_list);

	ctx->client_id = idr_alloc(&host->client_ids, ctx, 1, 0, GFP_KERNEL);
	if (ctx->client_id < 0) {
		HFI_AD_ERROR("failed to get id for client\n");
		return -HFI_ERROR;
	}

	mutex_init(&ctx->lock);

	return 0;
}

static struct hfi_cmdbuf_t *_hfi_adapter_get_cmd_buf_helper(struct hfi_client_t *ctx,
		u32 obj_id, u32 cmdbuf_type)
{
	struct hfi_cmdbuf_t *buffer;
	struct hfi_core_cmds_buf_desc *buff_desc;
	struct hfi_cmd_buff_hdl buff_handle;
	struct hfi_header_info header_info;
	struct hfi_buffer_pool *pool;
	struct hfi_adapter_t *adapter;
	int ret = 0;
	static u32 counter;
	int failed_loop = 0;

	if (!ctx) {
		HFI_AD_ERROR("invalid client callback function pointer\n");
		return NULL;
	}
	adapter = ctx->host;

	pool = get_avail_buffer(adapter);
	if (!pool) {
		HFI_AD_ERROR("failed to get available buffer pool\n");
		return NULL;
	}

	buff_desc = &pool->buffer_t.buf;

	if (!buff_desc) {
		HFI_AD_ERROR("failed to allocate memory for buffer descriptor\n");
		goto error;
	}

	buff_desc->prio_info = HFI_CORE_PRIO_0;

	counter++;
	do {
		if (atomic_read(&adapter->ssr_in_progress))
			break;
		ret = hfi_core_cmds_tx_buf_get(adapter->session, buff_desc);
		if (ret) {
			failed_loop++;
			HFI_AD_ERROR("failed to get tx buff counter:%d retry:%d ret:%d\n",
				counter, failed_loop, ret);
			usleep_range(MIN_USLEEP_RANGE, MAX_USLEEP_RANGE);
		}
	} while (ret && failed_loop < GET_BUF_RETRY);

	if (ret) {
		HFI_AD_ERROR("failed to get tx buffer from hfi core error code: %d", ret);
		goto error;
	}

	buffer = &pool->buffer_t;

	if (!buffer) {
		HFI_AD_ERROR("failed to allocate memory for adapter command buffer\n");
		/* If adapter command buffer allocation fails, release VIRTIO buffer */
		ret = hfi_core_release_tx_buffer(adapter->session, &buff_desc, 1);
		if (ret)
			HFI_AD_ERROR("failed to release buffer back to virtio queue\n");
		goto error;
	}

	buffer->is_released = false;

	/* Populate structs for HFI Packer */
	buff_handle.cmd_buffer = buff_desc->pbuf_vaddr;
	buff_handle.size = buff_desc->size;

	header_info.num_packets = 0;
	header_info.cmd_buff_type = hfi_cmd_type_map[cmdbuf_type];
	header_info.object_id = obj_id;
	header_info.header_id = _create_buffer_id(ctx->client_id);

	if (!atomic_read(&adapter->ssr_in_progress)) {
		ret = hfi_create_header(&buff_handle, &header_info);
		if (ret) {
			HFI_AD_ERROR("failed to create buffer header\n");
			ret = hfi_core_release_tx_buffer(adapter->session, &buff_desc, 1);
			if (ret)
				HFI_AD_ERROR("failed to release buffer back to virtio queue\n");
			goto error;
		}
	}

	/* Populate adapter buffer structure members */
	buffer->cmd_type = cmdbuf_type;
	buffer->unique_id = header_info.header_id;
	buffer->obj_id = obj_id;
	buffer->size = 32;
	buffer->ctx = ctx;
	buffer->virtq_type = HFI_VIRTQUEUE_TYPE_TX;

	return buffer;

error:
	atomic_set(&pool->available, 1);
	return NULL;
}

struct hfi_cmdbuf_t *hfi_adapter_get_cmd_buf(struct hfi_client_t *ctx, u32 obj_id, u32 cmdbuf_type)
{
	struct hfi_cmdbuf_t *buffer;

	buffer = _hfi_adapter_get_cmd_buf_helper(ctx, obj_id, cmdbuf_type);

	if (!buffer) {
		HFI_AD_ERROR("failed to get command buffer\n");
		return NULL;
	}

	/* For new (non-chained) buffer's, initialize chain list */
	INIT_LIST_HEAD(&buffer->cmd_buf_chain);

	/* Add buffer to client context */
	mutex_lock(&ctx->lock);
	list_del_init(&buffer->node);
	list_add_tail(&buffer->node, &ctx->cmd_buf_list);
	mutex_unlock(&ctx->lock);

	return buffer;
}

static struct hfi_cmdbuf_t *_chain_new_buffer(struct hfi_cmdbuf_t *buffer_head)
{
	struct hfi_cmdbuf_t *buffer;

	buffer = _hfi_adapter_get_cmd_buf_helper(buffer_head->ctx, buffer_head->obj_id,
			buffer_head->cmd_type);

	if (!buffer) {
		HFI_AD_ERROR("failed to chain command buffer\n");
		return NULL;
	}

	/* For chained buffer's, add to existing cmd_buf_chain */
	list_add_tail(&buffer->cmd_buf_chain, &buffer_head->cmd_buf_chain);

	return buffer;
}

static struct hfi_cmdbuf_t *_check_attached_buffer(struct hfi_cmdbuf_t *cmd_buf,
		enum hfi_payload_type hfi_payload_type, u32 size)
{
	struct hfi_adapter_t *host;
	struct hfi_cmdbuf_t *current_buffer = cmd_buf;

	if (!cmd_buf || !cmd_buf->ctx) {
		HFI_AD_ERROR("invalid command buffer\n");
		return NULL;
	}

	host = cmd_buf->ctx->host;
	if (!host)
		return NULL;

	mutex_lock(&host->hfi_adapter_cmd_buf_list_lock);
	u32 available_buff_size = cmd_buf->buf.size - cmd_buf->size;
	mutex_unlock(&host->hfi_adapter_cmd_buf_list_lock);

	/* 32 bytes for packet header */
	u32 packet_size = 32;

	/* If we have a payload, add the size of the payload to packet size */
	if (hfi_payload_type != HFI_PAYLOAD_TYPE_NONE)
		packet_size += size;

	mutex_lock(&host->hfi_adapter_cmd_buf_list_lock);
	/* If there is a chained buffer, use the tail */
	if (!list_empty(&cmd_buf->cmd_buf_chain)) {
		current_buffer = list_last_entry(&cmd_buf->cmd_buf_chain, struct hfi_cmdbuf_t,
				cmd_buf_chain);
		available_buff_size = current_buffer->buf.size - current_buffer->size;
		HFI_AD_DEBUG("found a chained buffer, using it as current buffer\n");

		if (!current_buffer) {
			HFI_AD_ERROR("failed to get chained buffer tail\n");
			mutex_unlock(&host->hfi_adapter_cmd_buf_list_lock);
			return NULL;
		}
	}
	mutex_unlock(&host->hfi_adapter_cmd_buf_list_lock);

	/* Validate size, if not available then chain new buffer */
	if (available_buff_size < packet_size)
		current_buffer = _chain_new_buffer(cmd_buf);

	if (!current_buffer) {
		HFI_AD_ERROR("failed to chain command buffer\n");
		return NULL;
	}

	current_buffer->size += packet_size;

	return current_buffer;
}

static u32 _hfi_adapter_add_prop_helper(struct hfi_cmdbuf_t *cmd_buf, u32 cmd, u32 object_id,
		enum hfi_payload_type hfi_payload_type, void *payload, u32 size, u32 flags,
		u32 cnt, u32 *packet_id)
{
	struct hfi_cmd_buff_hdl buff_handle;
	struct hfi_packet_info packet_info;
	unsigned long lock_flags;
	int rc = 0;

	if (hfi_payload_type != HFI_PAYLOAD_TYPE_NONE && !payload) {
		HFI_AD_ERROR("payload not provided for cmd:0x%x type:%d\n",
			cmd, hfi_payload_type);
		return -EINVAL;
	} else if (hfi_payload_type == HFI_PAYLOAD_TYPE_NONE && payload) {
		HFI_AD_WARN("unexpected packet payload for cmd:0x%x\n", cmd);
	}

	/*
	 * if SSR is in progress, cannot queue buffer to hfi core,
	 * so do not create packets.
	 */
	if (atomic_read(&cmd_buf->ctx->host->ssr_in_progress))
		return rc;

	/* Populate HFI packer structs */
	buff_handle.cmd_buffer = cmd_buf->buf.pbuf_vaddr;
	buff_handle.size = cmd_buf->buf.size;

	memset(&packet_info, 0, sizeof(struct hfi_packet_info));
	packet_info.cmd = cmd;
	packet_info.id = object_id;
	packet_info.flags = flags;
	spin_lock_irqsave(&cmd_buf->ctx->host->packet_id_lock, lock_flags);
	packet_info.packet_id = _generate_sequential_packet_id();
	spin_unlock_irqrestore(&cmd_buf->ctx->host->packet_id_lock, lock_flags);
	packet_info.payload_type = (enum hfi_packet_payload_type) hfi_payload_type;
	packet_info.payload_size = size;
	packet_info.payload_ptr = payload;

	if (hfi_payload_type == HFI_PAYLOAD_TYPE_NONE || cnt > 0)
		rc = hfi_create_packet_header(&buff_handle, &packet_info);
	else
		rc = hfi_create_full_packet(&buff_handle, &packet_info);

	if (rc) {
		HFI_AD_ERROR("failed to create hfi packet. error code = %d\n", rc);
		return rc;
	}

	if (cnt != 0) {
		/* Append the key value pairs */
		rc = hfi_append_packet_with_kv_pairs(&buff_handle, cmd,
				(enum hfi_packet_payload_type) hfi_payload_type, 0,
				(struct hfi_kv_info *)payload, cnt, size);
		if (rc) {
			HFI_AD_ERROR("failed to append kv pairs. error code = %d\n", rc);
			return rc;
		}
	}

	*packet_id = packet_info.packet_id;

	return rc;
}

int hfi_adapter_add_set_property(struct hfi_client_t *ctx, struct hfi_cmdbuf_t *cmd_buf, u32 cmd,
		u32 object_id, enum hfi_payload_type hfi_payload_type, void *payload, u32 size,
		u32 flags)
{
	struct hfi_cmdbuf_t *current_buffer = cmd_buf;
	u32 packet_id;
	int rc = 0;

	if (!ctx) {
		HFI_AD_ERROR("invalid client\n");
		return -EINVAL;
	}

	current_buffer = _check_attached_buffer(cmd_buf, hfi_payload_type, size);
	if (!current_buffer)
		return -EINVAL;

	rc = _hfi_adapter_add_prop_helper(current_buffer, cmd, object_id, hfi_payload_type,
			payload, size, flags, 0, &packet_id);

	return rc;
}

int hfi_adapter_add_get_property(struct hfi_client_t *ctx, struct hfi_cmdbuf_t *cmd_buf,
		u32 cmd_id, u32 obj_id, enum hfi_payload_type hfi_payload_type,
		void *payload, u32 size, struct hfi_prop_listener *listener, u32 flags)
{
	struct hfi_cmdbuf_t *current_buffer = cmd_buf;
	struct hfi_client_t *buff_client_ctx;
	u32 packet_id;
	int rc = 0;

	if (!ctx || !cmd_buf) {
		HFI_AD_ERROR("invalid client\n");
		return -EINVAL;
	}

	current_buffer = _check_attached_buffer(cmd_buf, hfi_payload_type, size);
	if (!current_buffer)
		return -EINVAL;

	rc = _hfi_adapter_add_prop_helper(current_buffer, cmd_id, obj_id, hfi_payload_type,
			payload, size, flags, 0, &packet_id);
	if (rc) {
		HFI_AD_ERROR("failed to populate buffer packet with cmd:0x%x\n", cmd_id);
		return rc;
	}

	buff_client_ctx = cmd_buf->ctx;

	/* Create new listener_list structure to insert. */
	struct listener_list *listener_entry = kmalloc(sizeof(struct listener_list), GFP_KERNEL);

	if (!listener_entry) {
		HFI_AD_ERROR("failed to allocate memory for listener_entry\n");
		return -ENOMEM;
	}

	listener_entry->packet_id = packet_id;
	listener_entry->listener_obj = listener;

	/* Add listener based on packet obj_id  */
	list_add_tail(&listener_entry->list_ptr,
			&buff_client_ctx->packet_listeners.list_ptr);

	return rc;
}

int hfi_adapter_add_prop_array(struct hfi_client_t *ctx, struct hfi_cmdbuf_t *cmd_buf, u32 cmd,
		u32 object_id, enum hfi_payload_type payload_type,
		struct hfi_kv_pairs *payload, u32 cnt, u32 size)
{
	struct hfi_cmdbuf_t *current_buffer = cmd_buf;
	u32 packet_id;
	int rc = 0;

	if (!ctx) {
		HFI_AD_ERROR("invalid client\n");
		return -EINVAL;
	}

	if (!payload) {
		HFI_AD_ERROR("payload not provided\n");
		return -EINVAL;
	}

	if (payload_type == HFI_PAYLOAD_TYPE_NONE || !size || !cnt) {
		HFI_AD_ERROR("invalid payload parameters\n");
		return -EINVAL;
	}

	current_buffer = _check_attached_buffer(cmd_buf, payload_type, size);
	if (!current_buffer)
		return -EINVAL;

	rc = _hfi_adapter_add_prop_helper(current_buffer, cmd, object_id, payload_type,
			payload, size, HFI_HOST_FLAGS_NON_DISCARDABLE, cnt, &packet_id);


	return rc;
}

static void _release_tx_buffers(struct hfi_cmdbuf_t *cmd_buf)
{
	struct list_head *pos, *updated_pos;
	struct hfi_cmdbuf_t *buf_entry;
	struct hfi_client_t *ctx;
	int i = 0;
	struct hfi_core_cmds_buf_desc *buff_arr[MAX_BUFFERS];

	if (!cmd_buf) {
		HFI_AD_ERROR("invalid params\n");
		return;
	}

	ctx = cmd_buf->ctx;
	if (!ctx) {
		HFI_AD_ERROR("no valid client for cmd_buf:%p\n", cmd_buf);
		return;
	}

	mutex_lock(&ctx->lock);

	buff_arr[i++] = &cmd_buf->buf;

	if (!list_empty(&cmd_buf->cmd_buf_chain)) {
		list_for_each_prev_safe(pos, updated_pos, &cmd_buf->cmd_buf_chain) {
			buf_entry = list_entry(pos, struct hfi_cmdbuf_t, cmd_buf_chain);
			buff_arr[i++] = &buf_entry->buf;
			if (buf_entry->pool)
				_hfi_clear_buffer(buf_entry);
			list_del(pos);
		}
	}

	if (!cmd_buf->is_released)
		hfi_core_release_tx_buffer(cmd_buf->ctx->host->session, buff_arr, i);

	list_del_init(&cmd_buf->node);
	mutex_unlock(&ctx->lock);
	_hfi_clear_buffer(cmd_buf);
}

int hfi_adapter_set_cmd_buf(struct hfi_client_t *ctx, struct hfi_cmdbuf_t *cmd_buf)
{
	u32 num_buffers = 1;
	struct hfi_core_cmds_buf_desc *buff_arr[MAX_BUFFERS];
	int rc = 0;
	struct hfi_adapter_t *host;
	struct hfi_cmdbuf_t *buf_entry;
	u32 i = 1;

	if (!cmd_buf || !cmd_buf->ctx || !ctx) {
		HFI_AD_ERROR("Invalid client ctx\n");
		return -EINVAL;
	}

	if (cmd_buf->virtq_type != HFI_VIRTQUEUE_TYPE_TX) {
		HFI_AD_ERROR("invalid virtqueue type %d\n", cmd_buf->virtq_type);
		return -EINVAL;
	}

	host = cmd_buf->ctx->host;
	if (!host)
		return -EINVAL;

	/* Append the number of chained buffers */
	struct list_head *pos;
	list_for_each(pos, &cmd_buf->cmd_buf_chain)
		num_buffers++;

	HFI_AD_DEBUG("from %pS\n", __builtin_return_address(0));

	buff_arr[0] = &cmd_buf->buf;

	list_for_each(pos, &cmd_buf->cmd_buf_chain) {
		buf_entry = list_entry(pos, struct hfi_cmdbuf_t, cmd_buf_chain);
		if (buf_entry)
			buff_arr[i++] = &buf_entry->buf;
	}

	u32 host_flags = HFI_CORE_SET_FLAGS_TRIGGER_IPC;

	if (!atomic_read(&host->ssr_in_progress)) {
		rc = hfi_core_cmds_tx_buf_send(cmd_buf->ctx->host->session,
				buff_arr, num_buffers, host_flags);
		if (rc)
			HFI_AD_ERROR("failed to send tx buffer. error code = %d\n", rc);
	}

	mutex_lock(&host->hfi_adapter_cmd_buf_list_lock);
	cmd_buf->is_released = true;
	_release_tx_buffers(cmd_buf);
	mutex_unlock(&host->hfi_adapter_cmd_buf_list_lock);

	return rc;
}

int hfi_adapter_set_cmd_buf_blocking(struct hfi_client_t *ctx, struct hfi_cmdbuf_t *cmd_buf)
{
	struct hfi_adapter_t *host;
	int rc;
	bool response_ack = false;
	u32 wait_count = 0;
	struct list_head *pos;
	struct hfi_cmdbuf_t *buf_entry;
	struct hfi_core_cmds_buf_desc *buff_arr[MAX_BUFFERS];
	u32 num_buffers = 1;
	u32 i = 0;

	if (!cmd_buf || !cmd_buf->ctx || !ctx) {
		HFI_AD_ERROR("Invalid client ctx\n");
		return -EINVAL;
	}

	if (cmd_buf->virtq_type != HFI_VIRTQUEUE_TYPE_TX) {
		HFI_AD_ERROR("invalid virtqueue type %d\n", cmd_buf->virtq_type);
		return -EINVAL;
	}

	host = cmd_buf->ctx->host;
	if (!host)
		return -EINVAL;

	/* Append the number of chained buffers */
	list_for_each(pos, &cmd_buf->cmd_buf_chain)
		num_buffers++;

	HFI_AD_DEBUG("from %pS\n", __builtin_return_address(0));

	buff_arr[i++] = &cmd_buf->buf;
	list_for_each(pos, &cmd_buf->cmd_buf_chain) {
		buf_entry = list_entry(pos, struct hfi_cmdbuf_t, cmd_buf_chain);
		if (buf_entry)
			buff_arr[i++] = &buf_entry->buf;
	}

	u32 host_flags = HFI_CORE_SET_FLAGS_TRIGGER_IPC;

	if (!atomic_read(&host->ssr_in_progress)) {
		rc = hfi_core_cmds_tx_buf_send(cmd_buf->ctx->host->session,
				buff_arr, num_buffers, host_flags);
		HFI_AD_DEBUG("from %pS: host_flags:0x%x\n",
			__builtin_return_address(0), host_flags);
		if (rc) {
			HFI_AD_ERROR("failed to send tx buffer. error code = %d\n", rc);
			return rc;
		}
	}

	HFI_AD_DEBUG("[info] tx buffer sent\n");

	atomic_set(&cmd_buf->waiting_for_rsp, 1);
	do {
		if (atomic_read(&host->ssr_in_progress))
			break;
		usleep_range(HFI_APADTER_STEP_US, HFI_APADTER_STEP_US + 10);
		if (wait_count++ > MAX_TRY_COUNT) {
			HFI_AD_ERROR("set_cmd_buf_blocking wait timed-out\n");
			rc = hfi_core_notify_rsp_timeout(host->session);
			atomic_set(&cmd_buf->waiting_for_rsp, 0);
			rc = -ETIMEDOUT;
			break;
		}
		response_ack = atomic_read(&cmd_buf->buffer_send_done);
		HFI_AD_INFO("response_ack = 0x%08X\n", response_ack);
	} while (!response_ack);
	atomic_set(&cmd_buf->waiting_for_rsp, 0);

	if (!response_ack)
		HFI_AD_ERROR("timed out waiting for response_ack for tx!\n");
	else
		HFI_AD_DEBUG("[info] buffer response received after %d ms\n", wait_count);

	mutex_lock(&host->hfi_adapter_cmd_buf_list_lock);
	cmd_buf->is_released = true;
	_release_tx_buffers(cmd_buf);
	mutex_unlock(&host->hfi_adapter_cmd_buf_list_lock);

	return rc;
}

int hfi_adapter_unpack_cmd_buf(struct hfi_client_t *ctx, struct hfi_cmdbuf_t *cmd_buf)
{
	struct hfi_prop_listener *listener = NULL;
	struct listener_list *listener_entry = NULL;
	struct hfi_cmd_buff_hdl buff_handle;
	struct hfi_header_info header_info;
	struct hfi_packet_info packet_info;
	struct list_head *pos = NULL;
	struct hfi_cmdbuf_t *buf_entry = NULL;
	struct list_head *updated_pos = NULL;
	u32 num_packets = 0;
	int i;
	int rc = 0;
	int ret = 0;

	if (!ctx || !cmd_buf || !ctx->host) {
		HFI_AD_ERROR("invalid param\n");
		return -EINVAL;
	}

	if (cmd_buf->virtq_type != HFI_VIRTQUEUE_TYPE_RX) {
		HFI_AD_ERROR("invalid virtqueue type %d\n", cmd_buf->virtq_type);
		return -EINVAL;
	}

	/* Request header info to obtain the number of packets available in the buffer */
	buff_handle.cmd_buffer = cmd_buf->buf.pbuf_vaddr;
	buff_handle.size = cmd_buf->buf.size;

	rc = hfi_unpacker_get_header_info(&buff_handle, &header_info);

	if (rc) {
		HFI_AD_ERROR("failed to unpack command buffer header info\n");
		return rc;
	}

	num_packets = header_info.num_packets;

	for (i = 0; i < num_packets; i++) {
		memset(&packet_info, 0, sizeof(struct hfi_packet_info));
		/* Request packet info for packet (i+1) since packets are 1-indexed */
		rc = hfi_unpacker_get_packet_info(&buff_handle, i+1, &packet_info);
		if (rc) {
			HFI_AD_ERROR("failed to get packet info for packet %d\n", i+1);
			continue;
		}

		/* Find the client's listener attached to the packet-id */
		list_for_each(pos, &ctx->packet_listeners.list_ptr) {
			listener_entry = list_entry(pos, struct listener_list, list_ptr);
			if (!listener_entry)
				continue;

			if (!listener_entry->listener_obj) {
				HFI_AD_ERROR("[warning] no listener's attached\n");
				ret = -HFI_ERROR;
				continue;
			}

			/* If packet_id's match for the response packet(s), invoke listener */
			if (packet_info.packet_id == listener_entry->packet_id) {
				listener = (struct hfi_prop_listener *)
					(listener_entry->listener_obj);
				if (!listener)
					continue;
				listener->hfi_prop_handler(packet_info.id,
						packet_info.cmd, packet_info.payload_ptr,
						packet_info.payload_size, listener);

				if (packet_info.flags != HFI_RX_FLAGS_NONE &&
						packet_info.flags != HFI_RX_FLAGS_SUCCESS) {
					HFI_AD_ERROR("response packet error. cmd:0x%x resp:0x%x\n",
							packet_info.cmd, packet_info.flags);
					ret = -HFI_ERROR;
					continue;
				}
			}
		}
	}

	/* Loop through clients list and if matching unique_id then release */
	mutex_lock(&ctx->host->hfi_adapter_cmd_buf_list_lock);
	list_for_each_safe(pos, updated_pos, &ctx->cmd_buf_list) {
		buf_entry = list_entry(pos, struct hfi_cmdbuf_t, node);
		if (!buf_entry)
			continue;

		if (buf_entry->unique_id == cmd_buf->unique_id) {
			HFI_AD_DEBUG("matched response buf 0x%x to original 0x%x at ktime:%llu\n",
					cmd_buf->unique_id, buf_entry->unique_id, ktime_get());
			atomic_inc(&buf_entry->buffer_send_done);
		}
	}
	mutex_unlock(&ctx->host->hfi_adapter_cmd_buf_list_lock);

	return ret;
}

int hfi_adapter_release_cmd_buf(struct hfi_client_t *ctx, struct hfi_cmdbuf_t *cmd_buf)
{
	struct list_head *pos, *updated_pos;
	struct hfi_cmdbuf_t *buf_entry;
	struct hfi_core_cmds_buf_desc *buff_arr[MAX_BUFFERS];
	int i = 0;
	int rc = 0;

	if (!cmd_buf || !ctx) {
		HFI_AD_ERROR("invalid param\n");
		return -EINVAL;
	}

	if (cmd_buf->virtq_type == HFI_VIRTQUEUE_TYPE_MAX) {
		HFI_AD_ERROR("invalid virtqueue type %d\n", cmd_buf->virtq_type);
		return -EINVAL;
	}

	mutex_lock(&cmd_buf->ctx->host->hfi_adapter_cmd_buf_list_lock);

	/* Release chained buffers */
	list_for_each_prev_safe(pos, updated_pos, &cmd_buf->cmd_buf_chain) {
		buf_entry = list_entry(pos, struct hfi_cmdbuf_t, node);
		list_del(pos);
		buff_arr[i++] = &buf_entry->buf;
		if (buf_entry->pool) {
			_hfi_clear_buffer(buf_entry);
			atomic_set(&cmd_buf->pool->available, 1);
		}
	}

	/* Remove from client's buffer list */
	list_for_each_safe(pos, updated_pos, &cmd_buf->ctx->cmd_buf_list) {
		buf_entry = list_entry(pos, struct hfi_cmdbuf_t, node);
		if (buf_entry == cmd_buf) {
			HFI_AD_ERROR("releasing buffer incorrectly\n");
			list_del(pos);
			buff_arr[i++] = &buf_entry->buf;
		}
	}

	if (i == 0)
		buff_arr[i++] = &cmd_buf->buf;

	HFI_AD_DEBUG("number of buffers to release = %d\n", i);
	if (cmd_buf->virtq_type == HFI_VIRTQUEUE_TYPE_RX)
		rc = hfi_core_release_rx_buffer(cmd_buf->ctx->host->session, buff_arr, i);
	else if (cmd_buf->virtq_type == HFI_VIRTQUEUE_TYPE_TX)
		rc = hfi_core_release_tx_buffer(cmd_buf->ctx->host->session, buff_arr, i);

	if (rc) {
		HFI_AD_ERROR("failed to release rx buffer(s)\n");
		mutex_unlock(&cmd_buf->ctx->host->hfi_adapter_cmd_buf_list_lock);
		return rc;
	}

	/* Free main buffer head */
	atomic_set(&cmd_buf->pool->available, 1);
	_hfi_clear_buffer(cmd_buf);

	mutex_unlock(&cmd_buf->ctx->host->hfi_adapter_cmd_buf_list_lock);

	return rc;
}

void hfi_adapter_deinit(struct hfi_client_t *ctx)
{
	struct list_head *pos, *updated_pos;
	struct hfi_cmdbuf_t *buf;
	int i = 0;

	if (!ctx || !ctx->host)
		return;

	mutex_lock(&ctx->host->hfi_adapter_cmd_buf_list_lock);
	if (!list_empty(&ctx->cmd_buf_list)) {
		list_for_each_safe(pos, updated_pos, &ctx->cmd_buf_list) {
			buf = list_entry(pos, struct hfi_cmdbuf_t, node);
			if (buf) {
				i++;
				_release_tx_buffers(buf);
			}
		}
	}
	mutex_unlock(&ctx->host->hfi_adapter_cmd_buf_list_lock);

	HFI_AD_DEBUG("Freeing %d buffers on close\n", i);
}

int hfi_adapter_buffer_alloc(struct hfi_client_t *ctx, struct hfi_shared_addr_map *addr_map)
{
	int ret = 0;

	if (!ctx) {
		HFI_AD_ERROR("invalid client\n");
		return -EINVAL;
	}

	if (!addr_map->size) {
		HFI_AD_ERROR("failed to get shared buffer size\n");
		return -EINVAL;
	}
	addr_map->aligned_size = ALIGN(addr_map->size, HFI_CORE_IOMMU_MAP_SIZE_ALIGNMENT);

	ret = hfi_core_allocate_shared_mem(&addr_map->alloc_info, addr_map->aligned_size,
		HFI_CORE_DMA_ALLOC_UNCACHE, HFI_CORE_MMAP_READ | HFI_CORE_MMAP_WRITE);
	if (ret) {
		HFI_AD_ERROR("failed to allocate shared buffer, ret: %d\n", ret);
		return ret;
	}

	addr_map->remote_addr = addr_map->alloc_info.mapped_iova;
	addr_map->local_addr = addr_map->alloc_info.cpu_va;

	if (!addr_map->remote_addr || !addr_map->local_addr) {
		HFI_AD_ERROR("failed to allocate shared buffer\n");
		return -EINVAL;
	}

	return ret;
}

int hfi_adapter_buffer_dealloc(struct hfi_client_t *ctx, struct hfi_shared_addr_map *addr_map)
{
	struct hfi_core_mem_alloc_info *alloc_info = &addr_map->alloc_info;
	int ret = 0;

	if (!ctx) {
		HFI_AD_ERROR("invalid client\n");
		return -EINVAL;
	}

	if (!addr_map->size) {
		HFI_AD_DEBUG("empty buf\n");
		return ret;
	}

	if (!alloc_info->mapped_iova || !alloc_info->cpu_va) {
		HFI_AD_ERROR("failed to get buffer mapping info\n");
		return -EINVAL;
	}

	ret = hfi_core_deallocate_shared_mem(alloc_info);
	if (ret)
		HFI_AD_ERROR("failed to deallocate shared buffer, ret: %d\n", ret);

	alloc_info->mapped_iova = 0;
	alloc_info->cpu_va = NULL;

	return ret;
}

int hfi_adapter_map_sg_table(struct hfi_client_t *ctx, struct sg_table *sgt, size_t size,
		unsigned long *mapped_iova)
{
	if (!ctx) {
		HFI_AD_ERROR("invalid client\n");
		return -EINVAL;
	}

	return hfi_core_map_sg_table(sgt, size, mapped_iova,
		HFI_CORE_MMAP_READ | HFI_CORE_MMAP_WRITE);
}

size_t hfi_adapter_get_shared_mem_allocated_size(struct hfi_client_t *ctx,
		struct hfi_shared_addr_map *addr_map)
{
	if (!ctx) {
		HFI_AD_ERROR("invalid client\n");
		return 0;
	}

	if (addr_map == NULL) {
		HFI_AD_ERROR("Invalid parameter, addr_map is NULL\n");
		return 0;
	}

	return addr_map->alloc_info.size_allocated;
}

int hfi_adapter_unmap_iova(struct hfi_client_t *ctx, unsigned long iova, size_t size)
{
	if (!ctx) {
		HFI_AD_ERROR("invalid client\n");
		return -EINVAL;
	}

	return hfi_core_unmap_iova(iova, size);
}

int hfi_adapter_release_all_cmd_bufs(struct hfi_client_t *client)
{
	struct list_head *pos, *updated_pos;
	struct hfi_cmdbuf_t *cmd_buf;
	struct hfi_adapter_t *host;
	int ret = 0;

	if (!client || !client->host) {
		HFI_AD_ERROR("invalid hfi_client\n");
		return -EINVAL;
	}

	HFI_AD_DEBUG("%s: client id: %d\n", __func__, client->client_id);
	host = client->host;

	/* Loop through command buffer list of the client */
	mutex_lock(&host->hfi_adapter_cmd_buf_list_lock);
	list_for_each_safe(pos, updated_pos, &client->cmd_buf_list) {
		cmd_buf = list_entry(pos, struct hfi_cmdbuf_t, node);
		if (!cmd_buf)
			continue;

		/* Unblock the client waiting on response from DCP */
		if (atomic_read(&cmd_buf->waiting_for_rsp)) {
			atomic_inc(&cmd_buf->buffer_send_done);
			continue;
		}
		ret = hfi_adapter_release_cmd_buf(client, cmd_buf);
		if (ret != 0) {
			HFI_AD_ERROR("failed to hfi_adapter_release_cmd_buf, ret: %d\n", ret);
			continue;
		}
	}
	mutex_unlock(&host->hfi_adapter_cmd_buf_list_lock);

	return 0;
}

#endif /* IS_ENABLED(CONFIG_QTI_HFI_CORE)*/
