/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#ifndef SDE_DBG_H_
#define SDE_DBG_H_

#include <linux/stdarg.h>
#include <linux/debugfs.h>
#include <linux/list.h>
#include <soc/qcom/minidump.h>
#include <drm/drm_print.h>
#include "msm_drv.h"

/* select an uncommon hex value for the limiter */
#define SDE_EVTLOG_DATA_LIMITER	(0xC0DEBEEF)
#define SDE_EVTLOG_FUNC_ENTRY	0x1111
#define SDE_EVTLOG_FUNC_EXIT	0x2222
#define SDE_EVTLOG_FUNC_CASE1	0x3333
#define SDE_EVTLOG_FUNC_CASE2	0x4444
#define SDE_EVTLOG_FUNC_CASE3	0x5555
#define SDE_EVTLOG_FUNC_CASE4	0x6666
#define SDE_EVTLOG_FUNC_CASE5	0x7777
#define SDE_EVTLOG_FUNC_CASE6	0x8888
#define SDE_EVTLOG_FUNC_CASE7	0x9999
#define SDE_EVTLOG_FUNC_CASE8	0xaaaa
#define SDE_EVTLOG_FUNC_CASE9	0xbbbb
#define SDE_EVTLOG_FUNC_CASE10	0xcccc
#define SDE_EVTLOG_PANIC	0xdead
#define SDE_EVTLOG_FATAL	0xbad
#define SDE_EVTLOG_ERROR	0xebad

#define SDE_EVTLOG_H32(val) (val >> 32)
#define SDE_EVTLOG_L32(val) (val & 0xffffffff)

#define LUTDMA_DBG_NAME "reg_dma"

/* flags to enable the HW block dumping */
#define SDE_DBG_SDE		BIT(0)
#define SDE_DBG_RSC		BIT(1)
#define SDE_DBG_SID		BIT(2)
#define SDE_DBG_LUTDMA		BIT(3)
#define SDE_DBG_VBIF_RT		BIT(4)
#define SDE_DBG_DSI		BIT(5)
#define SDE_DBG_SWFUSE		BIT(6)

/* flags to enable the HW block debugbus dumping */
#define SDE_DBG_SDE_DBGBUS	BIT(12)
#define SDE_DBG_RSC_DBGBUS	BIT(13)
#define SDE_DBG_LUTDMA_DBGBUS	BIT(14)
#define SDE_DBG_VBIF_RT_DBGBUS	BIT(15)
#define SDE_DBG_DSI_DBGBUS	BIT(16)

/* mask to enable all the built-in register blocks */
#define SDE_DBG_BUILT_IN_ALL	0xffffff

/* keeping DP separate as DP specific clks needs to be enabled for accessing */
#define SDE_DBG_DP		BIT(24)
#define SDE_DBG_DP_DBGBUS	BIT(25)

#define SDE_DBG_DUMP_DATA_LIMITER (NULL)

#define SDE_DBG_BASE_MAX		10
#define RANGE_NAME_LEN		40
#define REG_BASE_NAME_LEN	80

enum sde_dbg_evtlog_flag {
	SDE_EVTLOG_CRITICAL = BIT(0),
	SDE_EVTLOG_IRQ = BIT(1),
	SDE_EVTLOG_VERBOSE = BIT(2),
	SDE_EVTLOG_EXTERNAL = BIT(3),
	SDE_EVTLOG_ALWAYS = -1
};

enum sde_dbg_dump_flag {
	SDE_DBG_DUMP_IN_LOG = BIT(0),
	SDE_DBG_DUMP_IN_MEM = BIT(1),
	SDE_DBG_DUMP_IN_LOG_LIMITED = BIT(2),
	SDE_DBG_DUMP_IN_COREDUMP = BIT(3),
};

enum sde_dbg_dump_context {
	SDE_DBG_DUMP_PROC_CTX,
	SDE_DBG_DUMP_IRQ_CTX,
	SDE_DBG_DUMP_CLK_ENABLED_CTX,
};

enum sde_dbg_data {
	SDE_DBG_REG_DUMP = 1,
	SDE_DBG_EVT_LOG,
	SDE_DBG_BUS,
	SDE_DBG_DEV_STATE,
	SDE_DEBUG_MAX,
};

/*
 * Set in_coredump as default mode. Any existing script which rely on
 * dump_mode to be in_mem should now explicitly run the cmd
 * "adb shell echo 2 > /sys/kernel/debug/dri/0/debug/reg_dump" before
 * doing the test cases.
 */
