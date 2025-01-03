<<<<<<< HEAD   (ad9a29 Merge "o82: display: fixed the flash when trigger crash" int)
#include "max96789.h"

#define SINGLE_HUD 0
#define DUAL_HUD 1

#if SINGLE_HUD
static const struct cmd_16bit serializer_init_configs[] = {
	// int gpio cfg
	/*{ IS_HOST_I2C_ADDRESS, 0x2c4, 0x04, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c5, 0x24, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c6, 0x04, 0 },
	// int1 gpio cfg
	{ IS_HOST_I2C_ADDRESS, 0x2c7, 0x04, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c8, 0x28, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c9, 0x08, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x01, 0x08, 0 },*/

};

static const struct cmd_16bit pre_init_config[] = {
	//disable remote control
	{ IS_HOST_I2C_ADDRESS, MAX96789_REG1, 0x18, 0 },
	//disable ETOP for mipi
	{ IS_HOST_I2C_ADDRESS, 0x0389, 0x12, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x140, 0x20, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x10, 0x21, 0 },
	// int gpio cfg
	{ IS_HOST_I2C_ADDRESS, 0x2c4, 0x04, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c5, 0x24, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c6, 0x04, 0 },
	// int1 gpio cfg
	{ IS_HOST_I2C_ADDRESS, 0x2c7, 0x04, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c8, 0x28, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c9, 0x08, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x01, 0x08, 0 },
};

static const struct cmd_16bit deserializer_A_init_configs[] = {};

#elif DUAL_HUD

static const struct cmd_16bit serializer_init_configs[] = {
	// int gpio cfg
	/*{ IS_HOST_I2C_ADDRESS, 0x2c4, 0x04, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c5, 0x24, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c6, 0x04, 0 },
	// int1 gpio cfg
	{ IS_HOST_I2C_ADDRESS, 0x2c7, 0x04, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c8, 0x28, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c9, 0x08, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x01, 0x08, 0 },*/

};

static const struct cmd_16bit pre_init_config[] = {
	//disable remote control
	{ IS_HOST_I2C_ADDRESS, MAX96789_REG1, 0x18, 0 },
	{IS_HOST_I2C_ADDRESS, 0x02, 0x73 ,0},
	{IS_HOST_I2C_ADDRESS, 0x53, 0x10 ,0},
	{IS_HOST_I2C_ADDRESS, 0x57, 0x21 ,0},
	{IS_HOST_I2C_ADDRESS, 0x332, 0x4E ,0},
	{IS_HOST_I2C_ADDRESS, 0x333, 0xE4 ,0},
	{IS_HOST_I2C_ADDRESS, 0x04, 0xF2 ,0},
	{IS_HOST_I2C_ADDRESS, 0x389, 0x12 ,0},
	{IS_HOST_I2C_ADDRESS, 0x308, 0x5C ,0},
	{IS_HOST_I2C_ADDRESS, 0x311, 0x03 ,0},
	{IS_HOST_I2C_ADDRESS, 0x331, 0x03 ,0},
	{IS_HOST_I2C_ADDRESS, 0x330, 0x06 ,0},
	{IS_HOST_I2C_ADDRESS, 0x31C, 0x98 ,0},
	{IS_HOST_I2C_ADDRESS, 0x321, 0x24 ,0},
	{IS_HOST_I2C_ADDRESS, 0x31D, 0x98 ,0},
	{IS_HOST_I2C_ADDRESS, 0x322, 0x24 ,0},
	{IS_HOST_I2C_ADDRESS, 0x326, 0xE4 ,0},
	{IS_HOST_I2C_ADDRESS, 0x385, 0x14 ,0},
	{IS_HOST_I2C_ADDRESS, 0x386, 0x04 ,0},
	{IS_HOST_I2C_ADDRESS, 0x387, 0x00 ,0},
	{IS_HOST_I2C_ADDRESS, 0x3A5, 0x04 ,0},
	{IS_HOST_I2C_ADDRESS, 0x3A7, 0x00 ,0},
	{IS_HOST_I2C_ADDRESS, 0x3A6, 0x40 ,0},
	{IS_HOST_I2C_ADDRESS, 0x3A8, 0x80 ,0},
	{IS_HOST_I2C_ADDRESS, 0x3A9, 0x02 ,0},
	{IS_HOST_I2C_ADDRESS, 0x3AA, 0x60 ,0},
	{IS_HOST_I2C_ADDRESS, 0x3AC, 0x05 ,0},
	{IS_HOST_I2C_ADDRESS, 0x3AB, 0x00 ,0},
	{IS_HOST_I2C_ADDRESS, 0x3AD, 0x00 ,0},
	{IS_HOST_I2C_ADDRESS, 0x3AE, 0x0A ,0},
	{IS_HOST_I2C_ADDRESS, 0x3A4, 0xC1 ,0},
	{IS_HOST_I2C_ADDRESS, 0x32A, 0x07 ,0},
	{IS_HOST_I2C_ADDRESS, 0x02, 0x73 ,0},
	{IS_HOST_I2C_ADDRESS, 0x10, 0x23 ,0},
	// int gpio cfg
	{ IS_HOST_I2C_ADDRESS, 0x2c4, 0x04, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c5, 0x24, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c6, 0x04, 0 },
	// int1 gpio cfg
	{ IS_HOST_I2C_ADDRESS, 0x2c7, 0x04, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c8, 0x28, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c9, 0x08, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x01, 0x08, 0 },
};
static const struct cmd_16bit deserializer_A_init_configs[] = {};

