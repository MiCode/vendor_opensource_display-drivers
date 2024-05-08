#ifndef _IVI_TOUCH_H_
#define _IVI_TOUCH_H_
#include <linux/input.h>
#include <linux/input/mt.h>

#define POINTER_PRESSS  (0xC0)
#define POINTER_MOVING  (0x90)
#define POINTER_RELEASE (0xA0)

/****************************Touchpanel related define*********************************/
#define PRESSURE_MIN 0   /* /< min value of pressure reported */
#define PRESSURE_MAX 127 /* /< Max value of pressure reported */

#define TOUCH_ID_MAX 10 /* /< Max number of simoultaneous touches \
                         * reported */

#define AREA_MIN PRESSURE_MIN /* /< min value of Major/minor axis \
                               * reported */
#define AREA_MAX PRESSURE_MAX /* /< Man value of Major/minor axis \
                               * reported */
/**************************************************************************************/

struct touch_pointer {
	int32_t position_x;
	int32_t position_y;
	int8_t slot;
	int8_t tracking_id;
	int32_t pointer_status;
};

struct ivi_touch {
	struct input_dev *touch_input;
	struct delayed_work touch_delay_work;
	struct mutex touch_lock;
	struct touch_pointer pointer_arry[TOUCH_ID_MAX];
	void* ivi_data;
};

int process_touch_info(struct ivi_touch *data, uint8_t *reg_data);
int ivi_touch_init(struct ivi_touch *data);
int ivi_touch_remove(struct ivi_touch *data);

#endif