#define SDE_DBG_DEFAULT_DUMP_MODE	SDE_DBG_DUMP_IN_MEM

/*
 * Define blocks for register write logging.
 */
#define SDE_REG_LOG_DEFAULT  0
#define SDE_REG_LOG_NONE     1
#define SDE_REG_LOG_CDM      2
#define SDE_REG_LOG_DSPP     3
#define SDE_REG_LOG_INTF     4
#define SDE_REG_LOG_LM       5
#define SDE_REG_LOG_CTL      6
#define SDE_REG_LOG_PINGPONG 7
#define SDE_REG_LOG_SSPP     8
#define SDE_REG_LOG_WB       9
#define SDE_REG_LOG_TOP     10
#define SDE_REG_LOG_VBIF    11
#define SDE_REG_LOG_DSC     12
#define SDE_REG_LOG_ROT     13
#define SDE_REG_LOG_DS      14
#define SDE_REG_LOG_REGDMA  15
#define SDE_REG_LOG_UIDLE   16
#define SDE_REG_LOG_SID     16
#define SDE_REG_LOG_QDSS    17
/*
 * 0-32 are reserved for sde_reg_write due to log masks
 * Additional blocks are assigned from 33 to avoid conflict
 */
#define SDE_REG_LOG_RSCC    33

#define SDE_EVTLOG_DEFAULT_ENABLE (SDE_EVTLOG_CRITICAL | SDE_EVTLOG_IRQ | SDE_EVTLOG_EXTERNAL)

/*
 * evtlog will print this number of entries when it is called through
 * sysfs node or panic. This prevents kernel log from evtlog message
 * flood.
 */
#define SDE_EVTLOG_PRINT_ENTRY 1024

/*
 * evtlog keeps this number of entries in memory for debug purpose. This
 * number must be greater than print entry to prevent out of bound evtlog
 * entry array access.
 */

#if IS_ENABLED(CONFIG_DRM_MSM_LOW_MEM_FOOTPRINT)
#define SDE_EVTLOG_ENTRY	(SDE_EVTLOG_PRINT_ENTRY * 8)
#else
#define SDE_EVTLOG_ENTRY	(SDE_EVTLOG_PRINT_ENTRY * 32)
#endif /* IS_ENABLED(CONFIG_DRM_MSM_LOW_MEM_FOOTPRINT) */

#define SDE_EVTLOG_MAX_DATA 15
#define SDE_EVTLOG_BUF_MAX 512
#define SDE_EVTLOG_BUF_ALIGN 32


/**
 * struct sde_dbg_power_ctrl - Debug interface for controlling power state
 * @handle: Pointer to the power control handle
 * @client: Pointer to the client requesting power control
 * @enable_fn: Callback function to enable or disable power
 *              @handle: The power control handle
 *              @client: The requesting client
 *              @enable: Boolean flag to enable/disable power
 */
struct sde_dbg_power_ctrl {
	void *handle;
	void *client;
	int (*enable_fn)(void *handle, void *client, bool enable);
};

/**
 * struct sde_dbg_evtlog_log - Single event log entry for debug tracing
 * @time:      Timestamp of the event (in nanoseconds or system ticks)
 * @name:      Name of the function or module where the event occurred
 * @line:      Line number in the source code where the event was logged
 * @data:      Array of event-specific data values
 * @data_cnt:  Number of valid entries in the @data array
 * @pid:       Process ID associated with the event
 * @cpu:       CPU ID where the event was logged
 */

struct sde_dbg_evtlog_log {
	s64 time;
	const char *name;
	int line;
	u32 data[SDE_EVTLOG_MAX_DATA];
	u32 data_cnt;
	int pid;
	u8 cpu;
};


/**
 * struct sde_dbg_evtlog - Debug event log container for SDE
 * @logs:            Circular buffer of event log entries
 * @first:           Index of the first valid log entry
 * @last:            Atomic index of the last written log entry
 * @last_dump:       Index of the last dumped log entry
 * @curr:            Atomic index used for current log operations
 * @next:            Index for the next log entry to be written
 * @enable:          Flag indicating whether event logging is enabled
 * @dump_mode:       Mode used for dumping logs
 * @dumped_evtlog:   Pointer to the buffer holding the last dumped log data
 * @log_size:        Total number of log entries currently stored
 * @spin_lock:       Spinlock to protect access to the log buffer
 * @filter_list:     List of filters applied to event logging
 */
