// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__

#include "hfi_dbg.h"
#include "hfi_props.h"
#include "hfi_utils.h"

#define HFI_DBG_BASE_PROP_MAX_SIZE 1024

#define DEFAULT_PANIC		1
#define DBG_CTRL_STOP_FTRACE	BIT(0)
#define DBG_CTRL_PANIC_UNDERRUN	BIT(1)

struct hfi_dbg *hfi_dbg;

static u32 _hfi_dbg_read_prop(u32 hfi_prop, u32 *payload, u32 max_words)
{
	u32 read = 0;
	u32 prop_id = HFI_PROP_ID(hfi_prop);

	if (!hfi_dbg) {
		SDE_ERROR("invalid pointer to hfi_dbg\n");
		return read;
	}

	switch (prop_id) {
	case HFI_PROPERTY_DEBUG_REG_ALLOC:
		hfi_dbg->buff_map.reg_addr.size = payload[read++];
		break;
	case HFI_PROPERTY_DEBUG_EVT_LOG_ALLOC:
		hfi_dbg->buff_map.evt_log_addr.size = payload[read++];
		break;
	case HFI_PROPERTY_DEBUG_DBG_BUS_ALLOC:
		hfi_dbg->buff_map.dbg_bus_addr.size = payload[read++];
		break;
	case HFI_PROPERTY_DEBUG_STATE_ALLOC:
		hfi_dbg->buff_map.device_state_addr.size = payload[read++];
		break;
	default:
		SDE_ERROR("unknown property ID (%u)\n", prop_id);
	}

	return read;
}

static int hfi_dbg_parse_payload(void *payload, u32 size)
{
	struct hfi_util_kv_parser kv_parser;
	u32 prop, max_words, last_read = 0;
	u32 *value_ptr;
	int ret, read_sz;
	u32 read = 0;
	u32 *data = (u32 *)payload;
	u32 prop_count;

	prop_count = *data++;
	read++;

	read_sz = (sizeof(u32) * read);
	ret  = hfi_util_kv_parser_init(&kv_parser, size - read_sz, data);
	if (ret) {
		SDE_ERROR("failed to get int prop parser\n");
		return ret;
	}

	for (int i = 0; i < prop_count; i++) {
		ret = hfi_util_kv_parser_get_next(&kv_parser, last_read, &prop,
				&value_ptr, &max_words);
		if (ret) {
			SDE_ERROR("failed to get next prop %d\n", ret);
			return ret;
		}

		last_read = _hfi_dbg_read_prop(prop, value_ptr, max_words);
		if (!last_read) {
			SDE_ERROR("failed to get next prop\n");
			return ret;
		}
	}

	return ret;
}

static void hfi_dbg_property_handler(u32 display_id, u32 cmd_id,
		void *payload, u32 size, struct hfi_prop_listener *listener)
{
	if (!hfi_dbg) {
		SDE_ERROR("invalid object or listener from FW\n");
		return;
	}

	if (cmd_id == HFI_COMMAND_DEBUG_INIT && payload)
		hfi_dbg_parse_payload(payload, size);
	else
		SDE_ERROR("invalid payload or hfi command 0x%x\n", cmd_id);
}

