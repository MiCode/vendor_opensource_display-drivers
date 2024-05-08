#ifndef _HUD_H_
#define _HUD_H_
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/backlight.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include "i2c_interface.h"
#include "panel_common.h"

#define HUD_STANRD_READ_FRAME_LEN (17)
#define HUD_STANRD_WRITE_FRAME_LEN (13)

#define HUD_RLS_DEFAULT_VAL (2000)

typedef enum {
	/* salve to host */
	CMD_S2H_HUD_STARTUP_DONE = 0x10,
	CMD_S2H_PGU_BACKLIGHT_EN_FEEDBACK = 0x20,
	CMD_S2H_HUD_CURRENT_SCREEN_STATUS = 0x30,
	CMD_S2H_HUD_VEHSPD_FEEDBACK = 0x40,
	CMD_S2H_HUD_SW_STATUS_FEEDBACK = 0x50,
	CMD_S2H_HUD_SW_POSITION_FEEDBACK = 0x60,
	CMD_S2H_HUD_SW_LUMINANCE_FEEDBACK = 0x70,
	CMD_S2H_HUD_SW_ROTATION_FEEDBACK = 0x80,
	CMD_S2H_HUD_TIMEOUT_FEEDBACK = 0x90,
	CMD_S2H_HUD_RLS_SENSOR_STATUS = 0xA0,
	CMD_S2H_HUD_WORK_STATUS_FEEDBACK = 0xB0,
	CMD_S2H_HUD_UPGRADE_STANDARD = 0x17,
	CMD_S2H_HUD_UPGRADE_EXTEND = 0x18,

	/* host to salve */
	CMD_H2S_HUD_TIMESTAMP = 0x01,
	CMD_H2S_HUD_PGU_BACKLIGHT_ENABLE = 0x02,
	CMD_H2S_HUD_RLS_SENSOR_VAL = 0x03,
	CMD_H2S_HUD_VEHSPD = 0x04,
	CMD_H2S_HUD_SW_STATUS = 0x05,
	CMD_H2S_HUD_SW_POSITION = 0x06,
	CMD_H2S_HUD_SW_LUMINANCE = 0x07,
	CMD_H2S_HUD_SW_ROTATION = 0x08,
	CMD_H2S_HUD_TIMEOUT_RESPOND = 0x09,
	CMD_H2S_HUD_RLS_SENSOR_STATUS = 0x0A,
	CMD_H2S_HUD_UPGRADE_STANDARD = 0xF0,
	CMD_H2S_HUD_UPGRADE_EXTEND = 0xF1,
} hud_cmd_id;

typedef enum {
	HUD_BACKLIGHT_MODE_OFF,
	HUD_BACKLIGHT_MODE_ON,
} hud_backlight_mode;

typedef enum {
	HUD_SWITCH_STATUS_CLOSE,
	HUD_SWITCH_STATUS_OPEN,
} hud_switch_state;

typedef struct {
	uint8_t sensor_light_value;
	uint8_t sensor_light_status;
	uint8_t vehicle_speed;
} hud_e2e_value;

typedef struct {
	uint16_t RlsHudSnsrVal;
	uint16_t RlsFwdLiSnsrVal;
	uint16_t RlsTopLiSnsrVal;
	uint8_t RlsIRVal;
	uint8_t RlsOutdBriSts;
} rls_sensor_light_value;

typedef struct {
	uint8_t RlsSts;
	uint8_t RlsLiSnsrIRFlt;
	uint8_t RlsLiSnsrTopFlt;
	uint8_t RlsLiSnsrFwdFlt;
} rls_sensor_light_status;

typedef struct {
	uint16_t vehicle_speed;
	uint8_t vehicle_speed_valid;
} vehicle_speed_value;

typedef struct hud_safety_info {
	rls_sensor_light_value val;
	rls_sensor_light_status status;
	vehicle_speed_value speed;
	hud_e2e_value e2e_value;
} hud_safety_info_t;

struct hud_panel_device {
	struct panel_device *base;
	int irq;
	int irq_gpio;
	int backlight_mode;
	bool backlight_enable;

	struct work_struct safety_work;
	struct hrtimer safety_timer;
	hud_safety_info_t current_safe_info;
	bool msg_log_enable;
};

typedef enum {
	E2E_SENSOR_LIGHT_VALUE,
	E2E_SENSOR_LIGHT_STATUS,
	E2E_VEHICLE_SPEED,
} hud_e2e_type;


typedef struct setting_factor {
	volatile uint8_t setting_val;
	volatile uint8_t device_val;
	bool have_set;
} setting_factor;

typedef struct hud_setting {
	setting_factor switch_state;
	setting_factor backlight_enable;
	setting_factor position;
	setting_factor luminance;
	setting_factor rotation;
	setting_factor startup_done;
	setting_factor backlight_mode;
} hud_setting;

typedef struct hud_read_cmd {
	uint8_t cmd_id;
	uint8_t data[HUD_STANRD_READ_FRAME_LEN];
} hud_read_cmd;

typedef struct hud_write_cmd {
	uint8_t data[HUD_STANRD_WRITE_FRAME_LEN];
} hud_write_cmd;

void* hud_panel_device_init(struct panel_device *panel_base);
#endif