struct sde_dbg_evtlog {
	struct sde_dbg_evtlog_log logs[SDE_EVTLOG_ENTRY];
	u32 first;
	atomic_t last;
	u32 last_dump;
	atomic_t curr;
	u32 next;
	u32 enable;
	u32 dump_mode;
	char *dumped_evtlog;
	u32 log_size;
	spinlock_t spin_lock;
	struct list_head filter_list;
};

extern struct sde_dbg_evtlog *sde_dbg_base_evtlog;

/**
 * struct sde_dbg_reg_offset - tracking for start and end of region
 * @start: start offset
 * @end: end offset
 */
struct sde_dbg_reg_offset {
	u32 start;
	u32 end;
};

/**
 * struct sde_dbg_reg_range - register dumping named sub-range
 * @head: head of this node
 * @reg_dump: address for the mem dump
 * @range_name: name of this range
 * @offset: offsets for range to dump
 * @xin_id: client xin id
 */
struct sde_dbg_reg_range {
	struct list_head head;
	u32 *reg_dump;
	char range_name[RANGE_NAME_LEN];
	struct sde_dbg_reg_offset offset;
	uint32_t xin_id;
};

/**
 * struct sde_dbg_reg_base - register region base.
 *	may sub-ranges: sub-ranges are used for dumping
 *	or may not have sub-ranges: dumping is base -> max_offset
 * @reg_base_head: head of this node
 * @sub_range_list: head to the list with dump ranges
 * @name: register base name
 * @base: base pointer
 * @phys_addr: block physical address
 * @off: cached offset of region for manual register dumping
 * @cnt: cached range of region for manual register dumping
 * @max_offset: length of region
 * @buf: buffer used for manual register dumping
 * @buf_len:  buffer length used for manual register dumping
 * @reg_dump: address for the mem dump if no ranges used
 * @cb: callback for external dump function, null if not defined
 * @cb_ptr: private pointer to callback function
 * @blk_id: id indicate the HW block
 */
struct sde_dbg_reg_base {
	struct list_head reg_base_head;
	struct list_head sub_range_list;
	char name[REG_BASE_NAME_LEN];
	void __iomem *base;
	unsigned long phys_addr;
	size_t off;
	size_t cnt;
	size_t max_offset;
	char *buf;
	size_t buf_len;
	u32 *reg_dump;
	void (*cb)(void *ptr);
	void *cb_ptr;
	u64 blk_id;
};

/**
 * struct sde_debug_bus_entry - Entry for a debug bus test configuration
 * @wr_addr:     Write address for the debug bus
 * @rd_addr:     Read address for the debug bus
 * @block_id:    Starting block ID for the test
 * @block_id_max: Maximum block ID for the test range
 * @test_id:     Starting test ID for the test
 * @test_id_max: Maximum test ID for the test range
 * @analyzer:    Optional callback to analyze the read value
 *               @wr_addr: Write address used
 *               @block_id: Block ID tested
 *               @test_id: Test ID used
 *               @val: Value read from the debug bus
 */
struct sde_debug_bus_entry {
	u32 wr_addr;
	u32 rd_addr;
	u32 block_id;
	u32 block_id_max;
	u32 test_id;
	u32 test_id_max;
	void (*analyzer)(u32 wr_addr, u32 block_id, u32 test_id, u32 val);
};


/**
 * struct sde_dbg_dsi_ctrl_list_entry - DSI controller debug entry
 * @name: Name of the DSI controller
 * @base: Base memory address of the controller
 * @list: List node for linking multiple DSI controllers
 */
struct sde_dbg_dsi_ctrl_list_entry {
	const char *name;
	void __iomem *base;
	struct list_head list;
};

/**
 * struct sde_dbg_debug_bus_common - Common debug bus metadata
 * @name:                Name of the debug bus configuration
 * @entries_size:        Total number of debug bus entries
 * @limited_entries_size: Number of limited debug bus entries
 * @dumped_content:      Pointer to buffer holding dumped debug values
 * @content_idx:         Current index in the dumped content buffer
 * @content_size:        Total size of the dumped content buffer
 * @blk_id:              Block ID associated with this debug bus
 */
