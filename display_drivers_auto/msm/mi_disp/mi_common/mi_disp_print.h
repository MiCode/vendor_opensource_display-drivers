/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#ifndef _MI_DISP_PRINT_H_
#define _MI_DISP_PRINT_H_

#include <linux/compiler.h>
#include <linux/printk.h>

#include "mi_disp_config.h"

#if MI_DISP_PRINT_ENABLE
#define DISP_NAME    "mi_disp"

char *mi_disp_get_power_mode_name(int power_mode);

__printf(2, 3)
void mi_disp_printk(const char *level, const char *format, ...);
__printf(1, 2)
void mi_disp_dbg(const char *format, ...);
__printf(2, 3)
void mi_disp_local_time_printk(const char *level, const char *format, ...);
__printf(1, 2)
void mi_disp_local_time_dbg(const char *format, ...);

#define DISP_WARN(fmt, ...)     \
		mi_disp_printk(KERN_WARNING, "[W]" fmt, ##__VA_ARGS__)

#define DISP_INFO(fmt, ...)     \
		mi_disp_printk(KERN_INFO, "[I]" fmt, ##__VA_ARGS__)

#define DISP_ERROR(fmt, ...)    \
		mi_disp_printk(KERN_ERR, "[E]" fmt, ##__VA_ARGS__)

#define DISP_DEBUG(fmt, ...)                             \
	do {                                                   \
		pr_debug("[D]" fmt, ##__VA_ARGS__);                  \
	} while (0)

#define DISP_TIME_WARN(fmt, ...)   \
		mi_disp_local_time_printk(KERN_WARNING, "[W]" fmt, ##__VA_ARGS__)
#define DISP_TIME_INFO(fmt, ...)   \
		mi_disp_local_time_printk(KERN_INFO, "[I]" fmt, ##__VA_ARGS__)
#define DISP_TIME_ERROR(fmt, ...)  \
		mi_disp_local_time_printk(KERN_ERR, "[E]" fmt, ##__VA_ARGS__)
#define DISP_TIME_DEBUG(fmt, ...)                          \
	do {                                                     \
		pr_debug("[D]" fmt, ##__VA_ARGS__);                    \
	} while (0)
#else /* !MI_DISP_PRINT_ENABLE */
#define DISP_WARN(fmt, ...)     \
		printk(KERN_WARNING, fmt, ##__VA_ARGS__)
#define DISP_INFO(fmt, ...)     \
		printk(KERN_INFO, fmt, ##__VA_ARGS__)
#define DISP_ERROR(fmt, ...)    \
		printk(KERN_ERR, fmt, ##__VA_ARGS__)
#define DISP_DEBUG(fmt, ...)    \
		pr_debug(fmt, ##__VA_ARGS__)
#define DISP_TIME_WARN(fmt, ...)   \
		printk(KERN_WARNING, fmt, ##__VA_ARGS__)
#define DISP_TIME_INFO(fmt, ...)   \
		printk(KERN_INFO, fmt, ##__VA_ARGS__)
#define DISP_TIME_ERROR(fmt, ...)  \
		printk(KERN_ERR, fmt, ##__VA_ARGS__)
#define DISP_TIME_DEBUG(fmt, ...)  \
			pr_debug(fmt, ##__VA_ARGS__)
#endif


#endif /* _MI_DISP_PRINT_H_ */