int hfi_dbg_device_setup(struct hfi_kms *hfi_kms)
{
	struct hfi_cmdbuf_t *cmd_buf;
	int ret;
	u64 prop_val;
	struct hfi_prop_u64 prop_u64;

	if (!hfi_dbg || !hfi_kms) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	cmd_buf = hfi_adapter_get_cmd_buf(&hfi_kms->hfi_client,
			MSM_DRV_HFI_ID, HFI_CMDBUF_TYPE_GET_DEBUG_DATA);
	if (!cmd_buf) {
		SDE_ERROR("failed to get hfi command buffer\n");
		return -EINVAL;
	}

	if (hfi_dbg->buff_map.reg_addr.size && hfi_dbg->buff_map.reg_addr.remote_addr) {
		prop_val = (u64) hfi_dbg->buff_map.reg_addr.remote_addr;
		prop_u64.val_lo = HFI_VAL_L32(prop_val);
		prop_u64.val_hi = HFI_VAL_H32(prop_val);
		hfi_util_u32_prop_helper_add_prop(hfi_dbg->base_props, HFI_PROPERTY_DEBUG_REG_ADDR,
				HFI_VAL_U32_ARRAY, &prop_u64, sizeof(struct hfi_prop_u64));
	}

	if (hfi_dbg->buff_map.evt_log_addr.size && hfi_dbg->buff_map.evt_log_addr.remote_addr) {
		prop_val = (u64) hfi_dbg->buff_map.evt_log_addr.remote_addr;
		prop_u64.val_lo = HFI_VAL_L32(prop_val);
		prop_u64.val_hi = HFI_VAL_H32(prop_val);
		hfi_util_u32_prop_helper_add_prop(hfi_dbg->base_props,
			HFI_PROPERTY_DEBUG_EVT_LOG_ADDR, HFI_VAL_U32_ARRAY,
			&prop_u64, sizeof(struct hfi_prop_u64));
	}

	if (hfi_dbg->buff_map.dbg_bus_addr.size &&
		hfi_dbg->buff_map.dbg_bus_addr.remote_addr) {
		prop_val = (u64) hfi_dbg->buff_map.dbg_bus_addr.remote_addr;
		prop_u64.val_lo = HFI_VAL_L32(prop_val);
		prop_u64.val_hi = HFI_VAL_H32(prop_val);
		hfi_util_u32_prop_helper_add_prop(hfi_dbg->base_props,
			HFI_PROPERTY_DEBUG_DBG_BUS_ADDR, HFI_VAL_U32_ARRAY,
			&prop_u64, sizeof(struct hfi_prop_u64));
	}

	if (hfi_dbg->buff_map.device_state_addr.size &&
		hfi_dbg->buff_map.device_state_addr.remote_addr) {
		prop_val = (u64) hfi_dbg->buff_map.device_state_addr.remote_addr;
		prop_u64.val_lo = HFI_VAL_L32(prop_val);
		prop_u64.val_hi = HFI_VAL_H32(prop_val);
		hfi_util_u32_prop_helper_add_prop(hfi_dbg->base_props,
			HFI_PROPERTY_DEBUG_STATE_ADDR, HFI_VAL_U32_ARRAY,
			&prop_u64, sizeof(struct hfi_prop_u64));
	}

	ret = hfi_adapter_add_set_property(&hfi_kms->hfi_client,
			cmd_buf,
			HFI_COMMAND_DEBUG_SETUP,
			MSM_DRV_HFI_ID,
			HFI_PAYLOAD_TYPE_U32_ARRAY,
			hfi_util_u32_prop_helper_get_payload_addr(hfi_dbg->base_props),
			hfi_util_u32_prop_helper_get_size(hfi_dbg->base_props),
			HFI_HOST_FLAGS_NONE);

	ret = hfi_adapter_set_cmd_buf(&hfi_kms->hfi_client, cmd_buf);
	SDE_EVT32(MSM_DRV_HFI_ID, HFI_COMMAND_DEBUG_SETUP, ret, SDE_EVTLOG_FUNC_CASE1);
	if (ret) {
		SDE_ERROR("failed to send debug-init command\n");
		return ret;
	}

	return ret;
}

ssize_t hfi_devcoredump_read(char *buffer, loff_t offset, size_t count)
{
	ssize_t read_sz, evtlog_sz, total_sz;
	ssize_t copied = 0;
	ssize_t rd_buf_offset;
	ssize_t rd_buf_cpy, evtlog_cpy;

	if (!hfi_dbg || !hfi_dbg->base->evtlog || !hfi_dbg->base->evtlog->dumped_evtlog ||
		!hfi_dbg->base->read_buf)
		return 0;

	evtlog_sz = SDE_EVTLOG_ENTRY * SDE_EVTLOG_BUF_MAX;
	read_sz = hfi_dbg->base->buff_sz;
	total_sz = read_sz + evtlog_sz;
	hfi_dbg->base->is_dumped = true;

	/* Copy from evtlog if offset is within evtlog range */
	if (evtlog_sz > offset) {
		evtlog_cpy = min(count, (size_t)(evtlog_sz - offset));
		memcpy(buffer, hfi_dbg->base->evtlog->dumped_evtlog + offset,
			evtlog_cpy);
		copied += evtlog_cpy;
	}

	if ((total_sz > (offset + copied)) && copied < count) {
		rd_buf_offset = evtlog_sz < offset ? offset - evtlog_sz : 0;
		rd_buf_cpy = min(count - copied, (size_t)(read_sz - rd_buf_offset));

		memcpy(buffer + copied, hfi_dbg->base->read_buf + rd_buf_offset,
			rd_buf_cpy);
		copied += rd_buf_cpy;
	}

	return (offset < total_sz) ? copied : 0;
}