#else

static const struct cmd_16bit serializer_init_configs[] = {
	//{ IS_HOST_I2C_ADDRESS, 0x2c4, 0x04, 0 },
	//{ IS_HOST_I2C_ADDRESS, 0x2c5, 0x26, 0 },
	//{ IS_HOST_I2C_ADDRESS, 0x2c6, 0x06, 0 },
	//{ IS_HOST_I2C_ADDRESS, 0x2d3, 0x04, 0 },
	//{ IS_HOST_I2C_ADDRESS, 0x2d4, 0x24, 0 },
	//{ IS_HOST_I2C_ADDRESS, 0x2d5, 0x04, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x01, 0x08, 0 },

};

//use this to config cluster deserializer
static const struct cmd_16bit deserializer_A_init_configs[] = {
	/* GPIO03 BL_ON */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0209, 0x00, 0 },
	/* GPIO10 LCD_STBY_EN */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x021E, 0x00, 0 },
	/* GPIO06 LCD_RST */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0212, 0x00, 50000 },
	/* GPIO05 LCD3V3 */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x020F, 0x00, 50000 },
	/* MAX96752F link margin improvement set */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0242, 0x10, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0458, 0x28, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0459, 0x68, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x043E, 0xB3, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x043F, 0x72, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0558, 0x28, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0559, 0x68, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x053E, 0xB3, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x053F, 0x72, 100 },
	/* Disable max 96755 and max96752 audio channel */
	/* Turn off video pipe */
	//{ IS_DEVICE_I2C_A_ADDRESS, 0x0002, 0x43, 0 },
	//{ IS_DEVICE_I2C_A_ADDRESS, 0x0140, 0x20, 0 },

	/* MAX96781 GPIO18 MAPPING TO MAX96752 GPIO10 */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0215, 0x05, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0216, 0x32, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0217, 0x12, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0218, 0x04, 0 },

	/* Turn off video pipe */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0002, 0x03, 1000 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x01ce, 0x46, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x01ea, 0x05, 0 },
	//TODO change stream id
	/* set stream id */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0050, 0x00, 0 },

	/* GPIO04 BISTEN */
	//{ IS_DEVICE_I2C_A_ADDRESS, 0x020C, 0x10, 0 },
	/* GPIO07 LCD_FAIL_T */
	//{ IS_DEVICE_I2C_A_ADDRESS, 0x0215, 0x10, 0 },
	/* GPIO05 LCD3V3 */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x020F, 0x10, 5 * 1000 },
	/* GPIO06 LCD_RST */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0212, 0x10, 40 * 1000 },
	/* GPIO10 LCD_STBY_EN */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x021E, 0x10, 0 },
	/* turn on video */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0002, 0x43, 5 },
	/* GPIO03 BL_ON */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0209, 0x10, 100 },
	/* GPIO9 BL_PWM */
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0206, 0x10, 0 },
};

