#ifndef _IVI_COMMON_H_
#define _IVI_COMMON_H_
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/backlight.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include "i2c_interface.h"
#include "panel_common.h"

/* touch point(8 * 6) + msg info(3) + padding(13) */
#define IVI_EXT_READ_FRAME_LEN (8 * 6 + 3 + 16)

#define IVI_STANRD_READ_FRAME_LEN (16)
#define IVI_STANRD_WRITE_FRAME_LEN (12)
#define IVI_FRAME_INFO_LENGTH (6)
#define IVI_STANRD_READ_FRAME_DATA_LENGTH_MAX (IVI_STANRD_READ_FRAME_LEN - IVI_FRAME_INFO_LENGTH)

#define IVI_MCU_SEND_CMD_KEEP_INTERVAL_MS (11)
#define IVI_STANRD_FRAME_EXT_LENGTH_INDEX (IVI_STANRD_READ_FRAME_LEN - 2)
#define IVI_MCU_READ_CMD_RETRY_TIMES (1)
#define IVI_MCU_WRITE_CMD_RETRY_TIMES (3)
#define IVI_STUFF_BYTE (0xAA)
#define IVI_WAIT_WRITE_CMD_COMPLETE_MS (800)
#define IVI_UPGRADE_WAIT_WRITE_CMD_COMPLETE_MS (800)

#define IVI_HOT_PLUG_CHECK_TIME_MS (1000)
#define IVI_GET_SERDES_STATUS_TIME_MS (2000)
#define IVI_GET_SERDES_STATUS_CYCLE \
    (IVI_GET_SERDES_STATUS_TIME_MS / IVI_HOT_PLUG_CHECK_TIME_MS)
#define IVI_LOCK_FAILED_CHECK_TIME_MS (200)
#define IVI_LOCK_FAILED_PRINT_PERIOD_MS (1000)
#define IVI_LOCK_FAILED_PRINT_CYCLE \
    (IVI_LOCK_FAILED_PRINT_PERIOD_MS / IVI_LOCK_FAILED_CHECK_TIME_MS)
#define IVI_LOCK_FAILED_RESTART_MS (5000)
#define IVI_LOCK_FAILED_RESTART_CYCLE \
    (IVI_LOCK_FAILED_RESTART_MS / IVI_LOCK_FAILED_CHECK_TIME_MS)
#define IVI_DUMP_PANEL_STATUS_TIME_MS (10000)
#define IVI_DUMP_PANEL_STATUS_CYCLE \
    (IVI_DUMP_PANEL_STATUS_TIME_MS / IVI_HOT_PLUG_CHECK_TIME_MS)

typedef enum {
	/* salve to host */
	CMD_S2H_TP_STANDARD_INFO = 0x01,
	CMD_S2H_TP_EXTEND_INFO,
	CMD_S2H_SCREEN_STATUS = 0x10,
	CMD_S2H_BL_REQUEST,
	CMD_S2H_WAIT_TIMEOUT,
	CMD_S2H_SCREEN_INFO_RESPOND,
	CMD_S2H_PRODUCT_ID,
	CMD_S2H_VERSION_INFO,
	CMD_S2H_DIAGNOSE_INFO,
	CMD_S2H_UPGRADE,
	CMD_S2H_TIMESTAMP_REQUEST,
	CMD_S2H_ECU_SN_STANDARD = 0X1A,
	CMD_S2H_ECU_SN_EXT,
	CMD_S2H_ECU_PN_STANDARD,
	CMD_S2H_ECU_PN_EXT,
	CMD_S2H_POGO_ACCESS_STATUS = 0x21,

	/* host to salve */
	CMD_H2S_HANDSHAKE_STATE = 0x80,
	CMD_H2S_SCREEN_INFO_REQUEST,
	CMD_H2S_TIMEOUT_SETTING,
	CMD_H2S_SEND_BL,
	CMD_H2S_TP_RESET = 0x84,
	CMD_H2S_TIMESTAMP_RESPOND,
	CMD_H2S_UPGRADE_STANDARD = 0xF0,
	CMD_H2S_UPGRADE_EXTEND_1ST,
	CMD_H2S_UPGRADE_EXTEND_2ND,
	CMD_H2S_UPGRADE_EXTEND_3RD,
} ivi_cmd_id;

struct ivi_write_msg {
	uint8_t data[IVI_STANRD_WRITE_FRAME_LEN];
};

struct ivi_read_msg {
	uint8_t cmd_id;
	uint8_t* data;
	uint8_t num;
};

struct ivi_common_device {
	struct panel_device *base;
	int irq;
	struct mutex msg_mutex;
	u64 last_send_timestamp;
};

int ivi_msg_send(struct ivi_common_device* ivi_common, struct ivi_write_msg write_msg, size_t count);
int ivi_msg_receive(struct ivi_common_device *ivi_common, struct ivi_read_msg* read_msg, size_t count);
int32_t ivi_send_timestamp(struct ivi_common_device *ivi_common);
int32_t ivi_request_screeninfo(struct ivi_common_device *ivi_common);
void ivi_common_device_init(struct ivi_common_device *ivi_common_data, struct panel_device *panel_base);
#endif