#if IS_ENABLED(CONFIG_QCOM_VA_MINIDUMP)
void hfi_dbg_add_va_region(void)
{
	if (hfi_dbg->buff_map.reg_addr.size)
		sde_mini_dump_add_va_region("reg_dump",
			hfi_dbg->buff_map.reg_addr.size,
			hfi_dbg->buff_map.reg_addr.local_addr);

	if (hfi_dbg->buff_map.evt_log_addr.size)
		sde_mini_dump_add_va_region("evtlog",
			hfi_dbg->buff_map.evt_log_addr.size,
			hfi_dbg->buff_map.evt_log_addr.local_addr);

	if (hfi_dbg->buff_map.dbg_bus_addr.size)
		sde_mini_dump_add_va_region("dbgbus_dump",
			hfi_dbg->buff_map.dbg_bus_addr.size,
			hfi_dbg->buff_map.dbg_bus_addr.local_addr);
}
#else
void hfi_dbg_add_va_region(void)
{

}
#endif

void _hfi_dump_buff(void __iomem *local_addr, u32 size, char *evt_type)
{
	u32 in_log;
	int sz_read = 0;
	char *token, *data_cpy, *dump_addr;
	const char *delim = "\n";

	if (!local_addr || !size)
		return;

	in_log = (hfi_dbg->base->dump_option & (SDE_DBG_DUMP_IN_LOG | SDE_DBG_DUMP_IN_LOG_LIMITED));

	pr_debug("reg_dump_flag=%d in_log=%d\n", hfi_dbg->base->dump_option, in_log);

	dump_addr = kvzalloc(size, GFP_KERNEL);
	if (!dump_addr) {
		SDE_ERROR("failed to reg dump memory\n");
		return;
	}

	memcpy(dump_addr, local_addr, size);
	data_cpy = dump_addr;
	if (in_log && data_cpy) {
		sz_read = 0;
		pr_info("===================%s================\n", evt_type);
		token = strsep(&data_cpy, delim);
		while (token && (sz_read < size)) {
			sz_read += strlen(token) + 1;
			pr_info("%s\n", token);
			token = strsep(&data_cpy, delim);
		}
		pr_info("\n");
	}
	kfree(data_cpy);
}

/**
 * _hfi_dump_all - dumps all registers, eventlogs, debugbus
 * @do_panic: whether to trigger a panic after dumping
 * @name: string indicating origin of dump
 * @dump_secure: flag to indicate dumping in secure-session
 * @dump_blk_mask: mask of all the hw blk-ids that has to be dumped
 */
static void _hfi_dump_all(bool do_panic, const char *name, bool dump_secure)
{
	ktime_t start, end;
	u32 in_dump;

	if (!hfi_dbg)
		return;

	in_dump = (hfi_dbg->base->dump_option);

	if (sde_evtlog_is_enabled(hfi_dbg->base->evtlog, SDE_EVTLOG_ALWAYS)) {
		sde_evtlog_dump_all(hfi_dbg->base->evtlog);
		_hfi_dump_buff(hfi_dbg->buff_map.evt_log_addr.local_addr,
			hfi_dbg->buff_map.evt_log_addr.size, "evtlog");
	}

	start = ktime_get();
	_hfi_dump_buff(hfi_dbg->buff_map.reg_addr.local_addr,
		hfi_dbg->buff_map.reg_addr.size, "reg_dump");
	end = ktime_get();

	dev_info(hfi_dbg->dev,
		"reg-dump logging time start_us:%llu, end_us:%llu , duration_us:%llu\n",
		ktime_to_us(start), ktime_to_us(end), ktime_us_delta(end, start));

	start = ktime_get();
	_hfi_dump_buff(hfi_dbg->buff_map.dbg_bus_addr.local_addr,
		hfi_dbg->buff_map.dbg_bus_addr.size, "dbgbus_dump");
	end = ktime_get();

	dev_info(hfi_dbg->dev,
			"debug-bus logging time start_us:%llu, end_us:%llu , duration_us:%llu\n",
			ktime_to_us(start), ktime_to_us(end), ktime_us_delta(end, start));

	if (do_panic && hfi_dbg->base->panic_on_err && (!in_dump))
		panic(name);
}