static const struct cmd_16bit pre_init_config[] = {
	//disable remote control
	{ IS_HOST_I2C_ADDRESS, MAX96789_REG1, 0x18, 0 },
	//disable ETOP
	{ IS_HOST_I2C_ADDRESS, 0x0389, 0x12, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x0332, 0x4e, 0 },
	//single link B
	{ IS_HOST_I2C_ADDRESS, 0x10, 0x22, 0 },
};
#endif

#if 0
// for testing cluster 300*1280,hud 1280*640
static const struct cmd_16bit pre_init_config[] = {
	//disable remote control
	{ IS_HOST_I2C_ADDRESS, MAX96789_REG1, 0x18, 0 },
	//disable ETOP
	{ IS_HOST_I2C_ADDRESS, 0x53, 0x10, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x57, 0x21, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x332, 0x4E, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x333, 0xE4, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x04, 0xF2, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x308, 0x5C, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x311, 0x03, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x331, 0x03, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x330, 0x06, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x31C, 0x98, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x321, 0x24, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x31D, 0x98, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x322, 0x24, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x326, 0xE4, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x385, 0x40, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x386, 0x02, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x387, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3A5, 0x28, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3A7, 0x02, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3A6, 0x80, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3A8, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3A9, 0x05, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3AA, 0x28, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3AC, 0x02, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3AB, 0x80, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3AD, 0x2C, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3AE, 0x06, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3A4, 0xC1, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x700, 0x28, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x701, 0xBB, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x702, 0x07, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x703, 0x88, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x704, 0x0C, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x705, 0x0E, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x706, 0x38, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x707, 0xD4, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x708, 0x23, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x709, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x70A, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x70B, 0x2C, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x70C, 0x01, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x70D, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x70E, 0x05, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x70F, 0x2C, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x710, 0x01, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x711, 0x2C, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x712, 0x06, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x713, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x714, 0x05, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x715, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x716, 0x10, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x717, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x718, 0x10, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x720, 0x20, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x721, 0x7E, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x722, 0x07, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x723, 0xE8, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x724, 0x02, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x725, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x726, 0x20, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x727, 0x3A, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x728, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x729, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x72A, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x72B, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x72C, 0x20, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x72D, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x72E, 0x54, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x72F, 0x01, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x730, 0x52, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x731, 0x05, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x732, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x733, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x734, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x735, 0x34, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x736, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x737, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x738, 0x2C, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x739, 0x01, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x73A, 0x48, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x73B, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x73C, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x73D, 0x05, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x73E, 0xF4, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x73F, 0x76, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x740, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x750, 0xA0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x751, 0x9D, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x752, 0x0D, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x753, 0x48, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x754, 0x05, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x755, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x756, 0xA0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x757, 0x69, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x758, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x759, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x75A, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x75B, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x75C, 0x20, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x75D, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x75E, 0x28, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x75F, 0x05, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x760, 0xA9, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x761, 0x02, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x762, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x763, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x764, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x765, 0x34, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x766, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x767, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x768, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x769, 0x05, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x76A, 0x48, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x76B, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x76C, 0x80, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x76D, 0x02, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x76E, 0x54, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x76F, 0xD8, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x770, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x719, 0x27, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x02, 0x33, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x10, 0x23, 0 },
};