struct sde_dbg_debug_bus_common {
	char *name;
	u32 entries_size;
	u32 limited_entries_size;
	u32 *dumped_content;
	u32 content_idx;
	u32 content_size;
	u64 blk_id;
};

/**
 * struct sde_dbg_sde_debug_bus - Debug bus interface for SDE hardware
 * @cmn:           Common debug bus metadata
 * @entries:       Pointer to full list of debug bus entries
 * @limited_entries: Pointer to a reduced set of debug bus entries
 * @top_blk_off:   Offset to the top-level hardware block
 * @read_tp:       Function to read a test point from the debug bus
 *                 @mem_base: Base memory address
 *                 @wr_addr: Write address
 *                 @rd_addr: Read address
 *                 @block_id: Block ID
 *                 @test_id: Test ID
 * @clear_tp:      Function to clear a test point
 * @disable_block: Function to disable a debug block
 */
struct sde_dbg_sde_debug_bus {
	struct sde_dbg_debug_bus_common cmn;
	struct sde_debug_bus_entry *entries;
	struct sde_debug_bus_entry *limited_entries;
	u32 top_blk_off;
	u32 (*read_tp)(void __iomem *mem_base, u32 wr_addr, u32 rd_addr, u32 block_id, u32 test_id);
	void (*clear_tp)(void __iomem *mem_base, u32 wr_addr);
	void (*disable_block)(void __iomem *mem_base, u32 wr_addr);
};

/**
 * struct sde_dbg_regbuf - wraps buffer and tracking params for register dumps
 * @buf: pointer to allocated memory for storing register dumps in hw recovery
 * @buf_size: size of the memory allocated
 * @len: size of the dump data valid in the buffer
 * @rpos: cursor points to the buffer position read by client
 * @dump_done: to indicate if dumping to user memory is complete
 * @cur_blk: points to the current sde_dbg_reg_base block
 */
struct sde_dbg_regbuf {
	char *buf;
	int buf_size;
	int len;
	int rpos;
	int dump_done;
	struct sde_dbg_reg_base *cur_blk;
};

/**
 * struct sde_dbg_hal_funcs - interface api for sde debug hal
 */
struct sde_dbg_hal_funcs {
	/**
	 * dbg_dump - performs debug dump when panic is triggered
	 * Returns: none
	 */
	void (*dbg_dump[MSM_DISP_OP_MAX])(bool do_panic, const char *name,
			bool dump_secure, u64 dump_blk_mask);

	/**
	 * devcoredump_read - read handler for devcoredump interface in HFI mode
	 * Returns: Number of bytes copied to the buffer, or 0 if no more data available
	 */
	ssize_t (*devcoredump_read[MSM_DISP_OP_MAX])(char *buffer, loff_t offset, size_t count);

	/**
	 * add_minidump_va - registers minidump regions
	 * Returns: none
	 */
	void (*add_minidump_va[MSM_DISP_OP_MAX])(void);
};

/**
 * struct sde_dbg_base - global sde debug base structure
 * @evtlog: event log instance
 * @reglog: reg log instance
 * @reg_base_list: list of register dumping regions
 * @reg_dump_base: base address of register dump region
 * @reg_dump_addr: register dump address for a block/range
 * @dev: device pointer
 * @mutex: mutex to serialize access to serialze dumps, debugfs access
 * @req_dump_blks: list of blocks requested for dumping
 * @panic_on_err: whether to kernel panic after triggering dump via debugfs
 * @dump_work: work struct for deferring register dump work to separate thread
 * @work_panic: panic after dump if internal user passed "panic" special region
 * @dump_option: whether to dump registers and dbgbus into memory, kernel log, or both
 * @coredump_pending: coredump is pending read from userspace
 * @coredump_reading: coredump is in reading stage
 * @dbgbus_sde: debug bus structure for the sde
 * @dbgbus_vbif_rt: debug bus structure for the realtime vbif
 * @dbgbus_dsi: debug bus structure for the dsi
 * @dbgbus_rsc: debug bus structure for rscc
 * @dbgbus_lutdma: debug bus structure for the lutdma hw
 * @dbgbus_dp: debug bus structure for dp
 * @dump_blk_mask: mask of all the hw blk-ids that has to be dumped
 * @dump_secure: dump entries excluding few as it is in secure-session
 * @regbuf: buffer data to track the register dumping in hw recovery
 * @sde_dbg_printer: drm printer handle used to print sde_dbg info in devcoredump device
 * @cur_evt_index: index used for tracking event logs dump in hw recovery
 * @cur_reglog_index: index used for tracking register logs dump in hw recovery
 * @dbgbus_dump_idx: index used for tracking dbg-bus dump in hw recovery
 * @vbif_dbgbus_dump_idx: index for tracking vbif dumps in hw recovery
 * @hw_ownership: indicates if the VM owns the HW resources
 */