void hfi_dbg_dump(bool do_panic, const char *name, bool dump_secure, u64 dump_blk_mask)
{
	_hfi_dump_all(do_panic, name, dump_secure);
}

int hfi_dbg_init(struct device *dev, struct sde_dbg_base *dbg)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *ddev = platform_get_drvdata(pdev);
	struct msm_drm_private *priv = NULL;
	struct dentry *debugfs_root = NULL;
	struct hfi_cmdbuf_t *cmd_buf;
	struct sde_kms *kms;
	struct hfi_kms *hfi_kms;
	u32 offset_sz = 0, base_buff_sz = 0;

	if (!ddev || !ddev->dev_private) {
		SDE_ERROR("invalid drm device node\n");
		return -EINVAL;
	}
	priv = ddev->dev_private;
	debugfs_root = priv->debug_root;

	hfi_dbg = kvzalloc(sizeof(struct hfi_dbg), GFP_KERNEL);
	if (!hfi_dbg) {
		SDE_ERROR("failed to allocate hfi_dbg\n");
		return -EINVAL;
	}

	hfi_dbg->dev = dev;
	hfi_dbg->debugfs_root = debugfs_root;
	hfi_dbg->base = dbg;

	hfi_dbg->buff_map.reg_addr.size = 0;
	hfi_dbg->buff_map.evt_log_addr.size = 0;
	hfi_dbg->buff_map.dbg_bus_addr.size = 0;
	hfi_dbg->buff_map.device_state_addr.size = 0;

	dbg->hal_ops.dbg_dump[MSM_DISP_OP_HFI] = hfi_dbg_dump;
	dbg->hal_ops.devcoredump_read[MSM_DISP_OP_HFI] = hfi_devcoredump_read;
	dbg->hal_ops.add_minidump_va[MSM_DISP_OP_HFI] = hfi_dbg_add_va_region;

	kms = to_sde_kms(priv->kms);
	hfi_kms = to_hfi_kms(kms);

	hfi_dbg->base_props = hfi_util_u32_prop_helper_alloc(HFI_DBG_BASE_PROP_MAX_SIZE);
	if (IS_ERR(hfi_dbg->base_props)) {
		SDE_ERROR("failed to allocate memory for base prop collector\n");
		ret = PTR_ERR(hfi_dbg->base_props);
		goto free_kv;
	}

	cmd_buf = hfi_adapter_get_cmd_buf(&hfi_kms->hfi_client,
			MSM_DRV_HFI_ID, HFI_CMDBUF_TYPE_GET_DEBUG_DATA);
	if (!cmd_buf) {
		SDE_ERROR("failed to get hfi command buffer\n");
		return -EINVAL;
	}

	hfi_dbg->hfi_cb_obj.hfi_prop_handler = hfi_dbg_property_handler;
	ret = hfi_adapter_add_get_property(&hfi_kms->hfi_client, cmd_buf, HFI_COMMAND_DEBUG_INIT,
			MSM_DRV_HFI_ID, HFI_PAYLOAD_TYPE_NONE, NULL, 0,
			&hfi_dbg->hfi_cb_obj, HFI_HOST_FLAGS_RESPONSE_REQUIRED);
	if (ret) {
		SDE_ERROR("failed to add debug-init command\n");
		return ret;
	}

	SDE_EVT32(MSM_DRV_HFI_ID, HFI_COMMAND_DEBUG_INIT, SDE_EVTLOG_FUNC_CASE1);
	ret = hfi_adapter_set_cmd_buf_blocking(&hfi_kms->hfi_client, cmd_buf);
	SDE_EVT32(MSM_DRV_HFI_ID, HFI_COMMAND_DEBUG_INIT, ret, SDE_EVTLOG_FUNC_CASE2);
	if (ret) {
		SDE_ERROR("failed to send debug-init command\n");
		return ret;
	}

	base_buff_sz = hfi_dbg->buff_map.reg_addr.size + hfi_dbg->buff_map.evt_log_addr.size
		+ hfi_dbg->buff_map.dbg_bus_addr.size + hfi_dbg->buff_map.device_state_addr.size;

	hfi_dbg->base_buf_addr.size = base_buff_sz;
	ret = hfi_adapter_buffer_alloc(&hfi_kms->hfi_client, &hfi_dbg->base_buf_addr);
	if (ret) {
		SDE_ERROR("failed to allocate shared buffer, ret: %d\n", ret);
		return ret;
	}
	if (hfi_dbg->base_buf_addr.local_addr) {
		hfi_dbg->base->read_buf = hfi_dbg->base_buf_addr.local_addr;
		hfi_dbg->base->buff_sz = hfi_dbg->base_buf_addr.size;
	}

	if (hfi_dbg->buff_map.reg_addr.size) {
		hfi_dbg->buff_map.reg_addr.local_addr =
			hfi_dbg->base_buf_addr.local_addr + offset_sz;
		hfi_dbg->buff_map.reg_addr.remote_addr =
			hfi_dbg->base_buf_addr.remote_addr + offset_sz;
		offset_sz += hfi_dbg->buff_map.reg_addr.size;
	}

	if (hfi_dbg->buff_map.evt_log_addr.size) {
		hfi_dbg->buff_map.evt_log_addr.local_addr =
			hfi_dbg->base_buf_addr.local_addr + offset_sz;
		hfi_dbg->buff_map.evt_log_addr.remote_addr =
			hfi_dbg->base_buf_addr.remote_addr + offset_sz;
		offset_sz += hfi_dbg->buff_map.evt_log_addr.size;
	}

	if (hfi_dbg->buff_map.dbg_bus_addr.size) {
		hfi_dbg->buff_map.dbg_bus_addr.local_addr =
			hfi_dbg->base_buf_addr.local_addr + offset_sz;
		hfi_dbg->buff_map.dbg_bus_addr.remote_addr =
			hfi_dbg->base_buf_addr.remote_addr + offset_sz;
		offset_sz += hfi_dbg->buff_map.dbg_bus_addr.size;
	}

	if (hfi_dbg->buff_map.device_state_addr.size) {
		hfi_dbg->buff_map.device_state_addr.local_addr =
			hfi_dbg->base_buf_addr.local_addr + offset_sz;
		hfi_dbg->buff_map.device_state_addr.remote_addr =
			hfi_dbg->base_buf_addr.remote_addr + offset_sz;
		offset_sz += hfi_dbg->buff_map.device_state_addr.size;
	}

	ret = hfi_dbg_device_setup(hfi_kms);
	if (ret) {
		SDE_ERROR("failed to send debug-setup command\n");
		return ret;
	}
	return ret;

