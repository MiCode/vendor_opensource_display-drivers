#ifndef __CLUSTER_H_
#define __CLUSTER_H_
#include <linux/mutex.h>
#include "i2c_interface.h"
#include "panel_common.h"

#define CLUSTER_HOT_PLUG_CHECK_TIME_MS (500)
#define CLUSTER_TEMP_CHECK_TIME_MS (2000)
#define CLUSTER_TEMP_CHECK_CYCLE                                               \
	(CLUSTER_TEMP_CHECK_TIME_MS / CLUSTER_HOT_PLUG_CHECK_TIME_MS)
#define CLUSTER_GET_SERDES_STATUS_TIME_MS (2000)
#define CLUSTER_GET_SERDES_STATUS_CYCLE                                        \
	(CLUSTER_GET_SERDES_STATUS_TIME_MS / CLUSTER_HOT_PLUG_CHECK_TIME_MS)
#define CLUSTER_LOCK_FAILED_CHECK_TIME_MS (200)
#define CLUSTER_LOCK_FAILED_PRINT_PERIOD_MS (1000)
#define CLUSTER_LOCK_FAILED_PRINT_CYCLE                                        \
	(CLUSTER_LOCK_FAILED_PRINT_PERIOD_MS /                                 \
	 CLUSTER_LOCK_FAILED_CHECK_TIME_MS)
#define CLUSTER_LOCK_FAILED_RESTART_MS (3000)
#define CLUSTER_LOCK_FAILED_RESTART_CYCLE                                      \
	(CLUSTER_LOCK_FAILED_RESTART_MS / CLUSTER_LOCK_FAILED_CHECK_TIME_MS)

struct cluster_panel_device {
	struct panel_device *base;
	struct delayed_work check_work;
	struct notifier_block cluseter_brightness_notifier;
};

void* cluster_panel_device_init(struct panel_device *panel_base);
#endif