struct sde_dbg_base {
	struct sde_dbg_evtlog *evtlog;
	struct sde_dbg_reglog *reglog;
	struct list_head reg_base_list;
	void *reg_dump_base;
	void *reg_dump_addr;
	struct device *dev;
	struct mutex mutex;

	struct sde_dbg_reg_base *req_dump_blks[SDE_DBG_BASE_MAX];

	u32 panic_on_err;
	struct work_struct dump_work;
	bool work_panic;
	u32 dump_option;
	bool coredump_pending;
	bool coredump_reading;

	struct sde_dbg_sde_debug_bus dbgbus_sde;
	struct sde_dbg_sde_debug_bus dbgbus_vbif_rt;
	struct sde_dbg_sde_debug_bus dbgbus_dsi;
	struct sde_dbg_sde_debug_bus dbgbus_rsc;
	struct sde_dbg_sde_debug_bus dbgbus_lutdma;
	struct sde_dbg_sde_debug_bus dbgbus_dp;
	u64 dump_blk_mask;
	bool dump_secure;
	u32 debugfs_ctrl;

	struct drm_printer *sde_dbg_printer;
	struct sde_dbg_regbuf regbuf;
	u32 cur_evt_index;
	u32 cur_reglog_index;
	enum sde_dbg_dump_context dump_mode;
	bool hw_ownership;
	char *read_buf;
	u32 buff_sz;
	bool is_dumped;
	struct sde_dbg_hal_funcs hal_ops;
};

/*
 * reglog keeps this number of entries in memory for debug purpose. This
 * number must be greater than number of possible writes in at least one
 * single commit.
 */
#if IS_ENABLED(CONFIG_DRM_MSM_LOW_MEM_FOOTPRINT)
#define SDE_REGLOG_ENTRY 256
#else
#define SDE_REGLOG_ENTRY 1024
#endif /* IS_ENABLED(CONFIG_DRM_MSM_LOW_MEM_FOOTPRINT) */

struct sde_dbg_reglog_log {
	s64 time;
	u32 pid;
	u32 addr;
	u32 val;
	u8 blk_id;
};

/**
 * @last_dump: Index of last entry to be output during reglog dumps
 * @filter_list: Linked list of currently active filter strings
 */
struct sde_dbg_reglog {
	struct sde_dbg_reglog_log logs[SDE_REGLOG_ENTRY];
	u32 first;
	atomic64_t last;
	u32 last_dump;
	atomic64_t curr;
	u32 next;
	u32 enable;
	u32 enable_mask;
};

extern struct sde_dbg_reglog *sde_dbg_base_reglog;

/**
 * SDE_REG_LOG - Write register write to the register log
 */
#define SDE_REG_LOG(blk_id, val, addr) sde_reglog_log(blk_id, val, addr)

/**
 * SDE_EVT32 - Write a list of 32bit values to the event log, default area
 * ... - variable arguments
 */
#define SDE_EVT32(...) sde_evtlog_log(sde_dbg_base_evtlog, __func__, \
		__LINE__, SDE_EVTLOG_ALWAYS, ##__VA_ARGS__, \
		SDE_EVTLOG_DATA_LIMITER)

/**
 * SDE_EVT32_VERBOSE - Write a list of 32bit values for verbose event logging
 * ... - variable arguments
 */
#define SDE_EVT32_VERBOSE(...) sde_evtlog_log(sde_dbg_base_evtlog, __func__, \
		__LINE__, SDE_EVTLOG_VERBOSE, ##__VA_ARGS__, \
		SDE_EVTLOG_DATA_LIMITER)

/**
 * SDE_EVT32_IRQ - Write a list of 32bit values to the event log, IRQ area
 * ... - variable arguments
 */
#define SDE_EVT32_IRQ(...) sde_evtlog_log(sde_dbg_base_evtlog, __func__, \
		__LINE__, SDE_EVTLOG_IRQ, ##__VA_ARGS__, \
		SDE_EVTLOG_DATA_LIMITER)