free_kv:
	kfree(hfi_dbg->base_props);
	return -EINVAL;
}

void hfi_dbg_destroy(void)
{
	struct device *dev;
	struct platform_device *pdev;
	struct drm_device *ddev;
	struct msm_drm_private *priv = NULL;
	struct sde_kms *kms;
	struct hfi_kms *hfi_kms;

	if (!hfi_dbg) {
		SDE_ERROR("hfi_dbg not initialized\n");
		return;
	}

	dev = hfi_dbg->dev;
	if (!dev) {
		SDE_ERROR("could not obtained device\n");
		return;
	}
	pdev = to_platform_device(dev);
	ddev = platform_get_drvdata(pdev);

	if (!ddev || !ddev->dev_private) {
		SDE_ERROR("invalid drm device node\n");
		return;
	}
	priv = ddev->dev_private;

	kms = to_sde_kms(priv->kms);
	if (!kms)
		return;

	hfi_kms = to_hfi_kms(kms);

	if (!hfi_dbg || !hfi_kms)
		return;

	hfi_dbg->buff_map.reg_addr.local_addr = NULL;
	hfi_dbg->buff_map.evt_log_addr.local_addr = NULL;
	hfi_dbg->buff_map.dbg_bus_addr.local_addr = NULL;
	hfi_dbg->buff_map.device_state_addr.local_addr = NULL;

	hfi_dbg->buff_map.reg_addr.remote_addr = 0;
	hfi_dbg->buff_map.evt_log_addr.remote_addr = 0;
	hfi_dbg->buff_map.dbg_bus_addr.remote_addr = 0;
	hfi_dbg->buff_map.device_state_addr.remote_addr = 0;

	hfi_dbg->base->read_buf = NULL;
	hfi_dbg->base->buff_sz = 0;

	hfi_adapter_buffer_dealloc(&hfi_kms->hfi_client, &hfi_dbg->buff_map.reg_addr);

	kfree(hfi_dbg);
}