// for testing cluster 300*1390，hud 1280*695
static const struct cmd_16bit pre_init_config[] = {
	//disable remote control
	{ IS_HOST_I2C_ADDRESS, MAX96789_REG1, 0x18, 0 },
	//disable ETOP
	{ IS_HOST_I2C_ADDRESS, 0x53, 0x10, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x57, 0x21, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x332, 0x4E, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x333, 0xE4, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x04, 0xF2, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x308, 0x5C, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x311, 0x03, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x331, 0x03, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x330, 0x06, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x31C, 0x98, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x321, 0x24, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x31D, 0x98, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x322, 0x24, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x326, 0xE4, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x385, 0x40, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x386, 0x02, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x387, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3A5, 0x28, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3A7, 0x02, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3A6, 0x80, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3A8, 0x6E, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3A9, 0x05, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3AA, 0x28, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3AC, 0x02, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3AB, 0x80, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3AD, 0x2C, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3AE, 0x06, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3A4, 0xC1, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x700, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x701, 0x5B, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x702, 0x08, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x703, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x704, 0x2F, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x705, 0x0F, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x706, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x707, 0xB9, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x708, 0x26, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x709, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x70A, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x70B, 0x2C, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x70C, 0x01, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x70D, 0x6E, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x70E, 0x05, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x70F, 0x2C, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x710, 0x01, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x711, 0x2C, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x712, 0x06, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x713, 0x6E, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x714, 0x05, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x715, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x716, 0x10, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x717, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x718, 0x10, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x720, 0xF8, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x721, 0x1D, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x722, 0x08, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x723, 0xE8, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x724, 0x02, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x725, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x726, 0x20, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x727, 0x3A, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x728, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x729, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x72A, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x72B, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x72C, 0x20, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x72D, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x72E, 0x54, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x72F, 0x01, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x730, 0xC0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x731, 0x05, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x732, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x733, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x734, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x735, 0x34, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x736, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x737, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x738, 0x2C, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x739, 0x01, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x73A, 0x48, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x73B, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x73C, 0x6E, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x73D, 0x05, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x73E, 0xF4, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x73F, 0x76, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x740, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x750, 0x18, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x751, 0xC0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x752, 0x0E, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x753, 0x48, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x754, 0x05, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x755, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x756, 0xA0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x757, 0x69, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x758, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x759, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x75A, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x75B, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x75C, 0x20, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x75D, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x75E, 0x28, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x75F, 0x05, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x760, 0xE0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x761, 0x02, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x762, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x763, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x764, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x765, 0x34, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x766, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x767, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x768, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x769, 0x05, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x76A, 0x48, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x76B, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x76C, 0xB7, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x76D, 0x02, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x76E, 0x54, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x76F, 0xD8, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x770, 0x00, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x719, 0x27, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x02, 0x33, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x10, 0x23, 0 },
};
#endif

static const struct cmd_16bit dump_regs_config[] = {
	{ IS_HOST_I2C_ADDRESS, 0x01, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x10, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x13, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x332, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x100, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x102, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c4, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c5, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x2c6, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x308, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x311, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x333, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x55d, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x55e, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x55f, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x56a, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x380, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x388, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x389, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x3a0, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x407, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x0d, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x108, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x01, 0x02, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x03, 0x02, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x10, 0x11, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x11, 0x33, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x13, 0xea, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x3a, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1a, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1b, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1c, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1d, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1e, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1f, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x20, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x21, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x22, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x23, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x26, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x29, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x2a, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x50, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x2c, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x100, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1ce, 0x4e, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x2bc, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x013, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x1dc, 0x0, 0 },
	{ IS_DEVICE_I2C_A_ADDRESS, 0x50, 0x0, 0 },
};

static const struct cmd_16bit test_pattern_regs[] = {
	{ IS_HOST_I2C_ADDRESS, 0x102, 0x0, 0 },//PCLKDET
	{ IS_HOST_I2C_ADDRESS, 0x10A, 0x0, 0 },//PCLKDET
	{ IS_HOST_I2C_ADDRESS, 0x112, 0x0, 0 },//PCLKDET
	{ IS_HOST_I2C_ADDRESS, 0x3A0, 0x0, 0 },//DSI controller 0 error
	{ IS_HOST_I2C_ADDRESS, 0x3A2, 0x0, 0 },//DSI controller 1 error
	{ IS_HOST_I2C_ADDRESS, 0x55D, 0x0, 0 },//DSI HS VS DE
	{ IS_HOST_I2C_ADDRESS, 0x55E, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x55F, 0x0, 0 },
	{ IS_HOST_I2C_ADDRESS, 0x56A, 0x0, 0 },
};