/**
 * SDE_EVT32_EXTERNAL - Write a list of 32bit values for external display events
 * ... - variable arguments
 */
#define SDE_EVT32_EXTERNAL(...) sde_evtlog_log(sde_dbg_base_evtlog, __func__, \
		__LINE__, SDE_EVTLOG_EXTERNAL, ##__VA_ARGS__, \
		SDE_EVTLOG_DATA_LIMITER)

/**
 * SDE_DBG_DUMP - trigger dumping of all sde_dbg facilities
 * @dump_blk_mask: mask of all the hw blk-ids that has to be dumped
 * @va_args:	list of named register dump ranges and regions to dump, as
 *		registered previously through sde_dbg_reg_register_base and
 *		sde_dbg_reg_register_dump_range.
 *		Including the special name "panic" will trigger a panic after
 *		the dumping work has completed.
 */
#define SDE_DBG_DUMP(dump_blk_mask, ...) sde_dbg_dump(SDE_DBG_DUMP_PROC_CTX, __func__, \
		dump_blk_mask, ##__VA_ARGS__, SDE_DBG_DUMP_DATA_LIMITER)

/**
 * SDE_DBG_DUMP_WQ - trigger dumping of all sde_dbg facilities, queuing the work
 * @dump_blk_mask: mask of all the hw blk-ids that has to be dumped
 * @va_args:	list of named register dump ranges and regions to dump, as
 *		registered previously through sde_dbg_reg_register_base and
 *		sde_dbg_reg_register_dump_range.
 *		Including the special name "panic" will trigger a panic after
 *		the dumping work has completed.
 */
#define SDE_DBG_DUMP_WQ(dump_blk_mask, ...) sde_dbg_dump(SDE_DBG_DUMP_IRQ_CTX, __func__, \
		dump_blk_mask, ##__VA_ARGS__, SDE_DBG_DUMP_DATA_LIMITER)

/**
 * SDE_DBG_DUMP_CLK_EN - trigger dumping of all sde_dbg facilities, without clk
 * @dump_blk_mask: mask of all the hw blk-ids that has to be dumped
 * @va_args:	list of named register dump ranges and regions to dump, as
 *		registered previously through sde_dbg_reg_register_base and
 *		sde_dbg_reg_register_dump_range.
 *		Including the special name "panic" will trigger a panic after
 *		the dumping work has completed.
 */
#define SDE_DBG_DUMP_CLK_EN(dump_blk_mask, ...) sde_dbg_dump(SDE_DBG_DUMP_CLK_ENABLED_CTX, \
		__func__, dump_blk_mask, ##__VA_ARGS__, SDE_DBG_DUMP_DATA_LIMITER)

/**
 * SDE_DBG_EVT_CTRL - trigger a different driver events
 *  event: event that trigger different behavior in the driver
 */
#define SDE_DBG_CTRL(...) sde_dbg_ctrl(__func__, ##__VA_ARGS__, \
		SDE_DBG_DUMP_DATA_LIMITER)

/**
 * sde_mini_dump_add_va_region - add required va memory region to minidump.
 * @size:	size of the memory region to be added
 * @name:	minidump partition name
 * @virt_addr:	pointer to memory region
 * Returns:	none
 */
void sde_mini_dump_add_va_region(const char *name, u32 size, void *virt_addr);

/**
 * sde_evtlog_init - allocate a new event log object
 * Returns:	evtlog or -ERROR
 */
struct sde_dbg_evtlog *sde_evtlog_init(void);

/**
 * sde_reglog_init - allocate a new reg log object
 * Returns:	reglog or -ERROR
 */
struct sde_dbg_reglog *sde_reglog_init(void);

/**
 * sde_evtlog_destroy - destroy previously allocated event log
 * @evtlog:	pointer to evtlog
 * Returns:	none
 */
void sde_evtlog_destroy(struct sde_dbg_evtlog *evtlog);

/**
 * sde_reglog_destroy - destroy previously allocated reg log
 * @reglog:	pointer to reglog
 * Returns:	none
 */
void sde_reglog_destroy(struct sde_dbg_reglog *reglog);

/**
 * sde_evtlog_log - log an entry into the event log.
 *	log collection may be enabled/disabled entirely via debugfs
 *	log area collection may be filtered by user provided flags via debugfs.
 * @evtlog:	pointer to evtlog
 * @name:	function name of call site
 * @line:	line number of call site
 * @flag:	log area filter flag checked against user's debugfs request
 * Returns:	none
 */
void sde_evtlog_log(struct sde_dbg_evtlog *evtlog, const char *name, int line,
		int flag, ...);

/**
 * sde_reglog_log - log an entry into the reg log.
 *      log collection may be enabled/disabled entirely via debugfs
 *      log area collection may be filtered by user provided flags via debugfs.
 * @reglog:     pointer to evtlog
 * Returns:     none
 */
void sde_reglog_log(u8 blk_id, u32 val, u32 addr);

/**
 * sde_evtlog_dump_to_buffer - parse one line of evtlog to a given buffer
 * @evtlog:	pointer to evtlog
 * @evtlog_buf: buffer to store evtlog
 * @evtlog_buf_size: lenght of the buffer
 * @update_last_entry: whether update last dump marker
 * @full_dump: 1, print the whole evtlog captured; 0, print last 256 lines of log
 * Returns:	log size
 */
ssize_t sde_evtlog_dump_to_buffer(struct sde_dbg_evtlog *evtlog,
		char *evtlog_buf, ssize_t evtlog_buf_size,
		bool update_last_entry, bool full_dump);

/**
 * sde_evtlog_dump_all - dump evtlog based on selected dump options
 * @evtlog:	pointer to evtlog
 * Returns:	log size
 */
void sde_evtlog_dump_all(struct sde_dbg_evtlog *evtlog);

/**
 * sde_evtlog_count - count the current log size for print
 * @evtlog:	pointer to evtlog
 * Returns:	log size
 */
u32 sde_evtlog_count(struct sde_dbg_evtlog *evtlog);

/**
 * sde_evtlog_is_enabled - check whether log collection is enabled for given
 *	event log and log area flag
 * @evtlog:	pointer to evtlog
 * @flag:	log area filter flag
 * Returns:	none
 */
bool sde_evtlog_is_enabled(struct sde_dbg_evtlog *evtlog, u32 flag);

/**
 * sde_evtlog_dump_to_buffer - print content of event log to the given buffer
 * @evtlog:		pointer to evtlog
 * @evtlog_buf:		target buffer to print into
 * @evtlog_buf_size:	size of target buffer
 * @update_last_entry:	whether or not to stop at most recent entry
 * @full_dump:          whether to dump full or to limit print entries
 * Returns:		number of bytes written to buffer
 */
ssize_t sde_evtlog_dump_to_buffer(struct sde_dbg_evtlog *evtlog,
		char *evtlog_buf, ssize_t evtlog_buf_size,
		bool update_last_entry, bool full_dump);

/**
 * sde_dbg_init_dbg_buses - initialize debug bus dumping support for the chipset
 * @hw_rev:		Chipset revision
 */
void sde_dbg_init_dbg_buses(u32 hw_rev);

/**
 * sde_dbg_init - initialize global sde debug facilities: evtlog, regdump
 * @dev:		device handle
 * Returns:		0 or -ERROR
 */
int sde_dbg_init(struct device *dev);

/**
 * sde_dbg_setup - setup global sde debug
 * @dev:	pointer to device
 * Returns:		0 or -ERROR
 */
int sde_dbg_setup(struct device *dev);

/**
 * sde_dbg_debugfs_register - register entries at the given debugfs dir
 * @dev:	pointer to device
 * Returns:	0 or -ERROR
 */
int sde_dbg_debugfs_register(struct device *dev);

/**
 * sde_dbg_destroy - destroy the global sde debug facilities
 * Returns:	none
 */
void sde_dbg_destroy(void);

/**
 * sde_dbg_dump - trigger dumping of all sde_dbg facilities
 * @queue_work:	whether to queue the dumping work to the work_struct
 * @name:	string indicating origin of dump
 * @dump_blk_mask: mask of all the hw blk-ids that has to be dumped
 * @va_args:	list of named register dump ranges and regions to dump, as
 *		registered previously through sde_dbg_reg_register_base and
 *		sde_dbg_reg_register_dump_range.
 *		Including the special name "panic" will trigger a panic after
 *		the dumping work has completed.
 * Returns:	none
 */
void sde_dbg_dump(enum sde_dbg_dump_context mode, const char *name, u64 dump_blk_mask, ...);

/**
 * sde_dbg_ctrl - trigger specific actions for the driver with debugging
 *		purposes. Those actions need to be enabled by the debugfs entry
 *		so the driver executes those actions in the corresponding calls.
 * @va_args:	list of actions to trigger
 * Returns:	none
 */