int max96789_check_dev_id(struct max967xx_data *data)
{
	int i;
	int ret;
	int val = 0x0;

	for (i = 0; i < 20; i++) {
		ret = max967xx_read(MAX96789_DEV_ID_REG, &val,
				    data->regmap[IS_HOST_I2C_ADDRESS]);
		if (ret < 0) {
			DISP_ERROR("fail to get device id, retry = %d", i);
		}
		if (val == MAX96789_DEV_ID) {
			DISP_INFO("device id = 0x%02x", val);
			return 0;
		}
		usleep_range(500, 1000);
	}

	DISP_ERROR("fail to check device id finally");
	return -1;
}

int max96789_is_locked(struct max967xx_data *data, int port)
{
	int i;
	int ret;
	int val;
	int bit_mask = MAX96789_LOCK_MASK;

	for (i = 0; i < 1; i++) {
		ret = max967xx_read(MAX96789_CTRL3_REG, &val,
				    data->regmap[IS_HOST_I2C_ADDRESS]);
		if (ret < 0) {
			DISP_ERROR("fail to get lock status");
			return -1;
		}

		if (val & bit_mask) {
			DISP_INFO("is locked\n");
			return 1;
		}
		//TODO
		usleep_range(500, 1000);
	}

	return 0;
}

int max96789_initialize(struct max967xx_data *data)
{
	DISP_INFO("enter");
	max967xx_write_regs(data, serializer_init_configs,
			    ARRAY_SIZE(serializer_init_configs),
			    data->regmap[IS_HOST_I2C_ADDRESS]);

	return max967xx_write_regs(data, deserializer_A_init_configs,
				   ARRAY_SIZE(deserializer_A_init_configs),
				   data->regmap[IS_DEVICE_I2C_A_ADDRESS]);
}

int max96789_pre_init(struct max967xx_data *data)
{
	DISP_INFO("enter");
	return max967xx_write_regs(data, pre_init_config,
				   ARRAY_SIZE(pre_init_config),
				   data->regmap[IS_HOST_I2C_ADDRESS]);
}

int max96789_dump_regs(struct max967xx_data *data, char *buffer)
{
	int ret;
	int val = 0x0;
	int i;
	int size = 0;

	if (buffer)
		size += snprintf(buffer + size, PAGE_SIZE - size, "max96789_dump_regs:\n");

	for (i = 0; i < ARRAY_SIZE(dump_regs_config); i++) {
		ret = max967xx_read(dump_regs_config[i].reg_addr, &val,
				    data->regmap[dump_regs_config[i].addr]);
		if (ret < 0) {
			DISP_ERROR("fail to dump reg 0x%04x",
				   dump_regs_config[i].reg_addr);
			return -1;
		}
		DISP_INFO("dump reg 0x%04x val 0x%02x",
			  dump_regs_config[i].reg_addr, val);
		if (buffer)
			size += snprintf(buffer + size, PAGE_SIZE - size, "serdes_type:%d 0x%04x=0x%02x\n",
					dump_regs_config[i].addr, dump_regs_config[i].reg_addr, val);
	}

	return 0;
}

int max96789_test_pattern(struct max967xx_data *data, char *buffer)
{
	int ret;
	int val = 0x0;
	int i;
	int size = 0;

	if (buffer)
		size += snprintf(buffer + size, PAGE_SIZE - size, "max96789_test_pattern:\n");

	for (i = 0; i < ARRAY_SIZE(test_pattern_regs); i++) {
		ret = max967xx_read(test_pattern_regs[i].reg_addr, &val,
				    data->regmap[test_pattern_regs[i].addr]);
		if (ret < 0) {
			DISP_ERROR("fail to dump reg 0x%04x",
				   test_pattern_regs[i].reg_addr);
			return -1;
		}
		DISP_INFO("dump reg 0x%04x val 0x%02x",
			  test_pattern_regs[i].reg_addr, val);
		if (buffer)
			size += snprintf(buffer + size, PAGE_SIZE - size, "serdes_type:%d 0x%04x=0x%02x\n",
					test_pattern_regs[i].addr, test_pattern_regs[i].reg_addr, val);
	}

	return 0;
}
=======
>>>>>>> CHANGE (76ffd9 O82: display: opensource kernel)