void sde_dbg_ctrl(const char *name, ...);

/**
 * sde_dbg_reg_register_base - register a hw register address section for later
 *	dumping. call this before calling sde_dbg_reg_register_dump_range
 *	to be able to specify sub-ranges within the base hw range.
 * @name:	name of base region
 * @base:	base pointer of region
 * @max_offset:	length of region
 * @phys_addr:	physical address of region
 * @blk_id:	hw block id
 * Returns:	0 or -ERROR
 */
int sde_dbg_reg_register_base(const char *name, void __iomem *base,
		size_t max_offset, unsigned long phys_addr, u64 blk_id);

/**
 * sde_dbg_reg_register_cb - register a hw register callback for later
 *	dumping.
 * @name:	name of base region
 * @cb:		callback of external region
 * @cb_ptr:	private pointer of external region
 * Returns:	0 or -ERROR
 */
int sde_dbg_reg_register_cb(const char *name, void (*cb)(void *), void *ptr);

/**
 * sde_dbg_reg_unregister_cb - register a hw unregister callback for later
 *	dumping.
 * @name:	name of base region
 * @cb:		callback of external region
 * @cb_ptr:	private pointer of external region
 * Returns:	None
 */
void sde_dbg_reg_unregister_cb(const char *name, void (*cb)(void *), void *ptr);

/**
 * sde_dbg_reg_register_dump_range - register a hw register sub-region for
 *	later register dumping associated with base specified by
 *	sde_dbg_reg_register_base
 * @base_name:		name of base region
 * @range_name:		name of sub-range within base region
 * @offset_start:	sub-range's start offset from base's base pointer
 * @offset_end:		sub-range's end offset from base's base pointer
 * @xin_id:		xin id
 * Returns:		none
 */
void sde_dbg_reg_register_dump_range(const char *base_name,
		const char *range_name, u32 offset_start, u32 offset_end,
		uint32_t xin_id);

/**
 * sde_dbg_dsi_ctrl_register - register a dsi ctrl for debugbus dumping
 * @base:	iomem base address for dsi controller
 * @name:	name of the dsi controller
 * Returns:	0 or -ERROR
 */
int sde_dbg_dsi_ctrl_register(void __iomem *base, const char *name);

/**
 * sde_dbg_set_sde_top_offset - set the target specific offset from mdss base
 *	address of the top registers. Used for accessing debug bus controls.
 * @blk_off: offset from mdss base of the top block
 */
void sde_dbg_set_sde_top_offset(u32 blk_off);

/**
 * sde_dbg_set_hw_ownership_status - set the VM HW ownership status
 * @enable:	flag to control HW ownership status
 */
void sde_dbg_set_hw_ownership_status(bool enable);

/**
 * sde_evtlog_set_filter - update evtlog filtering
 * @evtlog:	pointer to evtlog
 * @filter:     pointer to optional function name filter, set to NULL to disable
 */
void sde_evtlog_set_filter(struct sde_dbg_evtlog *evtlog, char *filter);

/**
 * sde_evtlog_get_filter - query configured evtlog filters
 * @evtlog:	pointer to evtlog
 * @index:	filter index to retrieve
 * @buf:	pointer to output filter buffer
 * @bufsz:	size of output filter buffer
 * Returns:	zero if a filter string was returned
 */
int sde_evtlog_get_filter(struct sde_dbg_evtlog *evtlog, int index,
		char *buf, size_t bufsz);

#ifndef CONFIG_DRM_SDE_RSC
static inline void sde_rsc_debug_dump(u32 mux_sel)
{
}
#else
/**
 * sde_rsc_debug_dump - sde rsc debug dump status
 * @mux_sel:»       select mux on rsc debug bus
 */
void sde_rsc_debug_dump(u32 mux_sel);
#endif

/**
 * sde_dbg_update_dump_mode - update dump mode to in_coredump mode if devcoredump
 *  fueature is enabled. Default dump mode is in_mem, if HW recovery feature is
 *  enabled, this function will be called to set dump mode to in_coredump option.
 * @enable_coredump: if enable_coredump is true, update dump mode to in_coredump,
 *	otherwise reset the dump mode to default mode.
 */
void sde_dbg_update_dump_mode(bool enable_coredump);

#endif /* SDE_DBG_H_ */
