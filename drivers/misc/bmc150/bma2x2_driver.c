/*!
 * @section LICENSE
 * (C) Copyright 2013 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bma2x2.c
 * @date     2013/10/25
 * @id       "a2bd407"
 * @version  v1.7.3
 * @brief    brief This file contains all function implementations for the BMA2X2 in linux
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
#include <linux/wakelock.h>
#include <linux/gpio.h>
#endif

#include <linux/pm.h>
#include <linux/regulator/consumer.h>
#include <linux/bst_sensor_common.h>

#include "sensors_head.h"


#define SENSOR_NAME			"bma2x2"
#define ABSMIN				-512
#define ABSMAX				512
#define SLOPE_THRESHOLD_VALUE		32
#define SLOPE_DURATION_VALUE		1
#define INTERRUPT_LATCH_MODE		13
#define INTERRUPT_ENABLE		1
#define INTERRUPT_DISABLE		0
#define MAP_SLOPE_INTERRUPT		2
#define SLOPE_X_INDEX			5
#define SLOPE_Y_INDEX			6
#define SLOPE_Z_INDEX			7
#define BMA2X2_MAX_DELAY		200
#define BMA2X2_RANGE_SET		3  /* +/- 2G */
#define BMA2X2_BW_SET			12 /* 125HZ  */

#define LOW_G_INTERRUPT				REL_Z
#define HIGH_G_INTERRUPT			REL_HWHEEL
#define SLOP_INTERRUPT				REL_DIAL
#define DOUBLE_TAP_INTERRUPT			REL_WHEEL
#define SINGLE_TAP_INTERRUPT			REL_MISC
#define ORIENT_INTERRUPT			ABS_PRESSURE
#define FLAT_INTERRUPT				ABS_DISTANCE
#define SLOW_NO_MOTION_INTERRUPT		REL_Y

#define HIGH_G_INTERRUPT_X_HAPPENED			1
#define HIGH_G_INTERRUPT_Y_HAPPENED			2
#define HIGH_G_INTERRUPT_Z_HAPPENED			3
#define HIGH_G_INTERRUPT_X_NEGATIVE_HAPPENED		4
#define HIGH_G_INTERRUPT_Y_NEGATIVE_HAPPENED		5
#define HIGH_G_INTERRUPT_Z_NEGATIVE_HAPPENED		6
#define SLOPE_INTERRUPT_X_HAPPENED			7
#define SLOPE_INTERRUPT_Y_HAPPENED			8
#define SLOPE_INTERRUPT_Z_HAPPENED			9
#define SLOPE_INTERRUPT_X_NEGATIVE_HAPPENED		10
#define SLOPE_INTERRUPT_Y_NEGATIVE_HAPPENED		11
#define SLOPE_INTERRUPT_Z_NEGATIVE_HAPPENED		12
#define DOUBLE_TAP_INTERRUPT_HAPPENED			13
#define SINGLE_TAP_INTERRUPT_HAPPENED			14
#define UPWARD_PORTRAIT_UP_INTERRUPT_HAPPENED		15
#define UPWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED		16
#define UPWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED	17
#define UPWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED	18
#define DOWNWARD_PORTRAIT_UP_INTERRUPT_HAPPENED	19
#define DOWNWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED	20
#define DOWNWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED	21
#define DOWNWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED	22
#define FLAT_INTERRUPT_TURE_HAPPENED			23
#define FLAT_INTERRUPT_FALSE_HAPPENED			24
#define LOW_G_INTERRUPT_HAPPENED			25
#define SLOW_NO_MOTION_INTERRUPT_HAPPENED		26


#define PAD_LOWG					0
#define PAD_HIGHG					1
#define PAD_SLOP					2
#define PAD_DOUBLE_TAP					3
#define PAD_SINGLE_TAP					4
#define PAD_ORIENT					5
#define PAD_FLAT					6
#define PAD_SLOW_NO_MOTION				7


#define BMA2X2_EEP_OFFSET                       0x16
#define BMA2X2_IMAGE_BASE                       0x38
#define BMA2X2_IMAGE_LEN                        22


#define BMA2X2_CHIP_ID_REG                      0x00
#define BMA2X2_VERSION_REG                      0x01
#define BMA2X2_X_AXIS_LSB_REG                   0x02
#define BMA2X2_X_AXIS_MSB_REG                   0x03
#define BMA2X2_Y_AXIS_LSB_REG                   0x04
#define BMA2X2_Y_AXIS_MSB_REG                   0x05
#define BMA2X2_Z_AXIS_LSB_REG                   0x06
#define BMA2X2_Z_AXIS_MSB_REG                   0x07
#define BMA2X2_TEMPERATURE_REG                  0x08
#define BMA2X2_STATUS1_REG                      0x09
#define BMA2X2_STATUS2_REG                      0x0A
#define BMA2X2_STATUS_TAP_SLOPE_REG             0x0B
#define BMA2X2_STATUS_ORIENT_HIGH_REG           0x0C
#define BMA2X2_STATUS_FIFO_REG                  0x0E
#define BMA2X2_RANGE_SEL_REG                    0x0F
#define BMA2X2_BW_SEL_REG                       0x10
#define BMA2X2_MODE_CTRL_REG                    0x11
#define BMA2X2_LOW_NOISE_CTRL_REG               0x12
#define BMA2X2_DATA_CTRL_REG                    0x13
#define BMA2X2_RESET_REG                        0x14
#define BMA2X2_INT_ENABLE1_REG                  0x16
#define BMA2X2_INT_ENABLE2_REG                  0x17
#define BMA2X2_INT_SLO_NO_MOT_REG               0x18
#define BMA2X2_INT1_PAD_SEL_REG                 0x19
#define BMA2X2_INT_DATA_SEL_REG                 0x1A
#define BMA2X2_INT2_PAD_SEL_REG                 0x1B
#define BMA2X2_INT_SRC_REG                      0x1E
#define BMA2X2_INT_SET_REG                      0x20
#define BMA2X2_INT_CTRL_REG                     0x21
#define BMA2X2_LOW_DURN_REG                     0x22
#define BMA2X2_LOW_THRES_REG                    0x23
#define BMA2X2_LOW_HIGH_HYST_REG                0x24
#define BMA2X2_HIGH_DURN_REG                    0x25
#define BMA2X2_HIGH_THRES_REG                   0x26
#define BMA2X2_SLOPE_DURN_REG                   0x27
#define BMA2X2_SLOPE_THRES_REG                  0x28
#define BMA2X2_SLO_NO_MOT_THRES_REG             0x29
#define BMA2X2_TAP_PARAM_REG                    0x2A
#define BMA2X2_TAP_THRES_REG                    0x2B
#define BMA2X2_ORIENT_PARAM_REG                 0x2C
#define BMA2X2_THETA_BLOCK_REG                  0x2D
#define BMA2X2_THETA_FLAT_REG                   0x2E
#define BMA2X2_FLAT_HOLD_TIME_REG               0x2F
#define BMA2X2_FIFO_WML_TRIG                    0x30
#define BMA2X2_SELF_TEST_REG                    0x32
#define BMA2X2_EEPROM_CTRL_REG                  0x33
#define BMA2X2_SERIAL_CTRL_REG                  0x34
#define BMA2X2_EXTMODE_CTRL_REG                 0x35
#define BMA2X2_OFFSET_CTRL_REG                  0x36
#define BMA2X2_OFFSET_PARAMS_REG                0x37
#define BMA2X2_OFFSET_X_AXIS_REG                0x38
#define BMA2X2_OFFSET_Y_AXIS_REG                0x39
#define BMA2X2_OFFSET_Z_AXIS_REG                0x3A
#define BMA2X2_GP0_REG                          0x3B
#define BMA2X2_GP1_REG                          0x3C
#define BMA2X2_FIFO_MODE_REG                    0x3E
#define BMA2X2_FIFO_DATA_OUTPUT_REG             0x3F




#define BMA2X2_CHIP_ID__POS             0
#define BMA2X2_CHIP_ID__MSK             0xFF
#define BMA2X2_CHIP_ID__LEN             8
#define BMA2X2_CHIP_ID__REG             BMA2X2_CHIP_ID_REG

#define BMA2X2_VERSION__POS          0
#define BMA2X2_VERSION__LEN          8
#define BMA2X2_VERSION__MSK          0xFF
#define BMA2X2_VERSION__REG          BMA2X2_VERSION_REG

#define BMA2x2_SLO_NO_MOT_DUR__POS	2
#define BMA2x2_SLO_NO_MOT_DUR__LEN	6
#define BMA2x2_SLO_NO_MOT_DUR__MSK	0xFC
#define BMA2x2_SLO_NO_MOT_DUR__REG	BMA2X2_SLOPE_DURN_REG

#define BMA2X2_NEW_DATA_X__POS          0
#define BMA2X2_NEW_DATA_X__LEN          1
#define BMA2X2_NEW_DATA_X__MSK          0x01
#define BMA2X2_NEW_DATA_X__REG          BMA2X2_X_AXIS_LSB_REG

#define BMA2X2_ACC_X14_LSB__POS           2
#define BMA2X2_ACC_X14_LSB__LEN           6
#define BMA2X2_ACC_X14_LSB__MSK           0xFC
#define BMA2X2_ACC_X14_LSB__REG           BMA2X2_X_AXIS_LSB_REG

#define BMA2X2_ACC_X12_LSB__POS           4
#define BMA2X2_ACC_X12_LSB__LEN           4
#define BMA2X2_ACC_X12_LSB__MSK           0xF0
#define BMA2X2_ACC_X12_LSB__REG           BMA2X2_X_AXIS_LSB_REG

#define BMA2X2_ACC_X10_LSB__POS           6
#define BMA2X2_ACC_X10_LSB__LEN           2
#define BMA2X2_ACC_X10_LSB__MSK           0xC0
#define BMA2X2_ACC_X10_LSB__REG           BMA2X2_X_AXIS_LSB_REG

#define BMA2X2_ACC_X8_LSB__POS           0
#define BMA2X2_ACC_X8_LSB__LEN           0
#define BMA2X2_ACC_X8_LSB__MSK           0x00
#define BMA2X2_ACC_X8_LSB__REG           BMA2X2_X_AXIS_LSB_REG

#define BMA2X2_ACC_X_MSB__POS           0
#define BMA2X2_ACC_X_MSB__LEN           8
#define BMA2X2_ACC_X_MSB__MSK           0xFF
#define BMA2X2_ACC_X_MSB__REG           BMA2X2_X_AXIS_MSB_REG

#define BMA2X2_NEW_DATA_Y__POS          0
#define BMA2X2_NEW_DATA_Y__LEN          1
#define BMA2X2_NEW_DATA_Y__MSK          0x01
#define BMA2X2_NEW_DATA_Y__REG          BMA2X2_Y_AXIS_LSB_REG

#define BMA2X2_ACC_Y14_LSB__POS           2
#define BMA2X2_ACC_Y14_LSB__LEN           6
#define BMA2X2_ACC_Y14_LSB__MSK           0xFC
#define BMA2X2_ACC_Y14_LSB__REG           BMA2X2_Y_AXIS_LSB_REG

#define BMA2X2_ACC_Y12_LSB__POS           4
#define BMA2X2_ACC_Y12_LSB__LEN           4
#define BMA2X2_ACC_Y12_LSB__MSK           0xF0
#define BMA2X2_ACC_Y12_LSB__REG           BMA2X2_Y_AXIS_LSB_REG

#define BMA2X2_ACC_Y10_LSB__POS           6
#define BMA2X2_ACC_Y10_LSB__LEN           2
#define BMA2X2_ACC_Y10_LSB__MSK           0xC0
#define BMA2X2_ACC_Y10_LSB__REG           BMA2X2_Y_AXIS_LSB_REG

#define BMA2X2_ACC_Y8_LSB__POS           0
#define BMA2X2_ACC_Y8_LSB__LEN           0
#define BMA2X2_ACC_Y8_LSB__MSK           0x00
#define BMA2X2_ACC_Y8_LSB__REG           BMA2X2_Y_AXIS_LSB_REG

#define BMA2X2_ACC_Y_MSB__POS           0
#define BMA2X2_ACC_Y_MSB__LEN           8
#define BMA2X2_ACC_Y_MSB__MSK           0xFF
#define BMA2X2_ACC_Y_MSB__REG           BMA2X2_Y_AXIS_MSB_REG

#define BMA2X2_NEW_DATA_Z__POS          0
#define BMA2X2_NEW_DATA_Z__LEN          1
#define BMA2X2_NEW_DATA_Z__MSK          0x01
#define BMA2X2_NEW_DATA_Z__REG          BMA2X2_Z_AXIS_LSB_REG

#define BMA2X2_ACC_Z14_LSB__POS           2
#define BMA2X2_ACC_Z14_LSB__LEN           6
#define BMA2X2_ACC_Z14_LSB__MSK           0xFC
#define BMA2X2_ACC_Z14_LSB__REG           BMA2X2_Z_AXIS_LSB_REG

#define BMA2X2_ACC_Z12_LSB__POS           4
#define BMA2X2_ACC_Z12_LSB__LEN           4
#define BMA2X2_ACC_Z12_LSB__MSK           0xF0
#define BMA2X2_ACC_Z12_LSB__REG           BMA2X2_Z_AXIS_LSB_REG

#define BMA2X2_ACC_Z10_LSB__POS           6
#define BMA2X2_ACC_Z10_LSB__LEN           2
#define BMA2X2_ACC_Z10_LSB__MSK           0xC0
#define BMA2X2_ACC_Z10_LSB__REG           BMA2X2_Z_AXIS_LSB_REG

#define BMA2X2_ACC_Z8_LSB__POS           0
#define BMA2X2_ACC_Z8_LSB__LEN           0
#define BMA2X2_ACC_Z8_LSB__MSK           0x00
#define BMA2X2_ACC_Z8_LSB__REG           BMA2X2_Z_AXIS_LSB_REG

#define BMA2X2_ACC_Z_MSB__POS           0
#define BMA2X2_ACC_Z_MSB__LEN           8
#define BMA2X2_ACC_Z_MSB__MSK           0xFF
#define BMA2X2_ACC_Z_MSB__REG           BMA2X2_Z_AXIS_MSB_REG

#define BMA2X2_TEMPERATURE__POS         0
#define BMA2X2_TEMPERATURE__LEN         8
#define BMA2X2_TEMPERATURE__MSK         0xFF
#define BMA2X2_TEMPERATURE__REG         BMA2X2_TEMP_RD_REG

#define BMA2X2_LOWG_INT_S__POS          0
#define BMA2X2_LOWG_INT_S__LEN          1
#define BMA2X2_LOWG_INT_S__MSK          0x01
#define BMA2X2_LOWG_INT_S__REG          BMA2X2_STATUS1_REG

#define BMA2X2_HIGHG_INT_S__POS          1
#define BMA2X2_HIGHG_INT_S__LEN          1
#define BMA2X2_HIGHG_INT_S__MSK          0x02
#define BMA2X2_HIGHG_INT_S__REG          BMA2X2_STATUS1_REG

#define BMA2X2_SLOPE_INT_S__POS          2
#define BMA2X2_SLOPE_INT_S__LEN          1
#define BMA2X2_SLOPE_INT_S__MSK          0x04
#define BMA2X2_SLOPE_INT_S__REG          BMA2X2_STATUS1_REG


#define BMA2X2_SLO_NO_MOT_INT_S__POS          3
#define BMA2X2_SLO_NO_MOT_INT_S__LEN          1
#define BMA2X2_SLO_NO_MOT_INT_S__MSK          0x08
#define BMA2X2_SLO_NO_MOT_INT_S__REG          BMA2X2_STATUS1_REG

#define BMA2X2_DOUBLE_TAP_INT_S__POS     4
#define BMA2X2_DOUBLE_TAP_INT_S__LEN     1
#define BMA2X2_DOUBLE_TAP_INT_S__MSK     0x10
#define BMA2X2_DOUBLE_TAP_INT_S__REG     BMA2X2_STATUS1_REG

#define BMA2X2_SINGLE_TAP_INT_S__POS     5
#define BMA2X2_SINGLE_TAP_INT_S__LEN     1
#define BMA2X2_SINGLE_TAP_INT_S__MSK     0x20
#define BMA2X2_SINGLE_TAP_INT_S__REG     BMA2X2_STATUS1_REG

#define BMA2X2_ORIENT_INT_S__POS         6
#define BMA2X2_ORIENT_INT_S__LEN         1
#define BMA2X2_ORIENT_INT_S__MSK         0x40
#define BMA2X2_ORIENT_INT_S__REG         BMA2X2_STATUS1_REG

#define BMA2X2_FLAT_INT_S__POS           7
#define BMA2X2_FLAT_INT_S__LEN           1
#define BMA2X2_FLAT_INT_S__MSK           0x80
#define BMA2X2_FLAT_INT_S__REG           BMA2X2_STATUS1_REG

#define BMA2X2_FIFO_FULL_INT_S__POS           5
#define BMA2X2_FIFO_FULL_INT_S__LEN           1
#define BMA2X2_FIFO_FULL_INT_S__MSK           0x20
#define BMA2X2_FIFO_FULL_INT_S__REG           BMA2X2_STATUS2_REG

#define BMA2X2_FIFO_WM_INT_S__POS           6
#define BMA2X2_FIFO_WM_INT_S__LEN           1
#define BMA2X2_FIFO_WM_INT_S__MSK           0x40
#define BMA2X2_FIFO_WM_INT_S__REG           BMA2X2_STATUS2_REG

#define BMA2X2_DATA_INT_S__POS           7
#define BMA2X2_DATA_INT_S__LEN           1
#define BMA2X2_DATA_INT_S__MSK           0x80
#define BMA2X2_DATA_INT_S__REG           BMA2X2_STATUS2_REG

#define BMA2X2_SLOPE_FIRST_X__POS        0
#define BMA2X2_SLOPE_FIRST_X__LEN        1
#define BMA2X2_SLOPE_FIRST_X__MSK        0x01
#define BMA2X2_SLOPE_FIRST_X__REG        BMA2X2_STATUS_TAP_SLOPE_REG

#define BMA2X2_SLOPE_FIRST_Y__POS        1
#define BMA2X2_SLOPE_FIRST_Y__LEN        1
#define BMA2X2_SLOPE_FIRST_Y__MSK        0x02
#define BMA2X2_SLOPE_FIRST_Y__REG        BMA2X2_STATUS_TAP_SLOPE_REG

#define BMA2X2_SLOPE_FIRST_Z__POS        2
#define BMA2X2_SLOPE_FIRST_Z__LEN        1
#define BMA2X2_SLOPE_FIRST_Z__MSK        0x04
#define BMA2X2_SLOPE_FIRST_Z__REG        BMA2X2_STATUS_TAP_SLOPE_REG

#define BMA2X2_SLOPE_SIGN_S__POS         3
#define BMA2X2_SLOPE_SIGN_S__LEN         1
#define BMA2X2_SLOPE_SIGN_S__MSK         0x08
#define BMA2X2_SLOPE_SIGN_S__REG         BMA2X2_STATUS_TAP_SLOPE_REG

#define BMA2X2_TAP_FIRST_X__POS        4
#define BMA2X2_TAP_FIRST_X__LEN        1
#define BMA2X2_TAP_FIRST_X__MSK        0x10
#define BMA2X2_TAP_FIRST_X__REG        BMA2X2_STATUS_TAP_SLOPE_REG

#define BMA2X2_TAP_FIRST_Y__POS        5
#define BMA2X2_TAP_FIRST_Y__LEN        1
#define BMA2X2_TAP_FIRST_Y__MSK        0x20
#define BMA2X2_TAP_FIRST_Y__REG        BMA2X2_STATUS_TAP_SLOPE_REG

#define BMA2X2_TAP_FIRST_Z__POS        6
#define BMA2X2_TAP_FIRST_Z__LEN        1
#define BMA2X2_TAP_FIRST_Z__MSK        0x40
#define BMA2X2_TAP_FIRST_Z__REG        BMA2X2_STATUS_TAP_SLOPE_REG

#define BMA2X2_TAP_SIGN_S__POS         7
#define BMA2X2_TAP_SIGN_S__LEN         1
#define BMA2X2_TAP_SIGN_S__MSK         0x80
#define BMA2X2_TAP_SIGN_S__REG         BMA2X2_STATUS_TAP_SLOPE_REG

#define BMA2X2_HIGHG_FIRST_X__POS        0
#define BMA2X2_HIGHG_FIRST_X__LEN        1
#define BMA2X2_HIGHG_FIRST_X__MSK        0x01
#define BMA2X2_HIGHG_FIRST_X__REG        BMA2X2_STATUS_ORIENT_HIGH_REG

#define BMA2X2_HIGHG_FIRST_Y__POS        1
#define BMA2X2_HIGHG_FIRST_Y__LEN        1
#define BMA2X2_HIGHG_FIRST_Y__MSK        0x02
#define BMA2X2_HIGHG_FIRST_Y__REG        BMA2X2_STATUS_ORIENT_HIGH_REG

#define BMA2X2_HIGHG_FIRST_Z__POS        2
#define BMA2X2_HIGHG_FIRST_Z__LEN        1
#define BMA2X2_HIGHG_FIRST_Z__MSK        0x04
#define BMA2X2_HIGHG_FIRST_Z__REG        BMA2X2_STATUS_ORIENT_HIGH_REG

#define BMA2X2_HIGHG_SIGN_S__POS         3
#define BMA2X2_HIGHG_SIGN_S__LEN         1
#define BMA2X2_HIGHG_SIGN_S__MSK         0x08
#define BMA2X2_HIGHG_SIGN_S__REG         BMA2X2_STATUS_ORIENT_HIGH_REG

#define BMA2X2_ORIENT_S__POS             4
#define BMA2X2_ORIENT_S__LEN             3
#define BMA2X2_ORIENT_S__MSK             0x70
#define BMA2X2_ORIENT_S__REG             BMA2X2_STATUS_ORIENT_HIGH_REG

#define BMA2X2_FLAT_S__POS               7
#define BMA2X2_FLAT_S__LEN               1
#define BMA2X2_FLAT_S__MSK               0x80
#define BMA2X2_FLAT_S__REG               BMA2X2_STATUS_ORIENT_HIGH_REG

#define BMA2X2_FIFO_FRAME_COUNTER_S__POS             0
#define BMA2X2_FIFO_FRAME_COUNTER_S__LEN             7
#define BMA2X2_FIFO_FRAME_COUNTER_S__MSK             0x7F
#define BMA2X2_FIFO_FRAME_COUNTER_S__REG             BMA2X2_STATUS_FIFO_REG

#define BMA2X2_FIFO_OVERRUN_S__POS             7
#define BMA2X2_FIFO_OVERRUN_S__LEN             1
#define BMA2X2_FIFO_OVERRUN_S__MSK             0x80
#define BMA2X2_FIFO_OVERRUN_S__REG             BMA2X2_STATUS_FIFO_REG

#define BMA2X2_RANGE_SEL__POS             0
#define BMA2X2_RANGE_SEL__LEN             4
#define BMA2X2_RANGE_SEL__MSK             0x0F
#define BMA2X2_RANGE_SEL__REG             BMA2X2_RANGE_SEL_REG

#define BMA2X2_BANDWIDTH__POS             0
#define BMA2X2_BANDWIDTH__LEN             5
#define BMA2X2_BANDWIDTH__MSK             0x1F
#define BMA2X2_BANDWIDTH__REG             BMA2X2_BW_SEL_REG

#define BMA2X2_SLEEP_DUR__POS             1
#define BMA2X2_SLEEP_DUR__LEN             4
#define BMA2X2_SLEEP_DUR__MSK             0x1E
#define BMA2X2_SLEEP_DUR__REG             BMA2X2_MODE_CTRL_REG

#define BMA2X2_MODE_CTRL__POS             5
#define BMA2X2_MODE_CTRL__LEN             3
#define BMA2X2_MODE_CTRL__MSK             0xE0
#define BMA2X2_MODE_CTRL__REG             BMA2X2_MODE_CTRL_REG

#define BMA2X2_DEEP_SUSPEND__POS          5
#define BMA2X2_DEEP_SUSPEND__LEN          1
#define BMA2X2_DEEP_SUSPEND__MSK          0x20
#define BMA2X2_DEEP_SUSPEND__REG          BMA2X2_MODE_CTRL_REG

#define BMA2X2_EN_LOW_POWER__POS          6
#define BMA2X2_EN_LOW_POWER__LEN          1
#define BMA2X2_EN_LOW_POWER__MSK          0x40
#define BMA2X2_EN_LOW_POWER__REG          BMA2X2_MODE_CTRL_REG

#define BMA2X2_EN_SUSPEND__POS            7
#define BMA2X2_EN_SUSPEND__LEN            1
#define BMA2X2_EN_SUSPEND__MSK            0x80
#define BMA2X2_EN_SUSPEND__REG            BMA2X2_MODE_CTRL_REG

#define BMA2X2_SLEEP_TIMER__POS          5
#define BMA2X2_SLEEP_TIMER__LEN          1
#define BMA2X2_SLEEP_TIMER__MSK          0x20
#define BMA2X2_SLEEP_TIMER__REG          BMA2X2_LOW_NOISE_CTRL_REG

#define BMA2X2_LOW_POWER_MODE__POS          6
#define BMA2X2_LOW_POWER_MODE__LEN          1
#define BMA2X2_LOW_POWER_MODE__MSK          0x40
#define BMA2X2_LOW_POWER_MODE__REG          BMA2X2_LOW_NOISE_CTRL_REG

#define BMA2X2_EN_LOW_NOISE__POS          7
#define BMA2X2_EN_LOW_NOISE__LEN          1
#define BMA2X2_EN_LOW_NOISE__MSK          0x80
#define BMA2X2_EN_LOW_NOISE__REG          BMA2X2_LOW_NOISE_CTRL_REG

#define BMA2X2_DIS_SHADOW_PROC__POS       6
#define BMA2X2_DIS_SHADOW_PROC__LEN       1
#define BMA2X2_DIS_SHADOW_PROC__MSK       0x40
#define BMA2X2_DIS_SHADOW_PROC__REG       BMA2X2_DATA_CTRL_REG

#define BMA2X2_EN_DATA_HIGH_BW__POS         7
#define BMA2X2_EN_DATA_HIGH_BW__LEN         1
#define BMA2X2_EN_DATA_HIGH_BW__MSK         0x80
#define BMA2X2_EN_DATA_HIGH_BW__REG         BMA2X2_DATA_CTRL_REG

#define BMA2X2_EN_SOFT_RESET__POS         0
#define BMA2X2_EN_SOFT_RESET__LEN         8
#define BMA2X2_EN_SOFT_RESET__MSK         0xFF
#define BMA2X2_EN_SOFT_RESET__REG         BMA2X2_RESET_REG

#define BMA2X2_EN_SOFT_RESET_VALUE        0xB6

#define BMA2X2_EN_SLOPE_X_INT__POS         0
#define BMA2X2_EN_SLOPE_X_INT__LEN         1
#define BMA2X2_EN_SLOPE_X_INT__MSK         0x01
#define BMA2X2_EN_SLOPE_X_INT__REG         BMA2X2_INT_ENABLE1_REG

#define BMA2X2_EN_SLOPE_Y_INT__POS         1
#define BMA2X2_EN_SLOPE_Y_INT__LEN         1
#define BMA2X2_EN_SLOPE_Y_INT__MSK         0x02
#define BMA2X2_EN_SLOPE_Y_INT__REG         BMA2X2_INT_ENABLE1_REG

#define BMA2X2_EN_SLOPE_Z_INT__POS         2
#define BMA2X2_EN_SLOPE_Z_INT__LEN         1
#define BMA2X2_EN_SLOPE_Z_INT__MSK         0x04
#define BMA2X2_EN_SLOPE_Z_INT__REG         BMA2X2_INT_ENABLE1_REG

#define BMA2X2_EN_DOUBLE_TAP_INT__POS      4
#define BMA2X2_EN_DOUBLE_TAP_INT__LEN      1
#define BMA2X2_EN_DOUBLE_TAP_INT__MSK      0x10
#define BMA2X2_EN_DOUBLE_TAP_INT__REG      BMA2X2_INT_ENABLE1_REG

#define BMA2X2_EN_SINGLE_TAP_INT__POS      5
#define BMA2X2_EN_SINGLE_TAP_INT__LEN      1
#define BMA2X2_EN_SINGLE_TAP_INT__MSK      0x20
#define BMA2X2_EN_SINGLE_TAP_INT__REG      BMA2X2_INT_ENABLE1_REG

#define BMA2X2_EN_ORIENT_INT__POS          6
#define BMA2X2_EN_ORIENT_INT__LEN          1
#define BMA2X2_EN_ORIENT_INT__MSK          0x40
#define BMA2X2_EN_ORIENT_INT__REG          BMA2X2_INT_ENABLE1_REG

#define BMA2X2_EN_FLAT_INT__POS            7
#define BMA2X2_EN_FLAT_INT__LEN            1
#define BMA2X2_EN_FLAT_INT__MSK            0x80
#define BMA2X2_EN_FLAT_INT__REG            BMA2X2_INT_ENABLE1_REG

#define BMA2X2_EN_HIGHG_X_INT__POS         0
#define BMA2X2_EN_HIGHG_X_INT__LEN         1
#define BMA2X2_EN_HIGHG_X_INT__MSK         0x01
#define BMA2X2_EN_HIGHG_X_INT__REG         BMA2X2_INT_ENABLE2_REG

#define BMA2X2_EN_HIGHG_Y_INT__POS         1
#define BMA2X2_EN_HIGHG_Y_INT__LEN         1
#define BMA2X2_EN_HIGHG_Y_INT__MSK         0x02
#define BMA2X2_EN_HIGHG_Y_INT__REG         BMA2X2_INT_ENABLE2_REG

#define BMA2X2_EN_HIGHG_Z_INT__POS         2
#define BMA2X2_EN_HIGHG_Z_INT__LEN         1
#define BMA2X2_EN_HIGHG_Z_INT__MSK         0x04
#define BMA2X2_EN_HIGHG_Z_INT__REG         BMA2X2_INT_ENABLE2_REG

#define BMA2X2_EN_LOWG_INT__POS            3
#define BMA2X2_EN_LOWG_INT__LEN            1
#define BMA2X2_EN_LOWG_INT__MSK            0x08
#define BMA2X2_EN_LOWG_INT__REG            BMA2X2_INT_ENABLE2_REG

#define BMA2X2_EN_NEW_DATA_INT__POS        4
#define BMA2X2_EN_NEW_DATA_INT__LEN        1
#define BMA2X2_EN_NEW_DATA_INT__MSK        0x10
#define BMA2X2_EN_NEW_DATA_INT__REG        BMA2X2_INT_ENABLE2_REG

#define BMA2X2_INT_FFULL_EN_INT__POS        5
#define BMA2X2_INT_FFULL_EN_INT__LEN        1
#define BMA2X2_INT_FFULL_EN_INT__MSK        0x20
#define BMA2X2_INT_FFULL_EN_INT__REG        BMA2X2_INT_ENABLE2_REG

#define BMA2X2_INT_FWM_EN_INT__POS        6
#define BMA2X2_INT_FWM_EN_INT__LEN        1
#define BMA2X2_INT_FWM_EN_INT__MSK        0x40
#define BMA2X2_INT_FWM_EN_INT__REG        BMA2X2_INT_ENABLE2_REG

#define BMA2X2_INT_SLO_NO_MOT_EN_X_INT__POS        0
#define BMA2X2_INT_SLO_NO_MOT_EN_X_INT__LEN        1
#define BMA2X2_INT_SLO_NO_MOT_EN_X_INT__MSK        0x01
#define BMA2X2_INT_SLO_NO_MOT_EN_X_INT__REG        BMA2X2_INT_SLO_NO_MOT_REG

#define BMA2X2_INT_SLO_NO_MOT_EN_Y_INT__POS        1
#define BMA2X2_INT_SLO_NO_MOT_EN_Y_INT__LEN        1
#define BMA2X2_INT_SLO_NO_MOT_EN_Y_INT__MSK        0x02
#define BMA2X2_INT_SLO_NO_MOT_EN_Y_INT__REG        BMA2X2_INT_SLO_NO_MOT_REG

#define BMA2X2_INT_SLO_NO_MOT_EN_Z_INT__POS        2
#define BMA2X2_INT_SLO_NO_MOT_EN_Z_INT__LEN        1
#define BMA2X2_INT_SLO_NO_MOT_EN_Z_INT__MSK        0x04
#define BMA2X2_INT_SLO_NO_MOT_EN_Z_INT__REG        BMA2X2_INT_SLO_NO_MOT_REG

#define BMA2X2_INT_SLO_NO_MOT_EN_SEL_INT__POS        3
#define BMA2X2_INT_SLO_NO_MOT_EN_SEL_INT__LEN        1
#define BMA2X2_INT_SLO_NO_MOT_EN_SEL_INT__MSK        0x08
#define BMA2X2_INT_SLO_NO_MOT_EN_SEL_INT__REG        BMA2X2_INT_SLO_NO_MOT_REG

#define BMA2X2_EN_INT1_PAD_LOWG__POS        0
#define BMA2X2_EN_INT1_PAD_LOWG__LEN        1
#define BMA2X2_EN_INT1_PAD_LOWG__MSK        0x01
#define BMA2X2_EN_INT1_PAD_LOWG__REG        BMA2X2_INT1_PAD_SEL_REG

#define BMA2X2_EN_INT1_PAD_HIGHG__POS       1
#define BMA2X2_EN_INT1_PAD_HIGHG__LEN       1
#define BMA2X2_EN_INT1_PAD_HIGHG__MSK       0x02
#define BMA2X2_EN_INT1_PAD_HIGHG__REG       BMA2X2_INT1_PAD_SEL_REG

#define BMA2X2_EN_INT1_PAD_SLOPE__POS       2
#define BMA2X2_EN_INT1_PAD_SLOPE__LEN       1
#define BMA2X2_EN_INT1_PAD_SLOPE__MSK       0x04
#define BMA2X2_EN_INT1_PAD_SLOPE__REG       BMA2X2_INT1_PAD_SEL_REG

#define BMA2X2_EN_INT1_PAD_SLO_NO_MOT__POS        3
#define BMA2X2_EN_INT1_PAD_SLO_NO_MOT__LEN        1
#define BMA2X2_EN_INT1_PAD_SLO_NO_MOT__MSK        0x08
#define BMA2X2_EN_INT1_PAD_SLO_NO_MOT__REG        BMA2X2_INT1_PAD_SEL_REG

#define BMA2X2_EN_INT1_PAD_DB_TAP__POS      4
#define BMA2X2_EN_INT1_PAD_DB_TAP__LEN      1
#define BMA2X2_EN_INT1_PAD_DB_TAP__MSK      0x10
#define BMA2X2_EN_INT1_PAD_DB_TAP__REG      BMA2X2_INT1_PAD_SEL_REG

#define BMA2X2_EN_INT1_PAD_SNG_TAP__POS     5
#define BMA2X2_EN_INT1_PAD_SNG_TAP__LEN     1
#define BMA2X2_EN_INT1_PAD_SNG_TAP__MSK     0x20
#define BMA2X2_EN_INT1_PAD_SNG_TAP__REG     BMA2X2_INT1_PAD_SEL_REG

#define BMA2X2_EN_INT1_PAD_ORIENT__POS      6
#define BMA2X2_EN_INT1_PAD_ORIENT__LEN      1
#define BMA2X2_EN_INT1_PAD_ORIENT__MSK      0x40
#define BMA2X2_EN_INT1_PAD_ORIENT__REG      BMA2X2_INT1_PAD_SEL_REG

#define BMA2X2_EN_INT1_PAD_FLAT__POS        7
#define BMA2X2_EN_INT1_PAD_FLAT__LEN        1
#define BMA2X2_EN_INT1_PAD_FLAT__MSK        0x80
#define BMA2X2_EN_INT1_PAD_FLAT__REG        BMA2X2_INT1_PAD_SEL_REG

#define BMA2X2_EN_INT2_PAD_LOWG__POS        0
#define BMA2X2_EN_INT2_PAD_LOWG__LEN        1
#define BMA2X2_EN_INT2_PAD_LOWG__MSK        0x01
#define BMA2X2_EN_INT2_PAD_LOWG__REG        BMA2X2_INT2_PAD_SEL_REG

#define BMA2X2_EN_INT2_PAD_HIGHG__POS       1
#define BMA2X2_EN_INT2_PAD_HIGHG__LEN       1
#define BMA2X2_EN_INT2_PAD_HIGHG__MSK       0x02
#define BMA2X2_EN_INT2_PAD_HIGHG__REG       BMA2X2_INT2_PAD_SEL_REG

#define BMA2X2_EN_INT2_PAD_SLOPE__POS       2
#define BMA2X2_EN_INT2_PAD_SLOPE__LEN       1
#define BMA2X2_EN_INT2_PAD_SLOPE__MSK       0x04
#define BMA2X2_EN_INT2_PAD_SLOPE__REG       BMA2X2_INT2_PAD_SEL_REG

#define BMA2X2_EN_INT2_PAD_SLO_NO_MOT__POS        3
#define BMA2X2_EN_INT2_PAD_SLO_NO_MOT__LEN        1
#define BMA2X2_EN_INT2_PAD_SLO_NO_MOT__MSK        0x08
#define BMA2X2_EN_INT2_PAD_SLO_NO_MOT__REG        BMA2X2_INT2_PAD_SEL_REG

#define BMA2X2_EN_INT2_PAD_DB_TAP__POS      4
#define BMA2X2_EN_INT2_PAD_DB_TAP__LEN      1
#define BMA2X2_EN_INT2_PAD_DB_TAP__MSK      0x10
#define BMA2X2_EN_INT2_PAD_DB_TAP__REG      BMA2X2_INT2_PAD_SEL_REG

#define BMA2X2_EN_INT2_PAD_SNG_TAP__POS     5
#define BMA2X2_EN_INT2_PAD_SNG_TAP__LEN     1
#define BMA2X2_EN_INT2_PAD_SNG_TAP__MSK     0x20
#define BMA2X2_EN_INT2_PAD_SNG_TAP__REG     BMA2X2_INT2_PAD_SEL_REG

#define BMA2X2_EN_INT2_PAD_ORIENT__POS      6
#define BMA2X2_EN_INT2_PAD_ORIENT__LEN      1
#define BMA2X2_EN_INT2_PAD_ORIENT__MSK      0x40
#define BMA2X2_EN_INT2_PAD_ORIENT__REG      BMA2X2_INT2_PAD_SEL_REG

#define BMA2X2_EN_INT2_PAD_FLAT__POS        7
#define BMA2X2_EN_INT2_PAD_FLAT__LEN        1
#define BMA2X2_EN_INT2_PAD_FLAT__MSK        0x80
#define BMA2X2_EN_INT2_PAD_FLAT__REG        BMA2X2_INT2_PAD_SEL_REG

#define BMA2X2_EN_INT1_PAD_NEWDATA__POS     0
#define BMA2X2_EN_INT1_PAD_NEWDATA__LEN     1
#define BMA2X2_EN_INT1_PAD_NEWDATA__MSK     0x01
#define BMA2X2_EN_INT1_PAD_NEWDATA__REG     BMA2X2_INT_DATA_SEL_REG

#define BMA2X2_EN_INT1_PAD_FWM__POS     1
#define BMA2X2_EN_INT1_PAD_FWM__LEN     1
#define BMA2X2_EN_INT1_PAD_FWM__MSK     0x02
#define BMA2X2_EN_INT1_PAD_FWM__REG     BMA2X2_INT_DATA_SEL_REG

#define BMA2X2_EN_INT1_PAD_FFULL__POS     2
#define BMA2X2_EN_INT1_PAD_FFULL__LEN     1
#define BMA2X2_EN_INT1_PAD_FFULL__MSK     0x04
#define BMA2X2_EN_INT1_PAD_FFULL__REG     BMA2X2_INT_DATA_SEL_REG

#define BMA2X2_EN_INT2_PAD_FFULL__POS     5
#define BMA2X2_EN_INT2_PAD_FFULL__LEN     1
#define BMA2X2_EN_INT2_PAD_FFULL__MSK     0x20
#define BMA2X2_EN_INT2_PAD_FFULL__REG     BMA2X2_INT_DATA_SEL_REG

#define BMA2X2_EN_INT2_PAD_FWM__POS     6
#define BMA2X2_EN_INT2_PAD_FWM__LEN     1
#define BMA2X2_EN_INT2_PAD_FWM__MSK     0x40
#define BMA2X2_EN_INT2_PAD_FWM__REG     BMA2X2_INT_DATA_SEL_REG

#define BMA2X2_EN_INT2_PAD_NEWDATA__POS     7
#define BMA2X2_EN_INT2_PAD_NEWDATA__LEN     1
#define BMA2X2_EN_INT2_PAD_NEWDATA__MSK     0x80
#define BMA2X2_EN_INT2_PAD_NEWDATA__REG     BMA2X2_INT_DATA_SEL_REG

#define BMA2X2_UNFILT_INT_SRC_LOWG__POS        0
#define BMA2X2_UNFILT_INT_SRC_LOWG__LEN        1
#define BMA2X2_UNFILT_INT_SRC_LOWG__MSK        0x01
#define BMA2X2_UNFILT_INT_SRC_LOWG__REG        BMA2X2_INT_SRC_REG

#define BMA2X2_UNFILT_INT_SRC_HIGHG__POS       1
#define BMA2X2_UNFILT_INT_SRC_HIGHG__LEN       1
#define BMA2X2_UNFILT_INT_SRC_HIGHG__MSK       0x02
#define BMA2X2_UNFILT_INT_SRC_HIGHG__REG       BMA2X2_INT_SRC_REG

#define BMA2X2_UNFILT_INT_SRC_SLOPE__POS       2
#define BMA2X2_UNFILT_INT_SRC_SLOPE__LEN       1
#define BMA2X2_UNFILT_INT_SRC_SLOPE__MSK       0x04
#define BMA2X2_UNFILT_INT_SRC_SLOPE__REG       BMA2X2_INT_SRC_REG

#define BMA2X2_UNFILT_INT_SRC_SLO_NO_MOT__POS        3
#define BMA2X2_UNFILT_INT_SRC_SLO_NO_MOT__LEN        1
#define BMA2X2_UNFILT_INT_SRC_SLO_NO_MOT__MSK        0x08
#define BMA2X2_UNFILT_INT_SRC_SLO_NO_MOT__REG        BMA2X2_INT_SRC_REG

#define BMA2X2_UNFILT_INT_SRC_TAP__POS         4
#define BMA2X2_UNFILT_INT_SRC_TAP__LEN         1
#define BMA2X2_UNFILT_INT_SRC_TAP__MSK         0x10
#define BMA2X2_UNFILT_INT_SRC_TAP__REG         BMA2X2_INT_SRC_REG

#define BMA2X2_UNFILT_INT_SRC_DATA__POS        5
#define BMA2X2_UNFILT_INT_SRC_DATA__LEN        1
#define BMA2X2_UNFILT_INT_SRC_DATA__MSK        0x20
#define BMA2X2_UNFILT_INT_SRC_DATA__REG        BMA2X2_INT_SRC_REG

#define BMA2X2_INT1_PAD_ACTIVE_LEVEL__POS       0
#define BMA2X2_INT1_PAD_ACTIVE_LEVEL__LEN       1
#define BMA2X2_INT1_PAD_ACTIVE_LEVEL__MSK       0x01
#define BMA2X2_INT1_PAD_ACTIVE_LEVEL__REG       BMA2X2_INT_SET_REG

#define BMA2X2_INT2_PAD_ACTIVE_LEVEL__POS       2
#define BMA2X2_INT2_PAD_ACTIVE_LEVEL__LEN       1
#define BMA2X2_INT2_PAD_ACTIVE_LEVEL__MSK       0x04
#define BMA2X2_INT2_PAD_ACTIVE_LEVEL__REG       BMA2X2_INT_SET_REG

#define BMA2X2_INT1_PAD_OUTPUT_TYPE__POS        1
#define BMA2X2_INT1_PAD_OUTPUT_TYPE__LEN        1
#define BMA2X2_INT1_PAD_OUTPUT_TYPE__MSK        0x02
#define BMA2X2_INT1_PAD_OUTPUT_TYPE__REG        BMA2X2_INT_SET_REG

#define BMA2X2_INT2_PAD_OUTPUT_TYPE__POS        3
#define BMA2X2_INT2_PAD_OUTPUT_TYPE__LEN        1
#define BMA2X2_INT2_PAD_OUTPUT_TYPE__MSK        0x08
#define BMA2X2_INT2_PAD_OUTPUT_TYPE__REG        BMA2X2_INT_SET_REG

#define BMA2X2_INT_MODE_SEL__POS                0
#define BMA2X2_INT_MODE_SEL__LEN                4
#define BMA2X2_INT_MODE_SEL__MSK                0x0F
#define BMA2X2_INT_MODE_SEL__REG                BMA2X2_INT_CTRL_REG

#define BMA2X2_RESET_INT__POS           7
#define BMA2X2_RESET_INT__LEN           1
#define BMA2X2_RESET_INT__MSK           0x80
#define BMA2X2_RESET_INT__REG           BMA2X2_INT_CTRL_REG

#define BMA2X2_LOWG_DUR__POS                    0
#define BMA2X2_LOWG_DUR__LEN                    8
#define BMA2X2_LOWG_DUR__MSK                    0xFF
#define BMA2X2_LOWG_DUR__REG                    BMA2X2_LOW_DURN_REG

#define BMA2X2_LOWG_THRES__POS                  0
#define BMA2X2_LOWG_THRES__LEN                  8
#define BMA2X2_LOWG_THRES__MSK                  0xFF
#define BMA2X2_LOWG_THRES__REG                  BMA2X2_LOW_THRES_REG

#define BMA2X2_LOWG_HYST__POS                   0
#define BMA2X2_LOWG_HYST__LEN                   2
#define BMA2X2_LOWG_HYST__MSK                   0x03
#define BMA2X2_LOWG_HYST__REG                   BMA2X2_LOW_HIGH_HYST_REG

#define BMA2X2_LOWG_INT_MODE__POS               2
#define BMA2X2_LOWG_INT_MODE__LEN               1
#define BMA2X2_LOWG_INT_MODE__MSK               0x04
#define BMA2X2_LOWG_INT_MODE__REG               BMA2X2_LOW_HIGH_HYST_REG

#define BMA2X2_HIGHG_DUR__POS                    0
#define BMA2X2_HIGHG_DUR__LEN                    8
#define BMA2X2_HIGHG_DUR__MSK                    0xFF
#define BMA2X2_HIGHG_DUR__REG                    BMA2X2_HIGH_DURN_REG

#define BMA2X2_HIGHG_THRES__POS                  0
#define BMA2X2_HIGHG_THRES__LEN                  8
#define BMA2X2_HIGHG_THRES__MSK                  0xFF
#define BMA2X2_HIGHG_THRES__REG                  BMA2X2_HIGH_THRES_REG

#define BMA2X2_HIGHG_HYST__POS                  6
#define BMA2X2_HIGHG_HYST__LEN                  2
#define BMA2X2_HIGHG_HYST__MSK                  0xC0
#define BMA2X2_HIGHG_HYST__REG                  BMA2X2_LOW_HIGH_HYST_REG

#define BMA2X2_SLOPE_DUR__POS                    0
#define BMA2X2_SLOPE_DUR__LEN                    2
#define BMA2X2_SLOPE_DUR__MSK                    0x03
#define BMA2X2_SLOPE_DUR__REG                    BMA2X2_SLOPE_DURN_REG

#define BMA2X2_SLO_NO_MOT_DUR__POS                    2
#define BMA2X2_SLO_NO_MOT_DUR__LEN                    6
#define BMA2X2_SLO_NO_MOT_DUR__MSK                    0xFC
#define BMA2X2_SLO_NO_MOT_DUR__REG                    BMA2X2_SLOPE_DURN_REG

#define BMA2X2_SLOPE_THRES__POS                  0
#define BMA2X2_SLOPE_THRES__LEN                  8
#define BMA2X2_SLOPE_THRES__MSK                  0xFF
#define BMA2X2_SLOPE_THRES__REG                  BMA2X2_SLOPE_THRES_REG

#define BMA2X2_SLO_NO_MOT_THRES__POS                  0
#define BMA2X2_SLO_NO_MOT_THRES__LEN                  8
#define BMA2X2_SLO_NO_MOT_THRES__MSK                  0xFF
#define BMA2X2_SLO_NO_MOT_THRES__REG		BMA2X2_SLO_NO_MOT_THRES_REG

#define BMA2X2_TAP_DUR__POS                    0
#define BMA2X2_TAP_DUR__LEN                    3
#define BMA2X2_TAP_DUR__MSK                    0x07
#define BMA2X2_TAP_DUR__REG                    BMA2X2_TAP_PARAM_REG

#define BMA2X2_TAP_SHOCK_DURN__POS             6
#define BMA2X2_TAP_SHOCK_DURN__LEN             1
#define BMA2X2_TAP_SHOCK_DURN__MSK             0x40
#define BMA2X2_TAP_SHOCK_DURN__REG             BMA2X2_TAP_PARAM_REG

#define BMA2X2_ADV_TAP_INT__POS                5
#define BMA2X2_ADV_TAP_INT__LEN                1
#define BMA2X2_ADV_TAP_INT__MSK                0x20
#define BMA2X2_ADV_TAP_INT__REG                BMA2X2_TAP_PARAM_REG

#define BMA2X2_TAP_QUIET_DURN__POS             7
#define BMA2X2_TAP_QUIET_DURN__LEN             1
#define BMA2X2_TAP_QUIET_DURN__MSK             0x80
#define BMA2X2_TAP_QUIET_DURN__REG             BMA2X2_TAP_PARAM_REG

#define BMA2X2_TAP_THRES__POS                  0
#define BMA2X2_TAP_THRES__LEN                  5
#define BMA2X2_TAP_THRES__MSK                  0x1F
#define BMA2X2_TAP_THRES__REG                  BMA2X2_TAP_THRES_REG

#define BMA2X2_TAP_SAMPLES__POS                6
#define BMA2X2_TAP_SAMPLES__LEN                2
#define BMA2X2_TAP_SAMPLES__MSK                0xC0
#define BMA2X2_TAP_SAMPLES__REG                BMA2X2_TAP_THRES_REG

#define BMA2X2_ORIENT_MODE__POS                  0
#define BMA2X2_ORIENT_MODE__LEN                  2
#define BMA2X2_ORIENT_MODE__MSK                  0x03
#define BMA2X2_ORIENT_MODE__REG                  BMA2X2_ORIENT_PARAM_REG

#define BMA2X2_ORIENT_BLOCK__POS                 2
#define BMA2X2_ORIENT_BLOCK__LEN                 2
#define BMA2X2_ORIENT_BLOCK__MSK                 0x0C
#define BMA2X2_ORIENT_BLOCK__REG                 BMA2X2_ORIENT_PARAM_REG

#define BMA2X2_ORIENT_HYST__POS                  4
#define BMA2X2_ORIENT_HYST__LEN                  3
#define BMA2X2_ORIENT_HYST__MSK                  0x70
#define BMA2X2_ORIENT_HYST__REG                  BMA2X2_ORIENT_PARAM_REG

#define BMA2X2_ORIENT_AXIS__POS                  7
#define BMA2X2_ORIENT_AXIS__LEN                  1
#define BMA2X2_ORIENT_AXIS__MSK                  0x80
#define BMA2X2_ORIENT_AXIS__REG                  BMA2X2_THETA_BLOCK_REG

#define BMA2X2_ORIENT_UD_EN__POS                  6
#define BMA2X2_ORIENT_UD_EN__LEN                  1
#define BMA2X2_ORIENT_UD_EN__MSK                  0x40
#define BMA2X2_ORIENT_UD_EN__REG                  BMA2X2_THETA_BLOCK_REG

#define BMA2X2_THETA_BLOCK__POS                  0
#define BMA2X2_THETA_BLOCK__LEN                  6
#define BMA2X2_THETA_BLOCK__MSK                  0x3F
#define BMA2X2_THETA_BLOCK__REG                  BMA2X2_THETA_BLOCK_REG

#define BMA2X2_THETA_FLAT__POS                  0
#define BMA2X2_THETA_FLAT__LEN                  6
#define BMA2X2_THETA_FLAT__MSK                  0x3F
#define BMA2X2_THETA_FLAT__REG                  BMA2X2_THETA_FLAT_REG

#define BMA2X2_FLAT_HOLD_TIME__POS              4
#define BMA2X2_FLAT_HOLD_TIME__LEN              2
#define BMA2X2_FLAT_HOLD_TIME__MSK              0x30
#define BMA2X2_FLAT_HOLD_TIME__REG              BMA2X2_FLAT_HOLD_TIME_REG

#define BMA2X2_FLAT_HYS__POS                   0
#define BMA2X2_FLAT_HYS__LEN                   3
#define BMA2X2_FLAT_HYS__MSK                   0x07
#define BMA2X2_FLAT_HYS__REG                   BMA2X2_FLAT_HOLD_TIME_REG

#define BMA2X2_FIFO_WML_TRIG_RETAIN__POS                   0
#define BMA2X2_FIFO_WML_TRIG_RETAIN__LEN                   6
#define BMA2X2_FIFO_WML_TRIG_RETAIN__MSK                   0x3F
#define BMA2X2_FIFO_WML_TRIG_RETAIN__REG                   BMA2X2_FIFO_WML_TRIG

#define BMA2X2_EN_SELF_TEST__POS                0
#define BMA2X2_EN_SELF_TEST__LEN                2
#define BMA2X2_EN_SELF_TEST__MSK                0x03
#define BMA2X2_EN_SELF_TEST__REG                BMA2X2_SELF_TEST_REG

#define BMA2X2_NEG_SELF_TEST__POS               2
#define BMA2X2_NEG_SELF_TEST__LEN               1
#define BMA2X2_NEG_SELF_TEST__MSK               0x04
#define BMA2X2_NEG_SELF_TEST__REG               BMA2X2_SELF_TEST_REG

#define BMA2X2_SELF_TEST_AMP__POS               4
#define BMA2X2_SELF_TEST_AMP__LEN               1
#define BMA2X2_SELF_TEST_AMP__MSK               0x10
#define BMA2X2_SELF_TEST_AMP__REG               BMA2X2_SELF_TEST_REG


#define BMA2X2_UNLOCK_EE_PROG_MODE__POS     0
#define BMA2X2_UNLOCK_EE_PROG_MODE__LEN     1
#define BMA2X2_UNLOCK_EE_PROG_MODE__MSK     0x01
#define BMA2X2_UNLOCK_EE_PROG_MODE__REG     BMA2X2_EEPROM_CTRL_REG

#define BMA2X2_START_EE_PROG_TRIG__POS      1
#define BMA2X2_START_EE_PROG_TRIG__LEN      1
#define BMA2X2_START_EE_PROG_TRIG__MSK      0x02
#define BMA2X2_START_EE_PROG_TRIG__REG      BMA2X2_EEPROM_CTRL_REG

#define BMA2X2_EE_PROG_READY__POS          2
#define BMA2X2_EE_PROG_READY__LEN          1
#define BMA2X2_EE_PROG_READY__MSK          0x04
#define BMA2X2_EE_PROG_READY__REG          BMA2X2_EEPROM_CTRL_REG

#define BMA2X2_UPDATE_IMAGE__POS                3
#define BMA2X2_UPDATE_IMAGE__LEN                1
#define BMA2X2_UPDATE_IMAGE__MSK                0x08
#define BMA2X2_UPDATE_IMAGE__REG                BMA2X2_EEPROM_CTRL_REG

#define BMA2X2_EE_REMAIN__POS                4
#define BMA2X2_EE_REMAIN__LEN                4
#define BMA2X2_EE_REMAIN__MSK                0xF0
#define BMA2X2_EE_REMAIN__REG                BMA2X2_EEPROM_CTRL_REG

#define BMA2X2_EN_SPI_MODE_3__POS              0
#define BMA2X2_EN_SPI_MODE_3__LEN              1
#define BMA2X2_EN_SPI_MODE_3__MSK              0x01
#define BMA2X2_EN_SPI_MODE_3__REG              BMA2X2_SERIAL_CTRL_REG

#define BMA2X2_I2C_WATCHDOG_PERIOD__POS        1
#define BMA2X2_I2C_WATCHDOG_PERIOD__LEN        1
#define BMA2X2_I2C_WATCHDOG_PERIOD__MSK        0x02
#define BMA2X2_I2C_WATCHDOG_PERIOD__REG        BMA2X2_SERIAL_CTRL_REG

#define BMA2X2_EN_I2C_WATCHDOG__POS            2
#define BMA2X2_EN_I2C_WATCHDOG__LEN            1
#define BMA2X2_EN_I2C_WATCHDOG__MSK            0x04
#define BMA2X2_EN_I2C_WATCHDOG__REG            BMA2X2_SERIAL_CTRL_REG

#define BMA2X2_EXT_MODE__POS              7
#define BMA2X2_EXT_MODE__LEN              1
#define BMA2X2_EXT_MODE__MSK              0x80
#define BMA2X2_EXT_MODE__REG              BMA2X2_EXTMODE_CTRL_REG

#define BMA2X2_ALLOW_UPPER__POS        6
#define BMA2X2_ALLOW_UPPER__LEN        1
#define BMA2X2_ALLOW_UPPER__MSK        0x40
#define BMA2X2_ALLOW_UPPER__REG        BMA2X2_EXTMODE_CTRL_REG

#define BMA2X2_MAP_2_LOWER__POS            5
#define BMA2X2_MAP_2_LOWER__LEN            1
#define BMA2X2_MAP_2_LOWER__MSK            0x20
#define BMA2X2_MAP_2_LOWER__REG            BMA2X2_EXTMODE_CTRL_REG

#define BMA2X2_MAGIC_NUMBER__POS            0
#define BMA2X2_MAGIC_NUMBER__LEN            5
#define BMA2X2_MAGIC_NUMBER__MSK            0x1F
#define BMA2X2_MAGIC_NUMBER__REG            BMA2X2_EXTMODE_CTRL_REG

#define BMA2X2_UNLOCK_EE_WRITE_TRIM__POS        4
#define BMA2X2_UNLOCK_EE_WRITE_TRIM__LEN        4
#define BMA2X2_UNLOCK_EE_WRITE_TRIM__MSK        0xF0
#define BMA2X2_UNLOCK_EE_WRITE_TRIM__REG        BMA2X2_CTRL_UNLOCK_REG

#define BMA2X2_EN_SLOW_COMP_X__POS              0
#define BMA2X2_EN_SLOW_COMP_X__LEN              1
#define BMA2X2_EN_SLOW_COMP_X__MSK              0x01
#define BMA2X2_EN_SLOW_COMP_X__REG              BMA2X2_OFFSET_CTRL_REG

#define BMA2X2_EN_SLOW_COMP_Y__POS              1
#define BMA2X2_EN_SLOW_COMP_Y__LEN              1
#define BMA2X2_EN_SLOW_COMP_Y__MSK              0x02
#define BMA2X2_EN_SLOW_COMP_Y__REG              BMA2X2_OFFSET_CTRL_REG

#define BMA2X2_EN_SLOW_COMP_Z__POS              2
#define BMA2X2_EN_SLOW_COMP_Z__LEN              1
#define BMA2X2_EN_SLOW_COMP_Z__MSK              0x04
#define BMA2X2_EN_SLOW_COMP_Z__REG              BMA2X2_OFFSET_CTRL_REG

#define BMA2X2_FAST_CAL_RDY_S__POS             4
#define BMA2X2_FAST_CAL_RDY_S__LEN             1
#define BMA2X2_FAST_CAL_RDY_S__MSK             0x10
#define BMA2X2_FAST_CAL_RDY_S__REG             BMA2X2_OFFSET_CTRL_REG

#define BMA2X2_CAL_TRIGGER__POS                5
#define BMA2X2_CAL_TRIGGER__LEN                2
#define BMA2X2_CAL_TRIGGER__MSK                0x60
#define BMA2X2_CAL_TRIGGER__REG                BMA2X2_OFFSET_CTRL_REG

#define BMA2X2_RESET_OFFSET_REGS__POS           7
#define BMA2X2_RESET_OFFSET_REGS__LEN           1
#define BMA2X2_RESET_OFFSET_REGS__MSK           0x80
#define BMA2X2_RESET_OFFSET_REGS__REG           BMA2X2_OFFSET_CTRL_REG

#define BMA2X2_COMP_CUTOFF__POS                 0
#define BMA2X2_COMP_CUTOFF__LEN                 1
#define BMA2X2_COMP_CUTOFF__MSK                 0x01
#define BMA2X2_COMP_CUTOFF__REG                 BMA2X2_OFFSET_PARAMS_REG

#define BMA2X2_COMP_TARGET_OFFSET_X__POS        1
#define BMA2X2_COMP_TARGET_OFFSET_X__LEN        2
#define BMA2X2_COMP_TARGET_OFFSET_X__MSK        0x06
#define BMA2X2_COMP_TARGET_OFFSET_X__REG        BMA2X2_OFFSET_PARAMS_REG

#define BMA2X2_COMP_TARGET_OFFSET_Y__POS        3
#define BMA2X2_COMP_TARGET_OFFSET_Y__LEN        2
#define BMA2X2_COMP_TARGET_OFFSET_Y__MSK        0x18
#define BMA2X2_COMP_TARGET_OFFSET_Y__REG        BMA2X2_OFFSET_PARAMS_REG

#define BMA2X2_COMP_TARGET_OFFSET_Z__POS        5
#define BMA2X2_COMP_TARGET_OFFSET_Z__LEN        2
#define BMA2X2_COMP_TARGET_OFFSET_Z__MSK        0x60
#define BMA2X2_COMP_TARGET_OFFSET_Z__REG        BMA2X2_OFFSET_PARAMS_REG

#define BMA2X2_FIFO_DATA_SELECT__POS                 0
#define BMA2X2_FIFO_DATA_SELECT__LEN                 2
#define BMA2X2_FIFO_DATA_SELECT__MSK                 0x03
#define BMA2X2_FIFO_DATA_SELECT__REG                 BMA2X2_FIFO_MODE_REG

#define BMA2X2_FIFO_TRIGGER_SOURCE__POS                 2
#define BMA2X2_FIFO_TRIGGER_SOURCE__LEN                 2
#define BMA2X2_FIFO_TRIGGER_SOURCE__MSK                 0x0C
#define BMA2X2_FIFO_TRIGGER_SOURCE__REG                 BMA2X2_FIFO_MODE_REG

#define BMA2X2_FIFO_TRIGGER_ACTION__POS                 4
#define BMA2X2_FIFO_TRIGGER_ACTION__LEN                 2
#define BMA2X2_FIFO_TRIGGER_ACTION__MSK                 0x30
#define BMA2X2_FIFO_TRIGGER_ACTION__REG                 BMA2X2_FIFO_MODE_REG

#define BMA2X2_FIFO_MODE__POS                 6
#define BMA2X2_FIFO_MODE__LEN                 2
#define BMA2X2_FIFO_MODE__MSK                 0xC0
#define BMA2X2_FIFO_MODE__REG                 BMA2X2_FIFO_MODE_REG


#define BMA2X2_STATUS1                             0
#define BMA2X2_STATUS2                             1
#define BMA2X2_STATUS3                             2
#define BMA2X2_STATUS4                             3
#define BMA2X2_STATUS5                             4


#define BMA2X2_RANGE_2G                 3
#define BMA2X2_RANGE_4G                 5
#define BMA2X2_RANGE_8G                 8
#define BMA2X2_RANGE_16G                12


#define BMA2X2_BW_7_81HZ        0x08
#define BMA2X2_BW_15_63HZ       0x09
#define BMA2X2_BW_31_25HZ       0x0A
#define BMA2X2_BW_62_50HZ       0x0B
#define BMA2X2_BW_125HZ         0x0C
#define BMA2X2_BW_250HZ         0x0D
#define BMA2X2_BW_500HZ         0x0E
#define BMA2X2_BW_1000HZ        0x0F

#define BMA2X2_SLEEP_DUR_0_5MS        0x05
#define BMA2X2_SLEEP_DUR_1MS          0x06
#define BMA2X2_SLEEP_DUR_2MS          0x07
#define BMA2X2_SLEEP_DUR_4MS          0x08
#define BMA2X2_SLEEP_DUR_6MS          0x09
#define BMA2X2_SLEEP_DUR_10MS         0x0A
#define BMA2X2_SLEEP_DUR_25MS         0x0B
#define BMA2X2_SLEEP_DUR_50MS         0x0C
#define BMA2X2_SLEEP_DUR_100MS        0x0D
#define BMA2X2_SLEEP_DUR_500MS        0x0E
#define BMA2X2_SLEEP_DUR_1S           0x0F

#define BMA2X2_LATCH_DUR_NON_LATCH    0x00
#define BMA2X2_LATCH_DUR_250MS        0x01
#define BMA2X2_LATCH_DUR_500MS        0x02
#define BMA2X2_LATCH_DUR_1S           0x03
#define BMA2X2_LATCH_DUR_2S           0x04
#define BMA2X2_LATCH_DUR_4S           0x05
#define BMA2X2_LATCH_DUR_8S           0x06
#define BMA2X2_LATCH_DUR_LATCH        0x07
#define BMA2X2_LATCH_DUR_NON_LATCH1   0x08
#define BMA2X2_LATCH_DUR_250US        0x09
#define BMA2X2_LATCH_DUR_500US        0x0A
#define BMA2X2_LATCH_DUR_1MS          0x0B
#define BMA2X2_LATCH_DUR_12_5MS       0x0C
#define BMA2X2_LATCH_DUR_25MS         0x0D
#define BMA2X2_LATCH_DUR_50MS         0x0E
#define BMA2X2_LATCH_DUR_LATCH1       0x0F

#define BMA2X2_MODE_NORMAL             0
#define BMA2X2_MODE_LOWPOWER1          1
#define BMA2X2_MODE_SUSPEND            2
#define BMA2X2_MODE_DEEP_SUSPEND       3
#define BMA2X2_MODE_LOWPOWER2          4
#define BMA2X2_MODE_STANDBY            5

#define BMA2X2_X_AXIS           0
#define BMA2X2_Y_AXIS           1
#define BMA2X2_Z_AXIS           2

#define BMA2X2_Low_G_Interrupt       0
#define BMA2X2_High_G_X_Interrupt    1
#define BMA2X2_High_G_Y_Interrupt    2
#define BMA2X2_High_G_Z_Interrupt    3
#define BMA2X2_DATA_EN               4
#define BMA2X2_Slope_X_Interrupt     5
#define BMA2X2_Slope_Y_Interrupt     6
#define BMA2X2_Slope_Z_Interrupt     7
#define BMA2X2_Single_Tap_Interrupt  8
#define BMA2X2_Double_Tap_Interrupt  9
#define BMA2X2_Orient_Interrupt      10
#define BMA2X2_Flat_Interrupt        11
#define BMA2X2_FFULL_INTERRUPT       12
#define BMA2X2_FWM_INTERRUPT         13

#define BMA2X2_INT1_LOWG         0
#define BMA2X2_INT2_LOWG         1
#define BMA2X2_INT1_HIGHG        0
#define BMA2X2_INT2_HIGHG        1
#define BMA2X2_INT1_SLOPE        0
#define BMA2X2_INT2_SLOPE        1
#define BMA2X2_INT1_SLO_NO_MOT   0
#define BMA2X2_INT2_SLO_NO_MOT   1
#define BMA2X2_INT1_DTAP         0
#define BMA2X2_INT2_DTAP         1
#define BMA2X2_INT1_STAP         0
#define BMA2X2_INT2_STAP         1
#define BMA2X2_INT1_ORIENT       0
#define BMA2X2_INT2_ORIENT       1
#define BMA2X2_INT1_FLAT         0
#define BMA2X2_INT2_FLAT         1
#define BMA2X2_INT1_NDATA        0
#define BMA2X2_INT2_NDATA        1
#define BMA2X2_INT1_FWM          0
#define BMA2X2_INT2_FWM          1
#define BMA2X2_INT1_FFULL        0
#define BMA2X2_INT2_FFULL        1

#define BMA2X2_SRC_LOWG         0
#define BMA2X2_SRC_HIGHG        1
#define BMA2X2_SRC_SLOPE        2
#define BMA2X2_SRC_SLO_NO_MOT   3
#define BMA2X2_SRC_TAP          4
#define BMA2X2_SRC_DATA         5

#define BMA2X2_INT1_OUTPUT      0
#define BMA2X2_INT2_OUTPUT      1
#define BMA2X2_INT1_LEVEL       0
#define BMA2X2_INT2_LEVEL       1

#define BMA2X2_LOW_DURATION            0
#define BMA2X2_HIGH_DURATION           1
#define BMA2X2_SLOPE_DURATION          2
#define BMA2X2_SLO_NO_MOT_DURATION     3

#define BMA2X2_LOW_THRESHOLD            0
#define BMA2X2_HIGH_THRESHOLD           1
#define BMA2X2_SLOPE_THRESHOLD          2
#define BMA2X2_SLO_NO_MOT_THRESHOLD     3


#define BMA2X2_LOWG_HYST                0
#define BMA2X2_HIGHG_HYST               1

#define BMA2X2_ORIENT_THETA             0
#define BMA2X2_FLAT_THETA               1

#define BMA2X2_I2C_SELECT               0
#define BMA2X2_I2C_EN                   1

#define BMA2X2_SLOW_COMP_X              0
#define BMA2X2_SLOW_COMP_Y              1
#define BMA2X2_SLOW_COMP_Z              2

#define BMA2X2_CUT_OFF                  0
#define BMA2X2_OFFSET_TRIGGER_X         1
#define BMA2X2_OFFSET_TRIGGER_Y         2
#define BMA2X2_OFFSET_TRIGGER_Z         3

#define BMA2X2_GP0                      0
#define BMA2X2_GP1                      1

#define BMA2X2_SLO_NO_MOT_EN_X          0
#define BMA2X2_SLO_NO_MOT_EN_Y          1
#define BMA2X2_SLO_NO_MOT_EN_Z          2
#define BMA2X2_SLO_NO_MOT_EN_SEL        3

#define BMA2X2_WAKE_UP_DUR_20MS         0
#define BMA2X2_WAKE_UP_DUR_80MS         1
#define BMA2X2_WAKE_UP_DUR_320MS                2
#define BMA2X2_WAKE_UP_DUR_2560MS               3

#define BMA2X2_SELF_TEST0_ON            1
#define BMA2X2_SELF_TEST1_ON            2

#define BMA2X2_EE_W_OFF                 0
#define BMA2X2_EE_W_ON                  1

#define BMA2X2_LOW_TH_IN_G(gthres, range)           ((256 * gthres) / range)


#define BMA2X2_HIGH_TH_IN_G(gthres, range)          ((256 * gthres) / range)


#define BMA2X2_LOW_HY_IN_G(ghyst, range)            ((32 * ghyst) / range)


#define BMA2X2_HIGH_HY_IN_G(ghyst, range)           ((32 * ghyst) / range)


#define BMA2X2_SLOPE_TH_IN_G(gthres, range)    ((128 * gthres) / range)


#define BMA2X2_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)


#define BMA2X2_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))


#define BMA255_CHIP_ID 0XFA
#define BMA250E_CHIP_ID 0XF9
#define BMA222E_CHIP_ID 0XF8
#define BMA280_CHIP_ID 0XFB

#define BMA255_TYPE 0
#define BMA250E_TYPE 1
#define BMA222E_TYPE 2
#define BMA280_TYPE 3

#define MAX_FIFO_F_LEVEL 32
#define MAX_FIFO_F_BYTES 6
#define BMA_MAX_RETRY_I2C_XFER (100)

#define CALIBRATION_FILE_PATH	"/efs/calibration_data"

unsigned char *sensor_name[] = { "BMA255", "BMA250E", "BMA222E", "BMA280" };

struct bma2x2acc {
	s16	x,
		y,
		z;
} ;

struct bma2x2_data {
	struct i2c_client *bma2x2_client;
	atomic_t delay;
	atomic_t enable;
	atomic_t selftest_result;
	unsigned int chip_id;
	unsigned char mode;
	signed char sensor_type;
	struct input_dev *input;
	struct bma2x2acc value;
	struct mutex value_mutex;
	struct mutex enable_mutex;
	struct mutex mode_mutex;
	struct delayed_work work;
	struct work_struct irq_work;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
	struct mutex data_mutex;
	struct work_struct alert_work;
	struct wake_lock reactive_wake_lock;
	atomic_t reactive_state;
	atomic_t reactive_enable;
	atomic_t factory_test;
	atomic_t set_mode;
	bool factory_mode;
#endif
	int IRQ;

#ifdef CONFIG_BMA_USE_PLATFORM_DATA
	struct bosch_sensor_specific *bst_pd;
#endif
};

static struct device *bma2x2_device=NULL;

/*static struct bma2x2_data *bma2x2_data;
static struct bma2x2_data *bma2x2_acc_get_data(void)
{
	return bma2x2_data;
}*/

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bma2x2_early_suspend(struct early_suspend *h);
static void bma2x2_late_resume(struct early_suspend *h);
#endif
static int bma2x2_open_cal(struct i2c_client *client);

static void bma2x2_remap_sensor_data(struct bma2x2acc *val,
		struct bma2x2_data *client_data)
{
#ifdef CONFIG_BMA_USE_PLATFORM_DATA
	struct bosch_sensor_data bsd;

	if (NULL == client_data->bst_pd)
		return;

	bsd.x = val->x;
	bsd.y = val->y;
	bsd.z = val->z;

	bst_remap_sensor_data_dft_tab(&bsd,
			client_data->bst_pd->place);

	val->x = bsd.x;
	val->y = bsd.y;
	val->z = bsd.z;
#else
	(void)val;
	(void)client_data;
#endif
}


static int bma2x2_smbus_read_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	s32 dummy;
	dummy = i2c_smbus_read_byte_data(client, reg_addr);
	if (dummy < 0)
		return -1;
	*data = dummy & 0x000000ff;

	return 0;
}

static int bma2x2_smbus_write_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	s32 dummy;

	dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
	if (dummy < 0)
		return -1;
	return 0;
}

static int bma2x2_smbus_read_byte_block(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	s32 dummy;
	dummy = i2c_smbus_read_i2c_block_data(client, reg_addr, len, data);
	if (dummy < 0)
		return -1;
	return 0;
}

static int bma_i2c_burst_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u16 len)
{
	int retry;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg_addr,
		},

		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = data,
		 },
	};

	for (retry = 0; retry < BMA_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			mdelay(1);
	}

	if (BMA_MAX_RETRY_I2C_XFER <= retry) {
		printk(KERN_INFO "I2C xfer error");
		return -EIO;
	}

	return 0;
}


#ifdef BMA2X2_ENABLE_INT1
static int bma2x2_set_int1_pad_sel(struct i2c_client *client, unsigned char
		int1sel)
{
	int comres = 0;
	unsigned char data;
	unsigned char state;
	state = 0x01;


	switch (int1sel) {
	case 0:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_LOWG__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_LOWG,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_LOWG__REG, &data);
		break;
	case 1:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_HIGHG__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_HIGHG,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_HIGHG__REG, &data);
		break;
	case 2:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_SLOPE__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_SLOPE,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_SLOPE__REG, &data);
		break;
	case 3:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_DB_TAP__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_DB_TAP,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_DB_TAP__REG, &data);
		break;
	case 4:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_SNG_TAP__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_SNG_TAP,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_SNG_TAP__REG, &data);
		break;
	case 5:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_ORIENT__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_ORIENT,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_ORIENT__REG, &data);
		break;
	case 6:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_FLAT__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_FLAT,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_FLAT__REG, &data);
		break;
	case 7:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_SLO_NO_MOT__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_SLO_NO_MOT,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_SLO_NO_MOT__REG, &data);
		break;

	default:
		break;
	}

	return comres;
}
#endif /* BMA2X2_ENABLE_INT1 */
#ifdef BMA2X2_ENABLE_INT2
static int bma2x2_set_int2_pad_sel(struct i2c_client *client, unsigned char
		int2sel)
{
	int comres = 0;
	unsigned char data;
	unsigned char state;
	state = 0x01;


	switch (int2sel) {
	case 0:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_LOWG__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT2_PAD_LOWG,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_LOWG__REG, &data);
		break;
	case 1:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_HIGHG__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT2_PAD_HIGHG,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_HIGHG__REG, &data);
		break;
	case 2:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_SLOPE__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT2_PAD_SLOPE,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_SLOPE__REG, &data);
		break;
	case 3:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_DB_TAP__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT2_PAD_DB_TAP,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_DB_TAP__REG, &data);
		break;
	case 4:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_SNG_TAP__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT2_PAD_SNG_TAP,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_SNG_TAP__REG, &data);
		break;
	case 5:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_ORIENT__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT2_PAD_ORIENT,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_ORIENT__REG, &data);
		break;
	case 6:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_FLAT__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT2_PAD_FLAT,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_FLAT__REG, &data);
		break;
	case 7:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_SLO_NO_MOT__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT2_PAD_SLO_NO_MOT,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_SLO_NO_MOT__REG, &data);
		break;
	default:
		break;
	}

	return comres;
}
#endif /* BMA2X2_ENABLE_INT2 */

static int bma2x2_set_Int_Enable(struct i2c_client *client, unsigned char
		InterruptType , unsigned char value)
{
	int comres = 0;
	unsigned char data1, data2;

	if ((11 < InterruptType) && (InterruptType < 16)) {
		switch (InterruptType) {
		case 12:
			/* slow/no motion X Interrupt  */
			comres = bma2x2_smbus_read_byte(client,
				BMA2X2_INT_SLO_NO_MOT_EN_X_INT__REG, &data1);
			data1 = BMA2X2_SET_BITSLICE(data1,
				BMA2X2_INT_SLO_NO_MOT_EN_X_INT, value);
			comres = bma2x2_smbus_write_byte(client,
				BMA2X2_INT_SLO_NO_MOT_EN_X_INT__REG, &data1);
			break;
		case 13:
			/* slow/no motion Y Interrupt  */
			comres = bma2x2_smbus_read_byte(client,
				BMA2X2_INT_SLO_NO_MOT_EN_Y_INT__REG, &data1);
			data1 = BMA2X2_SET_BITSLICE(data1,
				BMA2X2_INT_SLO_NO_MOT_EN_Y_INT, value);
			comres = bma2x2_smbus_write_byte(client,
				BMA2X2_INT_SLO_NO_MOT_EN_Y_INT__REG, &data1);
			break;
		case 14:
			/* slow/no motion Z Interrupt  */
			comres = bma2x2_smbus_read_byte(client,
				BMA2X2_INT_SLO_NO_MOT_EN_Z_INT__REG, &data1);
			data1 = BMA2X2_SET_BITSLICE(data1,
				BMA2X2_INT_SLO_NO_MOT_EN_Z_INT, value);
			comres = bma2x2_smbus_write_byte(client,
				BMA2X2_INT_SLO_NO_MOT_EN_Z_INT__REG, &data1);
			break;
		case 15:
			/* slow / no motion Interrupt select */
			comres = bma2x2_smbus_read_byte(client,
				BMA2X2_INT_SLO_NO_MOT_EN_SEL_INT__REG, &data1);
			data1 = BMA2X2_SET_BITSLICE(data1,
				BMA2X2_INT_SLO_NO_MOT_EN_SEL_INT, value);
			comres = bma2x2_smbus_write_byte(client,
				BMA2X2_INT_SLO_NO_MOT_EN_SEL_INT__REG, &data1);
		}

	return comres;
	}


	comres = bma2x2_smbus_read_byte(client, BMA2X2_INT_ENABLE1_REG, &data1);
	comres = bma2x2_smbus_read_byte(client, BMA2X2_INT_ENABLE2_REG, &data2);

	value = value & 1;
	switch (InterruptType) {
	case 0:
		/* Low G Interrupt  */
		data2 = BMA2X2_SET_BITSLICE(data2, BMA2X2_EN_LOWG_INT, value);
		break;
	case 1:
		/* High G X Interrupt */

		data2 = BMA2X2_SET_BITSLICE(data2, BMA2X2_EN_HIGHG_X_INT,
				value);
		break;
	case 2:
		/* High G Y Interrupt */

		data2 = BMA2X2_SET_BITSLICE(data2, BMA2X2_EN_HIGHG_Y_INT,
				value);
		break;
	case 3:
		/* High G Z Interrupt */

		data2 = BMA2X2_SET_BITSLICE(data2, BMA2X2_EN_HIGHG_Z_INT,
				value);
		break;
	case 4:
		/* New Data Interrupt  */

		data2 = BMA2X2_SET_BITSLICE(data2, BMA2X2_EN_NEW_DATA_INT,
				value);
		break;
	case 5:
		/* Slope X Interrupt */

		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_SLOPE_X_INT,
				value);
		break;
	case 6:
		/* Slope Y Interrupt */

		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_SLOPE_Y_INT,
				value);
		break;
	case 7:
		/* Slope Z Interrupt */

		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_SLOPE_Z_INT,
				value);
		break;
	case 8:
		/* Single Tap Interrupt */

		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_SINGLE_TAP_INT,
				value);
		break;
	case 9:
		/* Double Tap Interrupt */

		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_DOUBLE_TAP_INT,
				value);
		break;
	case 10:
		/* Orient Interrupt  */

		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_ORIENT_INT, value);
		break;
	case 11:
		/* Flat Interrupt */

		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_FLAT_INT, value);
		break;
	default:
		break;
	}
	comres = bma2x2_smbus_write_byte(client, BMA2X2_INT_ENABLE1_REG,
			&data1);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_INT_ENABLE2_REG,
			&data2);

	return comres;
}


#if defined(BMA2X2_ENABLE_INT1) || defined(BMA2X2_ENABLE_INT2)
static int bma2x2_get_interruptstatus1(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_STATUS1_REG, &data);
	*intstatus = data;

	return comres;
}


static int bma2x2_get_HIGH_first(struct i2c_client *client, unsigned char
						param, unsigned char *intstatus)
{
	int comres = 0;
	unsigned char data;

	switch (param) {
	case 0:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_STATUS_ORIENT_HIGH_REG, &data);
		data = BMA2X2_GET_BITSLICE(data, BMA2X2_HIGHG_FIRST_X);
		*intstatus = data;
		break;
	case 1:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_STATUS_ORIENT_HIGH_REG, &data);
		data = BMA2X2_GET_BITSLICE(data, BMA2X2_HIGHG_FIRST_Y);
		*intstatus = data;
		break;
	case 2:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_STATUS_ORIENT_HIGH_REG, &data);
		data = BMA2X2_GET_BITSLICE(data, BMA2X2_HIGHG_FIRST_Z);
		*intstatus = data;
		break;
	default:
		break;
	}

	return comres;
}

static int bma2x2_get_HIGH_sign(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_STATUS_ORIENT_HIGH_REG,
			&data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_HIGHG_SIGN_S);
	*intstatus = data;

	return comres;
}


static int bma2x2_get_slope_first(struct i2c_client *client, unsigned char
	param, unsigned char *intstatus)
{
	int comres = 0;
	unsigned char data;

	switch (param) {
	case 0:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_STATUS_TAP_SLOPE_REG, &data);
		data = BMA2X2_GET_BITSLICE(data, BMA2X2_SLOPE_FIRST_X);
		*intstatus = data;
		break;
	case 1:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_STATUS_TAP_SLOPE_REG, &data);
		data = BMA2X2_GET_BITSLICE(data, BMA2X2_SLOPE_FIRST_Y);
		*intstatus = data;
		break;
	case 2:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_STATUS_TAP_SLOPE_REG, &data);
		data = BMA2X2_GET_BITSLICE(data, BMA2X2_SLOPE_FIRST_Z);
		*intstatus = data;
		break;
	default:
		break;
	}

	return comres;
}

static int bma2x2_get_slope_sign(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_STATUS_TAP_SLOPE_REG,
			&data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_SLOPE_SIGN_S);
	*intstatus = data;

	return comres;
}

static int bma2x2_get_orient_status(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_STATUS_ORIENT_HIGH_REG,
			&data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_ORIENT_S);
	*intstatus = data;

	return comres;
}

static int bma2x2_get_orient_flat_status(struct i2c_client *client, unsigned
		char *intstatus)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_STATUS_ORIENT_HIGH_REG,
			&data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_FLAT_S);
	*intstatus = data;

	return comres;
}
#endif /* defined(BMA2X2_ENABLE_INT1)||defined(BMA2X2_ENABLE_INT2) */
static int bma2x2_set_Int_Mode(struct i2c_client *client, unsigned char Mode)
{
	int comres = 0;
	unsigned char data;


	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_INT_MODE_SEL__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_INT_MODE_SEL, Mode);
	comres = bma2x2_smbus_write_byte(client,
			BMA2X2_INT_MODE_SEL__REG, &data);


	return comres;
}

static int bma2x2_get_Int_Mode(struct i2c_client *client, unsigned char *Mode)
{
	int comres = 0;
	unsigned char data;


	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_INT_MODE_SEL__REG, &data);
	data  = BMA2X2_GET_BITSLICE(data, BMA2X2_INT_MODE_SEL);
	*Mode = data;


	return comres;
}
static int bma2x2_set_slope_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres = 0;
	unsigned char data;


	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_SLOPE_DUR__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_SLOPE_DUR, duration);
	comres = bma2x2_smbus_write_byte(client,
			BMA2X2_SLOPE_DUR__REG, &data);


	return comres;
}

static int bma2x2_get_slope_duration(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data;


	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_SLOPE_DURN_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_SLOPE_DUR);
	*status = data;


	return comres;
}

static int bma2x2_set_slope_no_mot_duration(struct i2c_client *client,
			unsigned char duration)
{
	int comres = 0;
	unsigned char data;


	comres = bma2x2_smbus_read_byte(client,
			BMA2x2_SLO_NO_MOT_DUR__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2x2_SLO_NO_MOT_DUR, duration);
	comres = bma2x2_smbus_write_byte(client,
			BMA2x2_SLO_NO_MOT_DUR__REG, &data);


	return comres;
}

static int bma2x2_get_slope_no_mot_duration(struct i2c_client *client,
			unsigned char *status)
{
	int comres = 0;
	unsigned char data;


	comres = bma2x2_smbus_read_byte(client,
			BMA2x2_SLO_NO_MOT_DUR__REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2x2_SLO_NO_MOT_DUR);
	*status = data;


	return comres;
}

static int bma2x2_set_slope_threshold(struct i2c_client *client,
		unsigned char threshold)
{
	int comres = 0;
	unsigned char data;

	data = threshold;
	comres = bma2x2_smbus_write_byte(client,
			BMA2X2_SLOPE_THRES__REG, &data);

	return comres;
}

static int bma2x2_get_slope_threshold(struct i2c_client *client,
		unsigned char *status)
{
	int comres = 0;
	unsigned char data;


	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_SLOPE_THRES_REG, &data);
	*status = data;

	return comres;
}

static int bma2x2_set_slope_no_mot_threshold(struct i2c_client *client,
		unsigned char threshold)
{
	int comres = 0;
	unsigned char data;

	data = threshold;
	comres = bma2x2_smbus_write_byte(client,
			BMA2X2_SLO_NO_MOT_THRES_REG, &data);

	return comres;
}

static int bma2x2_get_slope_no_mot_threshold(struct i2c_client *client,
		unsigned char *status)
{
	int comres = 0;
	unsigned char data;


	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_SLO_NO_MOT_THRES_REG, &data);
	*status = data;

	return comres;
}


static int bma2x2_set_low_g_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres = 0;
	unsigned char data;

	data = duration;
	comres = bma2x2_smbus_write_byte(client, BMA2X2_LOWG_DUR__REG, &data);

	return comres;
}

static int bma2x2_get_low_g_duration(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_LOW_DURN_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_LOWG_DUR);
	*status = data;

	return comres;
}

static int bma2x2_set_low_g_threshold(struct i2c_client *client, unsigned char
		threshold)
{
	int comres = 0;
	unsigned char data;

	data = threshold;
	comres = bma2x2_smbus_write_byte(client, BMA2X2_LOWG_THRES__REG, &data);

	return comres;
}

static int bma2x2_get_low_g_threshold(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_LOW_THRES_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_LOWG_THRES);
	*status = data;

	return comres;
}

static int bma2x2_set_high_g_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres = 0;
	unsigned char data;

	data = duration;
	comres = bma2x2_smbus_write_byte(client, BMA2X2_HIGHG_DUR__REG, &data);

	return comres;
}

static int bma2x2_get_high_g_duration(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_HIGH_DURN_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_HIGHG_DUR);
	*status = data;

	return comres;
}

static int bma2x2_set_high_g_threshold(struct i2c_client *client, unsigned char
		threshold)
{
	int comres = 0;
	unsigned char data;

	data = threshold;
	comres = bma2x2_smbus_write_byte(client, BMA2X2_HIGHG_THRES__REG,
			&data);

	return comres;
}

static int bma2x2_get_high_g_threshold(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_HIGH_THRES_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_HIGHG_THRES);
	*status = data;

	return comres;
}


static int bma2x2_set_tap_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_DUR__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_TAP_DUR, duration);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_TAP_DUR__REG, &data);

	return comres;
}

static int bma2x2_get_tap_duration(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_PARAM_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_TAP_DUR);
	*status = data;

	return comres;
}

static int bma2x2_set_tap_shock(struct i2c_client *client, unsigned char setval)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_SHOCK_DURN__REG,
			&data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_TAP_SHOCK_DURN, setval);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_TAP_SHOCK_DURN__REG,
			&data);

	return comres;
}

static int bma2x2_get_tap_shock(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_PARAM_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_TAP_SHOCK_DURN);
	*status = data;

	return comres;
}

static int bma2x2_set_tap_quiet(struct i2c_client *client, unsigned char
		duration)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_QUIET_DURN__REG,
			&data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_TAP_QUIET_DURN, duration);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_TAP_QUIET_DURN__REG,
			&data);

	return comres;
}

static int bma2x2_get_tap_quiet(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_PARAM_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_TAP_QUIET_DURN);
	*status = data;

	return comres;
}

static int bma2x2_set_tap_threshold(struct i2c_client *client, unsigned char
		threshold)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_THRES__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_TAP_THRES, threshold);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_TAP_THRES__REG, &data);

	return comres;
}

static int bma2x2_get_tap_threshold(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_THRES_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_TAP_THRES);
	*status = data;

	return comres;
}

static int bma2x2_set_tap_samp(struct i2c_client *client, unsigned char samp)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_SAMPLES__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_TAP_SAMPLES, samp);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_TAP_SAMPLES__REG,
			&data);

	return comres;
}

static int bma2x2_get_tap_samp(struct i2c_client *client, unsigned char *status)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_THRES_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_TAP_SAMPLES);
	*status = data;

	return comres;
}

static int bma2x2_set_orient_mode(struct i2c_client *client, unsigned char mode)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_ORIENT_MODE__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_ORIENT_MODE, mode);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_ORIENT_MODE__REG,
			&data);

	return comres;
}

static int bma2x2_get_orient_mode(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_ORIENT_PARAM_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_ORIENT_MODE);
	*status = data;

	return comres;
}

static int bma2x2_set_orient_blocking(struct i2c_client *client, unsigned char
		samp)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_ORIENT_BLOCK__REG,
			&data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_ORIENT_BLOCK, samp);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_ORIENT_BLOCK__REG,
			&data);

	return comres;
}

static int bma2x2_get_orient_blocking(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_ORIENT_PARAM_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_ORIENT_BLOCK);
	*status = data;

	return comres;
}

static int bma2x2_set_orient_hyst(struct i2c_client *client, unsigned char
		orienthyst)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_ORIENT_HYST__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_ORIENT_HYST, orienthyst);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_ORIENT_HYST__REG,
			&data);

	return comres;
}

static int bma2x2_get_orient_hyst(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_ORIENT_PARAM_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_ORIENT_HYST);
	*status = data;

	return comres;
}
static int bma2x2_set_theta_blocking(struct i2c_client *client, unsigned char
		thetablk)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_THETA_BLOCK__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_THETA_BLOCK, thetablk);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_THETA_BLOCK__REG,
			&data);

	return comres;
}

static int bma2x2_get_theta_blocking(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_THETA_BLOCK_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_THETA_BLOCK);
	*status = data;

	return comres;
}

static int bma2x2_set_theta_flat(struct i2c_client *client, unsigned char
		thetaflat)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_THETA_FLAT__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_THETA_FLAT, thetaflat);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_THETA_FLAT__REG, &data);

	return comres;
}

static int bma2x2_get_theta_flat(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0 ;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_THETA_FLAT_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_THETA_FLAT);
	*status = data;

	return comres;
}

static int bma2x2_set_flat_hold_time(struct i2c_client *client, unsigned char
		holdtime)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_FLAT_HOLD_TIME__REG,
			&data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_FLAT_HOLD_TIME, holdtime);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_FLAT_HOLD_TIME__REG,
			&data);

	return comres;
}

static int bma2x2_get_flat_hold_time(struct i2c_client *client, unsigned char
		*holdtime)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_FLAT_HOLD_TIME_REG,
			&data);
	data  = BMA2X2_GET_BITSLICE(data, BMA2X2_FLAT_HOLD_TIME);
	*holdtime = data ;

	return comres;
}

static int bma2x2_set_mode(struct i2c_client *client, unsigned char Mode)
{
	int comres = 0;
	unsigned char data1, data2;
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
#endif

        printk(KERN_INFO "[BMC150] %s: %d\n", __func__, (unsigned char) Mode);  

	if (Mode < 6) {
		comres = bma2x2_smbus_read_byte(client, BMA2X2_MODE_CTRL_REG,
				&data1);
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_LOW_NOISE_CTRL_REG,
				&data2);
		switch (Mode) {
		case BMA2X2_MODE_NORMAL:
				#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
				if(atomic_read(&bma2x2->reactive_enable)&&!atomic_read(&bma2x2->reactive_state)){
					break;
				}
				#endif
				data1  = BMA2X2_SET_BITSLICE(data1,
						BMA2X2_MODE_CTRL, 0);
				data2  = BMA2X2_SET_BITSLICE(data2,
						BMA2X2_LOW_POWER_MODE, 0);
				bma2x2_smbus_write_byte(client,
						BMA2X2_MODE_CTRL_REG, &data1);
				mdelay(1);
				bma2x2_smbus_write_byte(client,
					BMA2X2_LOW_NOISE_CTRL_REG, &data2);
				break;
		case BMA2X2_MODE_LOWPOWER1:
				data1  = BMA2X2_SET_BITSLICE(data1,
						BMA2X2_MODE_CTRL, 2);
				data2  = BMA2X2_SET_BITSLICE(data2,
						BMA2X2_LOW_POWER_MODE, 0);
				bma2x2_smbus_write_byte(client,
						BMA2X2_MODE_CTRL_REG, &data1);
				mdelay(1);
				bma2x2_smbus_write_byte(client,
					BMA2X2_LOW_NOISE_CTRL_REG, &data2);
				break;
		case BMA2X2_MODE_SUSPEND:
				#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
				if(atomic_read(&bma2x2->reactive_enable)){
					break;
				}
				#endif
				data1  = BMA2X2_SET_BITSLICE(data1,
						BMA2X2_MODE_CTRL, 4);
				data2  = BMA2X2_SET_BITSLICE(data2,
						BMA2X2_LOW_POWER_MODE, 0);
				bma2x2_smbus_write_byte(client,
					BMA2X2_LOW_NOISE_CTRL_REG, &data2);
				mdelay(1);
				bma2x2_smbus_write_byte(client,
					BMA2X2_MODE_CTRL_REG, &data1);
				break;
		case BMA2X2_MODE_DEEP_SUSPEND:
				#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
				if(atomic_read(&bma2x2->reactive_enable)){
					break;
				}
				#endif			
				data1  = BMA2X2_SET_BITSLICE(data1,
							BMA2X2_MODE_CTRL, 1);
				data2  = BMA2X2_SET_BITSLICE(data2,
						BMA2X2_LOW_POWER_MODE, 1);
				bma2x2_smbus_write_byte(client,
						BMA2X2_MODE_CTRL_REG, &data1);
				mdelay(1);
				bma2x2_smbus_write_byte(client,
					BMA2X2_LOW_NOISE_CTRL_REG, &data2);
				break;
		case BMA2X2_MODE_LOWPOWER2:
				data1  = BMA2X2_SET_BITSLICE(data1,
						BMA2X2_MODE_CTRL, 2);
				data2  = BMA2X2_SET_BITSLICE(data2,
						BMA2X2_LOW_POWER_MODE, 1);
				bma2x2_smbus_write_byte(client,
						BMA2X2_MODE_CTRL_REG, &data1);
				mdelay(1);
				bma2x2_smbus_write_byte(client,
					BMA2X2_LOW_NOISE_CTRL_REG, &data2);
				break;
		case BMA2X2_MODE_STANDBY:
				data1  = BMA2X2_SET_BITSLICE(data1,
						BMA2X2_MODE_CTRL, 4);
				data2  = BMA2X2_SET_BITSLICE(data2,
						BMA2X2_LOW_POWER_MODE, 1);
				bma2x2_smbus_write_byte(client,
					BMA2X2_LOW_NOISE_CTRL_REG, &data2);
				mdelay(1);
				bma2x2_smbus_write_byte(client,
						BMA2X2_MODE_CTRL_REG, &data1);
		break;
		}
	} else {
		comres = -1 ;
	}


	return comres;
}


static int bma2x2_get_mode(struct i2c_client *client, unsigned char *Mode)
{
	int comres = 0;
	unsigned char data1, data2;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_MODE_CTRL_REG, &data1);
	comres = bma2x2_smbus_read_byte(client, BMA2X2_LOW_NOISE_CTRL_REG,
			&data2);

	data1  = (data1 & 0xE0) >> 5;
	data2  = (data2 & 0x40) >> 6;


	if ((data1 == 0x00) && (data2 == 0x00)) {
		*Mode  = BMA2X2_MODE_NORMAL;
	} else {
		if ((data1 == 0x02) && (data2 == 0x00)) {
			*Mode  = BMA2X2_MODE_LOWPOWER1;
		} else {
			if ((data1 == 0x04 || data1 == 0x06) &&
						(data2 == 0x00)) {
				*Mode  = BMA2X2_MODE_SUSPEND;
			} else {
				if (((data1 & 0x01) == 0x01)) {
					*Mode  = BMA2X2_MODE_DEEP_SUSPEND;
				} else {
					if ((data1 == 0x02) &&
							(data2 == 0x01)) {
						*Mode  = BMA2X2_MODE_LOWPOWER2;
					} else {
						if ((data1 == 0x04) && (data2 ==
									0x01)) {
							*Mode  =
							BMA2X2_MODE_STANDBY;
						} else {
							*Mode =
						BMA2X2_MODE_DEEP_SUSPEND;
						}
					}
				}
			}
		}
	}

	return comres;
}

static int bma2x2_set_range(struct i2c_client *client, unsigned char Range)
{
	int comres = 0 ;
	unsigned char data1;

	if ((Range == 3) || (Range == 5) || (Range == 8) || (Range == 12)) {
		comres = bma2x2_smbus_read_byte(client, BMA2X2_RANGE_SEL_REG,
				&data1);
		switch (Range) {
		case BMA2X2_RANGE_2G:
			data1  = BMA2X2_SET_BITSLICE(data1,
					BMA2X2_RANGE_SEL, 3);
			break;
		case BMA2X2_RANGE_4G:
			data1  = BMA2X2_SET_BITSLICE(data1,
					BMA2X2_RANGE_SEL, 5);
			break;
		case BMA2X2_RANGE_8G:
			data1  = BMA2X2_SET_BITSLICE(data1,
					BMA2X2_RANGE_SEL, 8);
			break;
		case BMA2X2_RANGE_16G:
			data1  = BMA2X2_SET_BITSLICE(data1,
					BMA2X2_RANGE_SEL, 12);
			break;
		}
		comres += bma2x2_smbus_write_byte(client, BMA2X2_RANGE_SEL_REG,
				&data1);
	} else {
		comres = -1 ;
	}

	return comres;
}

static int bma2x2_get_range(struct i2c_client *client, unsigned char *Range)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_RANGE_SEL__REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_RANGE_SEL);
	*Range = data;

	return comres;
}


static int bma2x2_set_bandwidth(struct i2c_client *client, unsigned char BW)
{
	int comres = 0;
	unsigned char data;
	int Bandwidth = 0;

	if (BW > 7 && BW < 16) {
		switch (BW) {
		case BMA2X2_BW_7_81HZ:
			Bandwidth = BMA2X2_BW_7_81HZ;

			/*  7.81 Hz      64000 uS   */
			break;
		case BMA2X2_BW_15_63HZ:
			Bandwidth = BMA2X2_BW_15_63HZ;

			/*  15.63 Hz     32000 uS   */
			break;
		case BMA2X2_BW_31_25HZ:
			Bandwidth = BMA2X2_BW_31_25HZ;

			/*  31.25 Hz     16000 uS   */
			break;
		case BMA2X2_BW_62_50HZ:
			Bandwidth = BMA2X2_BW_62_50HZ;

			/*  62.50 Hz     8000 uS   */
			break;
		case BMA2X2_BW_125HZ:
			Bandwidth = BMA2X2_BW_125HZ;

			/*  125 Hz       4000 uS   */
			break;
		case BMA2X2_BW_250HZ:
			Bandwidth = BMA2X2_BW_250HZ;

			/*  250 Hz       2000 uS   */
			break;
		case BMA2X2_BW_500HZ:
			Bandwidth = BMA2X2_BW_500HZ;

			/*  500 Hz       1000 uS   */
			break;
		case BMA2X2_BW_1000HZ:
			Bandwidth = BMA2X2_BW_1000HZ;

			/*  1000 Hz      500 uS   */
			break;
		}
		comres = bma2x2_smbus_read_byte(client, BMA2X2_BANDWIDTH__REG,
				&data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_BANDWIDTH, Bandwidth);
		comres += bma2x2_smbus_write_byte(client, BMA2X2_BANDWIDTH__REG,
				&data);
	} else {
		comres = -1 ;
	}

	return comres;
}

static int bma2x2_get_bandwidth(struct i2c_client *client, unsigned char *BW)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_BANDWIDTH__REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_BANDWIDTH);
	*BW = data ;

	return comres;
}

int bma2x2_get_sleep_duration(struct i2c_client *client, unsigned char
		*sleep_dur)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_SLEEP_DUR__REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_SLEEP_DUR);
	*sleep_dur = data;

	return comres;
}

int bma2x2_set_sleep_duration(struct i2c_client *client, unsigned char
		sleep_dur)
{
	int comres = 0;
	unsigned char data;
	int sleep_duration = 0;

	if (sleep_dur > 4 && sleep_dur < 16) {
		switch (sleep_dur) {
		case BMA2X2_SLEEP_DUR_0_5MS:
			sleep_duration = BMA2X2_SLEEP_DUR_0_5MS;

			/*  0.5 MS   */
			break;
		case BMA2X2_SLEEP_DUR_1MS:
			sleep_duration = BMA2X2_SLEEP_DUR_1MS;

			/*  1 MS  */
			break;
		case BMA2X2_SLEEP_DUR_2MS:
			sleep_duration = BMA2X2_SLEEP_DUR_2MS;

			/*  2 MS  */
			break;
		case BMA2X2_SLEEP_DUR_4MS:
			sleep_duration = BMA2X2_SLEEP_DUR_4MS;

			/*  4 MS   */
			break;
		case BMA2X2_SLEEP_DUR_6MS:
			sleep_duration = BMA2X2_SLEEP_DUR_6MS;

			/*  6 MS  */
			break;
		case BMA2X2_SLEEP_DUR_10MS:
			sleep_duration = BMA2X2_SLEEP_DUR_10MS;

			/*  10 MS  */
			break;
		case BMA2X2_SLEEP_DUR_25MS:
			sleep_duration = BMA2X2_SLEEP_DUR_25MS;

			/*  25 MS  */
			break;
		case BMA2X2_SLEEP_DUR_50MS:
			sleep_duration = BMA2X2_SLEEP_DUR_50MS;

			/*  50 MS   */
			break;
		case BMA2X2_SLEEP_DUR_100MS:
			sleep_duration = BMA2X2_SLEEP_DUR_100MS;

			/*  100 MS  */
			break;
		case BMA2X2_SLEEP_DUR_500MS:
			sleep_duration = BMA2X2_SLEEP_DUR_500MS;

			/*  500 MS   */
			break;
		case BMA2X2_SLEEP_DUR_1S:
			sleep_duration = BMA2X2_SLEEP_DUR_1S;

			/*  1 SECS   */
			break;
		}
		comres = bma2x2_smbus_read_byte(client, BMA2X2_SLEEP_DUR__REG,
				&data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_SLEEP_DUR,
				sleep_duration);
		comres = bma2x2_smbus_write_byte(client, BMA2X2_SLEEP_DUR__REG,
				&data);
	} else {
		comres = -1 ;
	}


	return comres;
}

static int bma2x2_get_fifo_mode(struct i2c_client *client, unsigned char
		*fifo_mode)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_FIFO_MODE__REG, &data);
	*fifo_mode = BMA2X2_GET_BITSLICE(data, BMA2X2_FIFO_MODE);

	return comres;
}

static int bma2x2_set_fifo_mode(struct i2c_client *client, unsigned char
		fifo_mode)
{
	unsigned char data;
	int comres = 0;

	if (fifo_mode < 4) {
		comres = bma2x2_smbus_read_byte(client, BMA2X2_FIFO_MODE__REG,
				&data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_FIFO_MODE, fifo_mode);
		comres = bma2x2_smbus_write_byte(client, BMA2X2_FIFO_MODE__REG,
				&data);
	} else {
		comres = -1 ;
	}

	return comres;
}


static int bma2x2_get_fifo_trig(struct i2c_client *client, unsigned char
		*fifo_trig)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_FIFO_TRIGGER_ACTION__REG, &data);
	*fifo_trig = BMA2X2_GET_BITSLICE(data, BMA2X2_FIFO_TRIGGER_ACTION);

	return comres;
}

static int bma2x2_set_fifo_trig(struct i2c_client *client, unsigned char
		fifo_trig)
{
	unsigned char data;
	int comres = 0;

	if (fifo_trig < 4) {
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_FIFO_TRIGGER_ACTION__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_FIFO_TRIGGER_ACTION,
				fifo_trig);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_FIFO_TRIGGER_ACTION__REG, &data);
	} else {
		comres = -1 ;
	}

	return comres;
}

static int bma2x2_get_fifo_trig_src(struct i2c_client *client, unsigned char
		*trig_src)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_FIFO_TRIGGER_SOURCE__REG, &data);
	*trig_src = BMA2X2_GET_BITSLICE(data, BMA2X2_FIFO_TRIGGER_SOURCE);

	return comres;
}


static int bma2x2_set_fifo_trig_src(struct i2c_client *client, unsigned char
		trig_src)
{
	unsigned char data;
	int comres = 0;

	if (trig_src < 4) {
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_FIFO_TRIGGER_SOURCE__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_FIFO_TRIGGER_SOURCE,
				trig_src);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_FIFO_TRIGGER_SOURCE__REG, &data);
	} else {
		comres = -1 ;
	}

	return comres;
}

static int bma2x2_get_fifo_framecount(struct i2c_client *client, unsigned char
			 *framecount)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_FIFO_FRAME_COUNTER_S__REG, &data);
	*framecount = BMA2X2_GET_BITSLICE(data, BMA2X2_FIFO_FRAME_COUNTER_S);

	return comres;
}

static int bma2x2_get_fifo_data_sel(struct i2c_client *client, unsigned char
		*data_sel)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_FIFO_DATA_SELECT__REG, &data);
	*data_sel = BMA2X2_GET_BITSLICE(data, BMA2X2_FIFO_DATA_SELECT);

	return comres;
}

static int bma2x2_set_fifo_data_sel(struct i2c_client *client, unsigned char
		data_sel)
{
	unsigned char data;
	int comres = 0;

	if (data_sel < 4) {
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_FIFO_DATA_SELECT__REG,
				&data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_FIFO_DATA_SELECT,
				data_sel);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_FIFO_DATA_SELECT__REG,
				&data);
	} else {
		comres = -1 ;
	}

	return comres;
}

static int bma2x2_get_fifo_data_out_reg(struct i2c_client *client, unsigned char
		*out_reg)
{
	unsigned char data;
	int comres = 0;

	comres = bma2x2_smbus_read_byte(client,
				BMA2X2_FIFO_DATA_OUTPUT_REG, &data);
	*out_reg = data;

	return comres;
}

static int bma2x2_get_offset_target(struct i2c_client *client, unsigned char
		channel, unsigned char *offset)
{
	unsigned char data;
	int comres = 0;

	switch (channel) {
	case BMA2X2_CUT_OFF:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_COMP_CUTOFF__REG, &data);
		*offset = BMA2X2_GET_BITSLICE(data, BMA2X2_COMP_CUTOFF);
		break;
	case BMA2X2_OFFSET_TRIGGER_X:
		comres = bma2x2_smbus_read_byte(client,
			BMA2X2_COMP_TARGET_OFFSET_X__REG, &data);
		*offset = BMA2X2_GET_BITSLICE(data,
				BMA2X2_COMP_TARGET_OFFSET_X);
		break;
	case BMA2X2_OFFSET_TRIGGER_Y:
		comres = bma2x2_smbus_read_byte(client,
			BMA2X2_COMP_TARGET_OFFSET_Y__REG, &data);
		*offset = BMA2X2_GET_BITSLICE(data,
				BMA2X2_COMP_TARGET_OFFSET_Y);
		break;
	case BMA2X2_OFFSET_TRIGGER_Z:
		comres = bma2x2_smbus_read_byte(client,
			BMA2X2_COMP_TARGET_OFFSET_Z__REG, &data);
		*offset = BMA2X2_GET_BITSLICE(data,
				BMA2X2_COMP_TARGET_OFFSET_Z);
		break;
	default:
		comres = -1;
		break;
	}

	return comres;
}

static int bma2x2_set_offset_target(struct i2c_client *client, unsigned char
		channel, unsigned char offset)
{
	unsigned char data;
	int comres = 0;

	switch (channel) {
	case BMA2X2_CUT_OFF:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_COMP_CUTOFF__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_COMP_CUTOFF,
				offset);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_COMP_CUTOFF__REG, &data);
		break;
	case BMA2X2_OFFSET_TRIGGER_X:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_COMP_TARGET_OFFSET_X__REG,
				&data);
		data = BMA2X2_SET_BITSLICE(data,
				BMA2X2_COMP_TARGET_OFFSET_X,
				offset);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_COMP_TARGET_OFFSET_X__REG,
				&data);
		break;
	case BMA2X2_OFFSET_TRIGGER_Y:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_COMP_TARGET_OFFSET_Y__REG,
				&data);
		data = BMA2X2_SET_BITSLICE(data,
				BMA2X2_COMP_TARGET_OFFSET_Y,
				offset);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_COMP_TARGET_OFFSET_Y__REG,
				&data);
		break;
	case BMA2X2_OFFSET_TRIGGER_Z:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_COMP_TARGET_OFFSET_Z__REG,
				&data);
		data = BMA2X2_SET_BITSLICE(data,
				BMA2X2_COMP_TARGET_OFFSET_Z,
				offset);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_COMP_TARGET_OFFSET_Z__REG,
				&data);
		break;
	default:
		comres = -1;
		break;
	}

	return comres;
}

static int bma2x2_get_cal_ready(struct i2c_client *client, unsigned char *calrdy
		)
{
	int comres = 0 ;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_FAST_CAL_RDY_S__REG,
			&data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_FAST_CAL_RDY_S);
	*calrdy = data;

	return comres;
}

static int bma2x2_set_cal_trigger(struct i2c_client *client, unsigned char
		caltrigger)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_CAL_TRIGGER__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_CAL_TRIGGER, caltrigger);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_CAL_TRIGGER__REG,
			&data);

	return comres;
}

static int bma2x2_write_reg(struct i2c_client *client, unsigned char addr,
		unsigned char *data)
{
	int comres = 0 ;
	comres = bma2x2_smbus_write_byte(client, addr, data);

	return comres;
}


static int bma2x2_set_offset_x(struct i2c_client *client, unsigned char
		offsetfilt)
{
	int comres = 0;
	unsigned char data;

	data =  offsetfilt;
	comres = bma2x2_smbus_write_byte(client, BMA2X2_OFFSET_X_AXIS_REG,
						&data);

	return comres;
}


static int bma2x2_get_offset_x(struct i2c_client *client, unsigned char
						*offsetfilt)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_OFFSET_X_AXIS_REG,
						&data);
	*offsetfilt = data;

	return comres;
}

static int bma2x2_set_offset_y(struct i2c_client *client, unsigned char
						offsetfilt)
{
	int comres = 0;
	unsigned char data;

	data =  offsetfilt;
	comres = bma2x2_smbus_write_byte(client, BMA2X2_OFFSET_Y_AXIS_REG,
						&data);

	return comres;
}

static int bma2x2_get_offset_y(struct i2c_client *client, unsigned char
						*offsetfilt)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_OFFSET_Y_AXIS_REG,
						&data);
	*offsetfilt = data;

	return comres;
}

static int bma2x2_set_offset_z(struct i2c_client *client, unsigned char
						offsetfilt)
{
	int comres = 0;
	unsigned char data;

	data =  offsetfilt;
	comres = bma2x2_smbus_write_byte(client, BMA2X2_OFFSET_Z_AXIS_REG,
						&data);

	return comres;
}

static int bma2x2_get_offset_z(struct i2c_client *client, unsigned char
						*offsetfilt)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_OFFSET_Z_AXIS_REG,
						&data);
	*offsetfilt = data;

	return comres;
}


static int bma2x2_set_selftest_st(struct i2c_client *client, unsigned char
		selftest)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_EN_SELF_TEST__REG,
			&data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_SELF_TEST, selftest);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_EN_SELF_TEST__REG,
			&data);

	return comres;
}

static int bma2x2_set_selftest_stn(struct i2c_client *client, unsigned char stn)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_NEG_SELF_TEST__REG,
			&data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_NEG_SELF_TEST, stn);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_NEG_SELF_TEST__REG,
			&data);

	return comres;
}

static int bma2x2_set_selftest_amp(struct i2c_client *client, unsigned char amp)
{
	int comres = 0;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_SELF_TEST_AMP__REG,
			&data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_SELF_TEST_AMP, amp);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_SELF_TEST_AMP__REG,
			&data);

	return comres;
}

static int bma2x2_read_accel_x(struct i2c_client *client,
				signed char sensor_type, short *a_x)
{
	int comres = 0;
	unsigned char data[2];

	switch (sensor_type) {
	case 0:
		comres = bma2x2_smbus_read_byte_block(client,
					BMA2X2_ACC_X12_LSB__REG, data, 2);
		*a_x = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_X12_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_X_MSB)<<(BMA2X2_ACC_X12_LSB__LEN));
		*a_x = *a_x << (sizeof(short)*8-(BMA2X2_ACC_X12_LSB__LEN
					+ BMA2X2_ACC_X_MSB__LEN));
		*a_x = *a_x >> (sizeof(short)*8-(BMA2X2_ACC_X12_LSB__LEN
					+ BMA2X2_ACC_X_MSB__LEN));
		break;
	case 1:
		comres = bma2x2_smbus_read_byte_block(client,
					BMA2X2_ACC_X10_LSB__REG, data, 2);
		*a_x = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_X10_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_X_MSB)<<(BMA2X2_ACC_X10_LSB__LEN));
		*a_x = *a_x << (sizeof(short)*8-(BMA2X2_ACC_X10_LSB__LEN
					+ BMA2X2_ACC_X_MSB__LEN));
		*a_x = *a_x >> (sizeof(short)*8-(BMA2X2_ACC_X10_LSB__LEN
					+ BMA2X2_ACC_X_MSB__LEN));
		break;
	case 2:
		comres = bma2x2_smbus_read_byte_block(client,
					BMA2X2_ACC_X8_LSB__REG, data, 2);
		*a_x = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_X8_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_X_MSB)<<(BMA2X2_ACC_X8_LSB__LEN));
		*a_x = *a_x << (sizeof(short)*8-(BMA2X2_ACC_X8_LSB__LEN
					+ BMA2X2_ACC_X_MSB__LEN));
		*a_x = *a_x >> (sizeof(short)*8-(BMA2X2_ACC_X8_LSB__LEN
					+ BMA2X2_ACC_X_MSB__LEN));
		break;
	case 3:
		comres = bma2x2_smbus_read_byte_block(client,
					BMA2X2_ACC_X14_LSB__REG, data, 2);
		*a_x = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_X14_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_X_MSB)<<(BMA2X2_ACC_X14_LSB__LEN));
		*a_x = *a_x << (sizeof(short)*8-(BMA2X2_ACC_X14_LSB__LEN
					+ BMA2X2_ACC_X_MSB__LEN));
		*a_x = *a_x >> (sizeof(short)*8-(BMA2X2_ACC_X14_LSB__LEN
					+ BMA2X2_ACC_X_MSB__LEN));
		break;
	default:
		break;
	}

	return comres;
}

static int bma2x2_soft_reset(struct i2c_client *client)
{
	int comres = 0;
	unsigned char data = BMA2X2_EN_SOFT_RESET_VALUE ;

	comres = bma2x2_smbus_write_byte(client, BMA2X2_EN_SOFT_RESET__REG,
					&data);

	return comres;
}

static int bma2x2_read_accel_y(struct i2c_client *client,
				signed char sensor_type, short *a_y)
{
	int comres = 0;
	unsigned char data[2];

	switch (sensor_type) {
	case 0:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_Y12_LSB__REG, data, 2);
		*a_y = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_Y12_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_Y_MSB)<<(BMA2X2_ACC_Y12_LSB__LEN));
		*a_y = *a_y << (sizeof(short)*8-(BMA2X2_ACC_Y12_LSB__LEN
						+ BMA2X2_ACC_Y_MSB__LEN));
		*a_y = *a_y >> (sizeof(short)*8-(BMA2X2_ACC_Y12_LSB__LEN
						+ BMA2X2_ACC_Y_MSB__LEN));
		break;
	case 1:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_Y10_LSB__REG, data, 2);
		*a_y = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_Y10_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_Y_MSB)<<(BMA2X2_ACC_Y10_LSB__LEN));
		*a_y = *a_y << (sizeof(short)*8-(BMA2X2_ACC_Y10_LSB__LEN
						+ BMA2X2_ACC_Y_MSB__LEN));
		*a_y = *a_y >> (sizeof(short)*8-(BMA2X2_ACC_Y10_LSB__LEN
						+ BMA2X2_ACC_Y_MSB__LEN));
		break;
	case 2:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_Y8_LSB__REG, data, 2);
		*a_y = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_Y8_LSB)|
				(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_Y_MSB)<<(BMA2X2_ACC_Y8_LSB__LEN));
		*a_y = *a_y << (sizeof(short)*8-(BMA2X2_ACC_Y8_LSB__LEN
						+ BMA2X2_ACC_Y_MSB__LEN));
		*a_y = *a_y >> (sizeof(short)*8-(BMA2X2_ACC_Y8_LSB__LEN
						+ BMA2X2_ACC_Y_MSB__LEN));
		break;
	case 3:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_Y14_LSB__REG, data, 2);
		*a_y = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_Y14_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_Y_MSB)<<(BMA2X2_ACC_Y14_LSB__LEN));
		*a_y = *a_y << (sizeof(short)*8-(BMA2X2_ACC_Y14_LSB__LEN
						+ BMA2X2_ACC_Y_MSB__LEN));
		*a_y = *a_y >> (sizeof(short)*8-(BMA2X2_ACC_Y14_LSB__LEN
						+ BMA2X2_ACC_Y_MSB__LEN));
		break;
	default:
		break;
	}

	return comres;
}

static int bma2x2_read_accel_z(struct i2c_client *client,
				signed char sensor_type, short *a_z)
{
	int comres = 0;
	unsigned char data[2];

	switch (sensor_type) {
	case 0:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_Z12_LSB__REG, data, 2);
		*a_z = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_Z12_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_Z_MSB)<<(BMA2X2_ACC_Z12_LSB__LEN));
		*a_z = *a_z << (sizeof(short)*8-(BMA2X2_ACC_Z12_LSB__LEN
						+ BMA2X2_ACC_Z_MSB__LEN));
		*a_z = *a_z >> (sizeof(short)*8-(BMA2X2_ACC_Z12_LSB__LEN
						+ BMA2X2_ACC_Z_MSB__LEN));
		break;
	case 1:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_Z10_LSB__REG, data, 2);
		*a_z = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_Z10_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_Z_MSB)<<(BMA2X2_ACC_Z10_LSB__LEN));
		*a_z = *a_z << (sizeof(short)*8-(BMA2X2_ACC_Z10_LSB__LEN
						+ BMA2X2_ACC_Z_MSB__LEN));
		*a_z = *a_z >> (sizeof(short)*8-(BMA2X2_ACC_Z10_LSB__LEN
						+ BMA2X2_ACC_Z_MSB__LEN));
		break;
	case 2:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_Z8_LSB__REG, data, 2);
		*a_z = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_Z8_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_Z_MSB)<<(BMA2X2_ACC_Z8_LSB__LEN));
		*a_z = *a_z << (sizeof(short)*8-(BMA2X2_ACC_Z8_LSB__LEN
						+ BMA2X2_ACC_Z_MSB__LEN));
		*a_z = *a_z >> (sizeof(short)*8-(BMA2X2_ACC_Z8_LSB__LEN
						+ BMA2X2_ACC_Z_MSB__LEN));
		break;
	case 3:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_Z14_LSB__REG, data, 2);
		*a_z = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_Z14_LSB)|
				(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_Z_MSB)<<(BMA2X2_ACC_Z14_LSB__LEN));
		*a_z = *a_z << (sizeof(short)*8-(BMA2X2_ACC_Z14_LSB__LEN
						+ BMA2X2_ACC_Z_MSB__LEN));
		*a_z = *a_z >> (sizeof(short)*8-(BMA2X2_ACC_Z14_LSB__LEN
						+ BMA2X2_ACC_Z_MSB__LEN));
		break;
	default:
		break;
	}

	return comres;
}


static int bma2x2_read_temperature(struct i2c_client *client,
					signed char *temperature)
{
	unsigned char data;
	int comres = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TEMPERATURE_REG, &data);
	*temperature = (signed char)data;

	return comres;
}

static int bma2x2_open_cal(struct i2c_client *client)
{
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

        int cal_data[3];
        int err;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH,
		O_RDONLY, 0666);
        if (IS_ERR(cal_filp)) {
		pr_err("[BMC150] %s: Can't open calibration file\n",
			__func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		return err;
	}

	err = cal_filp->f_op->read(cal_filp,
		(char *)cal_data,
		3 * sizeof(int), &cal_filp->f_pos);
        if (err != 3 * sizeof(int)) {
		pr_err("[BMC150] %s: Can't read the cal data from file\n",
			__func__);
		filp_close(cal_filp, current->files);
		set_fs(old_fs);
	        return -EIO;
	}

	printk(KERN_INFO "[BMC150] %s (%d,%d,%d)",__func__, cal_data[0],cal_data[1],cal_data[2]);

        bma2x2_set_offset_x(bma2x2->bma2x2_client, (unsigned char)cal_data[0]);
        bma2x2_set_offset_y(bma2x2->bma2x2_client, (unsigned char)cal_data[1]);
        bma2x2_set_offset_z(bma2x2->bma2x2_client, (unsigned char)cal_data[2]);

        return 0;

}

#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
static int bma2x2_get_motion_interrupt(struct i2c_client *client)
{
	int comres = 0;
	int result = 0;
	unsigned char data;
	
	comres = bma2x2_smbus_read_byte(client, BMA2X2_SLOPE_INT_S__REG,
			&data);

	pr_info("[BMC150] %s: ACC_INT_STATUS %x\n", __func__, data);
	if (data & 0x04)
		result = 1;

	return result;
}

static void bma2x2_set_motion_interrupt(struct i2c_client *client, bool enable, bool factorytest)
{
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
	int comres = 0;
	unsigned char data;
	if (enable) {
		bma2x2_soft_reset(bma2x2->bma2x2_client);
		mdelay(20);
		bma2x2_set_mode(bma2x2->bma2x2_client, BMA2X2_MODE_LOWPOWER1);
        	mdelay(20);
        	bma2x2_set_bandwidth(client, BMA2X2_BW_SET);
		bma2x2_set_range(client, BMA2X2_RANGE_SET);
		mdelay(5);
		data = 0x04;
		comres = bma2x2_smbus_write_byte(client, BMA2X2_EN_INT1_PAD_SLOPE__REG,
							&data);
		data = 0x02;
		comres = bma2x2_smbus_write_byte(client, BMA2X2_SLOPE_DUR__REG,
							&data);
		if (factorytest){
			data = 0x00;
			comres = bma2x2_smbus_write_byte(client, BMA2X2_SLOPE_THRES__REG,
								&data);
		}
		else{
			data = 0x0A;
			comres = bma2x2_smbus_write_byte(client, BMA2X2_SLOPE_THRES__REG,
								&data);
		}
		data = 0x07;
		comres = bma2x2_smbus_write_byte(client, BMA2X2_INT_ENABLE1_REG,
							&data);
		mdelay(1);
	} 
	else {
		data = 0x00;
		comres = bma2x2_smbus_write_byte(client, BMA2X2_EN_INT1_PAD_SLOPE__REG,
							&data);
		data = 0x03;
		comres = bma2x2_smbus_write_byte(client, BMA2X2_SLOPE_DUR__REG,
							&data);
		data = 0x00;
		comres = bma2x2_smbus_write_byte(client, BMA2X2_INT_ENABLE1_REG,
							&data);
		data = 0xff;
		comres = bma2x2_smbus_write_byte(client, BMA2X2_SLOPE_THRES__REG,
							&data);
		if(atomic_read(&bma2x2->factory_test)){
			atomic_set(&bma2x2->factory_test, 0);
			bma2x2_set_mode(bma2x2->bma2x2_client, BMA2X2_MODE_NORMAL);
		}else{
			bma2x2_set_mode(bma2x2->bma2x2_client, BMA2X2_MODE_SUSPEND);
		}
		mdelay(5);
	}
}
#endif

static ssize_t bma2x2_enable_int_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int type, value;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	sscanf(buf, "%d%d", &type, &value);

	if (bma2x2_set_Int_Enable(bma2x2->bma2x2_client, type, value) < 0)
		return -EINVAL;

	return count;
}


static ssize_t bma2x2_int_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_Int_Mode(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_int_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_Int_Mode(bma2x2->bma2x2_client, (unsigned char)data) < 0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_slope_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_slope_duration(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_slope_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_slope_duration(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_slope_no_mot_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_slope_no_mot_duration(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_slope_no_mot_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_slope_no_mot_duration(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}


static ssize_t bma2x2_slope_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_slope_threshold(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_slope_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_slope_threshold(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_slope_no_mot_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_slope_no_mot_threshold(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_slope_no_mot_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_slope_no_mot_threshold(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_high_g_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_high_g_duration(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_high_g_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_high_g_duration(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_high_g_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_high_g_threshold(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_high_g_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_high_g_threshold(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_low_g_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_low_g_duration(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_low_g_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_low_g_duration(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_low_g_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_low_g_threshold(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_low_g_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_low_g_threshold(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_tap_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_tap_threshold(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_tap_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_tap_threshold(bma2x2->bma2x2_client, (unsigned char)data)
			< 0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_tap_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_tap_duration(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_tap_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_tap_duration(bma2x2->bma2x2_client, (unsigned char)data)
			< 0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_tap_quiet_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_tap_quiet(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_tap_quiet_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_tap_quiet(bma2x2->bma2x2_client, (unsigned char)data) <
			0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_tap_shock_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_tap_shock(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_tap_shock_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_tap_shock(bma2x2->bma2x2_client, (unsigned char)data) <
			0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_tap_samp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_tap_samp(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_tap_samp_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_tap_samp(bma2x2->bma2x2_client, (unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_orient_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_orient_mode(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_orient_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_orient_mode(bma2x2->bma2x2_client, (unsigned char)data) <
			0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_orient_blocking_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_orient_blocking(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_orient_blocking_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_orient_blocking(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_orient_hyst_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_orient_hyst(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_orient_hyst_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_orient_hyst(bma2x2->bma2x2_client, (unsigned char)data) <
			0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_orient_theta_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_theta_blocking(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_orient_theta_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_theta_blocking(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_flat_theta_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_theta_flat(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_flat_theta_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_theta_flat(bma2x2->bma2x2_client, (unsigned char)data) <
			0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_flat_hold_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_flat_hold_time(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}
static ssize_t bma2x2_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result;

	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	result = atomic_read(&bma2x2->selftest_result);

	if (result == 0)
		result = 1;
	else
		result = 0;

	return sprintf(buf, "%d, %d, %d, %d\n", result, 0, 0, 0);

}

static ssize_t bma2x2_softreset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_soft_reset(bma2x2->bma2x2_client) < 0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_selftest_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{

	unsigned long data;
	unsigned char clear_value = 0;
	int error;
	short value1 = 0;
	short value2 = 0;
	short diff = 0;
	unsigned long result = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	bma2x2_soft_reset(bma2x2->bma2x2_client);
	mdelay(5);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (data != 1)
		return -EINVAL;

	bma2x2_write_reg(bma2x2->bma2x2_client, 0x32, &clear_value);

	if ((bma2x2->sensor_type == BMA280_TYPE) ||
			(bma2x2->sensor_type == BMA255_TYPE)) {
		/* set to 4 G range */
		if (bma2x2_set_range(bma2x2->bma2x2_client, 5) < 0)
			return -EINVAL;
	}

	if ((bma2x2->sensor_type == BMA250E_TYPE) ||
			(bma2x2->sensor_type == BMA222E_TYPE)) {
		/* set to 8 G range */
		if (bma2x2_set_range(bma2x2->bma2x2_client, 8) < 0)
			return -EINVAL;
		if (bma2x2_set_selftest_amp(bma2x2->bma2x2_client, 1) < 0)
			return -EINVAL;
	}

	bma2x2_set_selftest_st(bma2x2->bma2x2_client, 1); /* 1 for x-axis*/
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0); /* positive
							      direction*/
	mdelay(10);
	bma2x2_read_accel_x(bma2x2->bma2x2_client,
					bma2x2->sensor_type, &value1);
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1); /* negative
							      direction*/
	mdelay(10);
	bma2x2_read_accel_x(bma2x2->bma2x2_client,
					bma2x2->sensor_type, &value2);
	diff = value1-value2;

	printk(KERN_INFO "diff x is %d,value1 is %d, value2 is %d\n", diff,
			value1, value2);

	if (bma2x2->sensor_type == BMA280_TYPE) {
		if (abs(diff) < 1638)
			result |= 1;
	}
	if (bma2x2->sensor_type == BMA255_TYPE) {
		if (abs(diff) < 409)
			result |= 1;
	}
	if (bma2x2->sensor_type == BMA250E_TYPE) {
		if (abs(diff) < 51)
			result |= 1;
	}
	if (bma2x2->sensor_type == BMA222E_TYPE) {
		if (abs(diff) < 12)
			result |= 1;
	}

	bma2x2_set_selftest_st(bma2x2->bma2x2_client, 2); /* 2 for y-axis*/
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0); /* positive
							      direction*/
	mdelay(10);
	bma2x2_read_accel_y(bma2x2->bma2x2_client,
					bma2x2->sensor_type, &value1);
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1); /* negative
							      direction*/
	mdelay(10);
	bma2x2_read_accel_y(bma2x2->bma2x2_client,
					bma2x2->sensor_type, &value2);
	diff = value1-value2;
	printk(KERN_INFO "diff y is %d,value1 is %d, value2 is %d\n", diff,
			value1, value2);

	if (bma2x2->sensor_type == BMA280_TYPE) {
		if (abs(diff) < 1638)
			result |= 2;
	}
	if (bma2x2->sensor_type == BMA255_TYPE) {
		if (abs(diff) < 409)
			result |= 2;
	}
	if (bma2x2->sensor_type == BMA250E_TYPE) {
		if (abs(diff) < 51)
			result |= 2;
	}
	if (bma2x2->sensor_type == BMA222E_TYPE) {
		if (abs(diff) < 12)
			result |= 2;
	}


	bma2x2_set_selftest_st(bma2x2->bma2x2_client, 3); /* 3 for z-axis*/
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0); /* positive
							      direction*/
	mdelay(10);
	bma2x2_read_accel_z(bma2x2->bma2x2_client,
					bma2x2->sensor_type, &value1);
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1); /* negative
							      direction*/
	mdelay(10);
	bma2x2_read_accel_z(bma2x2->bma2x2_client,
					bma2x2->sensor_type, &value2);
	diff = value1-value2;

	printk(KERN_INFO "diff z is %d,value1 is %d, value2 is %d\n", diff,
			value1, value2);

	if (bma2x2->sensor_type == BMA280_TYPE) {
		if (abs(diff) < 819)
			result |= 4;
	}
	if (bma2x2->sensor_type == BMA255_TYPE) {
		if (abs(diff) < 204)
			result |= 4;
	}
	if (bma2x2->sensor_type == BMA250E_TYPE) {
		if (abs(diff) < 25)
			result |= 4;
	}
	if (bma2x2->sensor_type == BMA222E_TYPE) {
		if (abs(diff) < 6)
			result |= 4;
	}

	/* self test for bma254 */
	if ((bma2x2->sensor_type == BMA255_TYPE) && (result > 0)) {
		result = 0;
		bma2x2_soft_reset(bma2x2->bma2x2_client);
		mdelay(5);
		bma2x2_write_reg(bma2x2->bma2x2_client, 0x32, &clear_value);
		/* set to 8 G range */
		if (bma2x2_set_range(bma2x2->bma2x2_client, 8) < 0)
			return -EINVAL;
		if (bma2x2_set_selftest_amp(bma2x2->bma2x2_client, 1) < 0)
			return -EINVAL;

		bma2x2_set_selftest_st(bma2x2->bma2x2_client, 1); /* 1
								for x-axis*/
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0); /*
							positive direction*/
		mdelay(10);
		bma2x2_read_accel_x(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value1);
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1); /*
							negative direction*/
		mdelay(10);
		bma2x2_read_accel_x(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value2);
		diff = value1-value2;

		printk(KERN_INFO "diff x is %d,value1 is %d, value2 is %d\n",
						diff, value1, value2);
		if (abs(diff) < 204)
			result |= 1;

		bma2x2_set_selftest_st(bma2x2->bma2x2_client, 2); /* 2
								for y-axis*/
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0); /*
							positive direction*/
		mdelay(10);
		bma2x2_read_accel_y(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value1);
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1); /*
							negative direction*/
		mdelay(10);
		bma2x2_read_accel_y(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value2);
		diff = value1-value2;
		printk(KERN_INFO "diff y is %d,value1 is %d, value2 is %d\n",
						diff, value1, value2);

		if (abs(diff) < 204)
			result |= 2;

		bma2x2_set_selftest_st(bma2x2->bma2x2_client, 3); /* 3
								for z-axis*/
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0); /*
							positive direction*/
		mdelay(10);
		bma2x2_read_accel_z(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value1);
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1); /*
							negative direction*/
		mdelay(10);
		bma2x2_read_accel_z(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value2);
		diff = value1-value2;

		printk(KERN_INFO "diff z is %d,value1 is %d, value2 is %d\n",
						diff, value1, value2);
		if (abs(diff) < 102)
			result |= 4;
	}

	atomic_set(&bma2x2->selftest_result, (unsigned int)result);

	bma2x2_soft_reset(bma2x2->bma2x2_client);
	mdelay(5);
	printk(KERN_INFO "self test finished\n");

	return count;
}



static ssize_t bma2x2_flat_hold_time_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_flat_hold_time(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}



static int bma2x2_read_accel_xyz(struct i2c_client *client,
		signed char sensor_type, struct bma2x2acc *acc)
{
	int comres = 0;
	unsigned char data[6];
	struct bma2x2_data *client_data = i2c_get_clientdata(client);

#ifdef BMA2X2_SENSOR_IDENTIFICATION_ENABLE
	comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_X12_LSB__REG, data, 6);
	acc->x = (data[1]<<8)|data[0];
	acc->y = (data[3]<<8)|data[2];
	acc->z = (data[5]<<8)|data[4];

#else
	switch (sensor_type) {
	case 0:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_X12_LSB__REG, data, 6);
		acc->x = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_X12_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_X_MSB)<<(BMA2X2_ACC_X12_LSB__LEN));
		acc->x = acc->x << (sizeof(short)*8-(BMA2X2_ACC_X12_LSB__LEN +
					BMA2X2_ACC_X_MSB__LEN));
		acc->x = acc->x >> (sizeof(short)*8-(BMA2X2_ACC_X12_LSB__LEN +
					BMA2X2_ACC_X_MSB__LEN));

		acc->y = BMA2X2_GET_BITSLICE(data[2], BMA2X2_ACC_Y12_LSB)|
			(BMA2X2_GET_BITSLICE(data[3],
				BMA2X2_ACC_Y_MSB)<<(BMA2X2_ACC_Y12_LSB__LEN
									));
		acc->y = acc->y << (sizeof(short)*8-(BMA2X2_ACC_Y12_LSB__LEN +
					BMA2X2_ACC_Y_MSB__LEN));
		acc->y = acc->y >> (sizeof(short)*8-(BMA2X2_ACC_Y12_LSB__LEN +
					BMA2X2_ACC_Y_MSB__LEN));

		acc->z = BMA2X2_GET_BITSLICE(data[4], BMA2X2_ACC_Z12_LSB)|
			(BMA2X2_GET_BITSLICE(data[5],
				BMA2X2_ACC_Z_MSB)<<(BMA2X2_ACC_Z12_LSB__LEN));
		acc->z = acc->z << (sizeof(short)*8-(BMA2X2_ACC_Z12_LSB__LEN +
					BMA2X2_ACC_Z_MSB__LEN));
		acc->z = acc->z >> (sizeof(short)*8-(BMA2X2_ACC_Z12_LSB__LEN +
					BMA2X2_ACC_Z_MSB__LEN));
		break;
	case 1:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_X10_LSB__REG, data, 6);
		acc->x = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_X10_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_X_MSB)<<(BMA2X2_ACC_X10_LSB__LEN));
		acc->x = acc->x << (sizeof(short)*8-(BMA2X2_ACC_X10_LSB__LEN +
					BMA2X2_ACC_X_MSB__LEN));
		acc->x = acc->x >> (sizeof(short)*8-(BMA2X2_ACC_X10_LSB__LEN +
					BMA2X2_ACC_X_MSB__LEN));

		acc->y = BMA2X2_GET_BITSLICE(data[2], BMA2X2_ACC_Y10_LSB)|
			(BMA2X2_GET_BITSLICE(data[3],
				BMA2X2_ACC_Y_MSB)<<(BMA2X2_ACC_Y10_LSB__LEN
									));
		acc->y = acc->y << (sizeof(short)*8-(BMA2X2_ACC_Y10_LSB__LEN +
					BMA2X2_ACC_Y_MSB__LEN));
		acc->y = acc->y >> (sizeof(short)*8-(BMA2X2_ACC_Y10_LSB__LEN +
					BMA2X2_ACC_Y_MSB__LEN));

		acc->z = BMA2X2_GET_BITSLICE(data[4], BMA2X2_ACC_Z10_LSB)|
			(BMA2X2_GET_BITSLICE(data[5],
				BMA2X2_ACC_Z_MSB)<<(BMA2X2_ACC_Z10_LSB__LEN));
		acc->z = acc->z << (sizeof(short)*8-(BMA2X2_ACC_Z10_LSB__LEN +
					BMA2X2_ACC_Z_MSB__LEN));
		acc->z = acc->z >> (sizeof(short)*8-(BMA2X2_ACC_Z10_LSB__LEN +
					BMA2X2_ACC_Z_MSB__LEN));
		break;
	case 2:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_X8_LSB__REG, data, 6);
		acc->x = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_X8_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_X_MSB)<<(BMA2X2_ACC_X8_LSB__LEN));
		acc->x = acc->x << (sizeof(short)*8-(BMA2X2_ACC_X8_LSB__LEN +
					BMA2X2_ACC_X_MSB__LEN));
		acc->x = acc->x >> (sizeof(short)*8-(BMA2X2_ACC_X8_LSB__LEN +
					BMA2X2_ACC_X_MSB__LEN));

		acc->y = BMA2X2_GET_BITSLICE(data[2], BMA2X2_ACC_Y8_LSB)|
			(BMA2X2_GET_BITSLICE(data[3],
				BMA2X2_ACC_Y_MSB)<<(BMA2X2_ACC_Y8_LSB__LEN
									));
		acc->y = acc->y << (sizeof(short)*8-(BMA2X2_ACC_Y8_LSB__LEN +
					BMA2X2_ACC_Y_MSB__LEN));
		acc->y = acc->y >> (sizeof(short)*8-(BMA2X2_ACC_Y8_LSB__LEN +
					BMA2X2_ACC_Y_MSB__LEN));

		acc->z = BMA2X2_GET_BITSLICE(data[4], BMA2X2_ACC_Z8_LSB)|
			(BMA2X2_GET_BITSLICE(data[5],
				BMA2X2_ACC_Z_MSB)<<(BMA2X2_ACC_Z8_LSB__LEN));
		acc->z = acc->z << (sizeof(short)*8-(BMA2X2_ACC_Z8_LSB__LEN +
					BMA2X2_ACC_Z_MSB__LEN));
		acc->z = acc->z >> (sizeof(short)*8-(BMA2X2_ACC_Z8_LSB__LEN +
					BMA2X2_ACC_Z_MSB__LEN));
		break;
	case 3:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_X14_LSB__REG, data, 6);
		acc->x = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_X14_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_X_MSB)<<(BMA2X2_ACC_X14_LSB__LEN));
		acc->x = acc->x << (sizeof(short)*8-(BMA2X2_ACC_X14_LSB__LEN +
					BMA2X2_ACC_X_MSB__LEN));
		acc->x = acc->x >> (sizeof(short)*8-(BMA2X2_ACC_X14_LSB__LEN +
					BMA2X2_ACC_X_MSB__LEN));

		acc->y = BMA2X2_GET_BITSLICE(data[2], BMA2X2_ACC_Y14_LSB)|
			(BMA2X2_GET_BITSLICE(data[3],
				BMA2X2_ACC_Y_MSB)<<(BMA2X2_ACC_Y14_LSB__LEN
									));
		acc->y = acc->y << (sizeof(short)*8-(BMA2X2_ACC_Y14_LSB__LEN +
					BMA2X2_ACC_Y_MSB__LEN));
		acc->y = acc->y >> (sizeof(short)*8-(BMA2X2_ACC_Y14_LSB__LEN +
					BMA2X2_ACC_Y_MSB__LEN));

		acc->z = BMA2X2_GET_BITSLICE(data[4], BMA2X2_ACC_Z14_LSB)|
			(BMA2X2_GET_BITSLICE(data[5],
				BMA2X2_ACC_Z_MSB)<<(BMA2X2_ACC_Z14_LSB__LEN));
		acc->z = acc->z << (sizeof(short)*8-(BMA2X2_ACC_Z14_LSB__LEN +
					BMA2X2_ACC_Z_MSB__LEN));
		acc->z = acc->z >> (sizeof(short)*8-(BMA2X2_ACC_Z14_LSB__LEN +
					BMA2X2_ACC_Z_MSB__LEN));
		break;
	default:
		break;
	}
#endif

	bma2x2_remap_sensor_data(acc, client_data);
	return comres;
}

static void bma2x2_work_func(struct work_struct *work)
{
	struct bma2x2_data *bma2x2 = container_of((struct delayed_work *)work,
			struct bma2x2_data, work);
	static struct bma2x2acc acc;
	unsigned long delay = msecs_to_jiffies(atomic_read(&bma2x2->delay));

	bma2x2_read_accel_xyz(bma2x2->bma2x2_client, bma2x2->sensor_type,
									 &acc);
	input_report_abs(bma2x2->input, ABS_X, acc.x);
	input_report_abs(bma2x2->input, ABS_Y, acc.y);
	input_report_abs(bma2x2->input, ABS_Z, acc.z);
	input_sync(bma2x2->input);

	mutex_lock(&bma2x2->value_mutex);
	bma2x2->value.x = acc.x;
	bma2x2->value.y = acc.y;
	bma2x2->value.z = acc.z;
	mutex_unlock(&bma2x2->value_mutex);

	schedule_delayed_work(&bma2x2->work, delay);
}


static ssize_t bma2x2_register_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int address, value;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	sscanf(buf, "%d%d", &address, &value);
	if (bma2x2_write_reg(bma2x2->bma2x2_client, (unsigned char)address,
				(unsigned char *)&value) < 0)
		return -EINVAL;
	return count;
}
static ssize_t bma2x2_register_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	size_t count = 0;
	u8 reg[0x40];
	int i;

	for (i = 0; i < 0x40; i++) {
		bma2x2_smbus_read_byte(bma2x2->bma2x2_client, i, reg+i);

		count += sprintf(&buf[count], "0x%x: %d\n", i, reg[i]);
	}
	return count;


}

static ssize_t bma2x2_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_range(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_range_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_range(bma2x2->bma2x2_client, (unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_bandwidth_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_bandwidth(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_bandwidth_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2->sensor_type == BMA280_TYPE)
		if ((unsigned char) data > 14)
			return -EINVAL;

	if (bma2x2_set_bandwidth(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_mode(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
	mutex_lock(&bma2x2->data_mutex);
	atomic_set(&bma2x2->set_mode, (unsigned int)data);
#endif
	bma2x2_set_mode(bma2x2->bma2x2_client, (unsigned char) data);
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
	mutex_unlock(&bma2x2->data_mutex);
#endif
	return count;
}
static ssize_t bma2x2_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma2x2_data *bma2x2 = input_get_drvdata(input);
	struct bma2x2acc acc_value;

#if 0
	bma2x2_read_accel_xyz(bma2x2->bma2x2_client, bma2x2->sensor_type,
								&acc_value);
#else
	/* cache of last input event */
	mutex_lock(&bma2x2->value_mutex);
	acc_value.x = bma2x2->value.x;
	acc_value.y = bma2x2->value.y;
	acc_value.z = bma2x2->value.z;
	mutex_unlock(&bma2x2->value_mutex);
#endif

	return sprintf(buf, "%d %d %d\n", acc_value.x, acc_value.y,
			acc_value.z);
}
/* raw-data for sensor test */
static ssize_t bma2x2_raw_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma2x2_data *bma2x2 = input_get_drvdata(input);
	struct bma2x2acc acc_value;
#if 0
	bma2x2_read_accel_xyz(bma2x2->bma2x2_client, bma2x2->sensor_type,
								&acc_value);
#else
	/* cache of last input event */
	mutex_lock(&bma2x2->value_mutex);
	acc_value.x = bma2x2->value.x;
	acc_value.y = bma2x2->value.y;
	acc_value.z = bma2x2->value.z;
	mutex_unlock(&bma2x2->value_mutex);

#endif

	acc_value.x = acc_value.x >>4;
	acc_value.y = acc_value.y >>4;
	acc_value.z = acc_value.z >>4;

	return sprintf(buf, "%d,%d,%d\n", acc_value.x, acc_value.y,
			acc_value.z);
}

static ssize_t bma2x2_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", atomic_read(&bma2x2->delay));

}

static ssize_t bma2x2_chip_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", bma2x2->chip_id);

}


static ssize_t bma2x2_place_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_BMA_USE_PLATFORM_DATA
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
#endif
	int place = BOSCH_SENSOR_PLACE_UNKNOWN;

#ifdef CONFIG_BMA_USE_PLATFORM_DATA
	if (NULL != bma2x2->bst_pd)
		place = bma2x2->bst_pd->place;
#endif

	return sprintf(buf, "%d\n", place);
}


static ssize_t bma2x2_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);

	printk(KERN_INFO "[BMC150] %s : delay = %ld\n", __func__, data);

	if (error)
		return error;
	if (data > BMA2X2_MAX_DELAY)
		data = BMA2X2_MAX_DELAY;
	atomic_set(&bma2x2->delay, (unsigned int) data);

	return count;
}


static ssize_t bma2x2_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", atomic_read(&bma2x2->enable));

}

static void bma2x2_set_enable(struct device *dev, int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
	int pre_enable = atomic_read(&bma2x2->enable);

#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
	mutex_lock(&bma2x2->data_mutex);
#endif
	mutex_lock(&bma2x2->enable_mutex);
	if (enable) {
		if (pre_enable == 0) {
			bma2x2_set_mode(bma2x2->bma2x2_client,
					BMA2X2_MODE_NORMAL);
			schedule_delayed_work(&bma2x2->work,
				msecs_to_jiffies(atomic_read(&bma2x2->delay)));
			atomic_set(&bma2x2->enable, 1);
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
			atomic_set(&bma2x2->set_mode, (unsigned int)BMA2X2_MODE_NORMAL);
#endif
		}

	} else {
		if (pre_enable == 1) {
			bma2x2_set_mode(bma2x2->bma2x2_client, BMA2X2_MODE_SUSPEND);
			cancel_delayed_work_sync(&bma2x2->work);
			atomic_set(&bma2x2->enable, 0);
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
			atomic_set(&bma2x2->set_mode, (unsigned int)BMA2X2_MODE_SUSPEND);
#endif
		}
	}
	mutex_unlock(&bma2x2->enable_mutex);
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
	mutex_unlock(&bma2x2->data_mutex);
#endif

}

static ssize_t bma2x2_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;

	error = strict_strtoul(buf, 10, &data);

	printk(KERN_INFO "[BMC150] %s : enable = %ld\n", __func__, data);

	if (error)
		return error;
	if ((data == 0) || (data == 1))
		bma2x2_set_enable(dev, data);


	return count;
}
static ssize_t bma2x2_fast_calibration_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{


	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_offset_target(bma2x2->bma2x2_client, 1, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_fast_calibration_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_offset_target(bma2x2->bma2x2_client, 1, (unsigned
					char)data) < 0)
		return -EINVAL;

	if (bma2x2_set_cal_trigger(bma2x2->bma2x2_client, 1) < 0)
		return -EINVAL;

	do {
		mdelay(2);
		bma2x2_get_cal_ready(bma2x2->bma2x2_client, &tmp);

/*		printk(KERN_INFO "wait 2ms cal ready flag is %d\n", tmp);
 */
		timeout++;
		if (timeout == 50) {
			printk(KERN_INFO "get fast calibration ready error\n");
			return -EINVAL;
		};

	} while (tmp == 0);

	printk(KERN_INFO "x axis fast calibration finished\n");
	return count;
}

static ssize_t bma2x2_fast_calibration_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{


	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_offset_target(bma2x2->bma2x2_client, 2, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_fast_calibration_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_offset_target(bma2x2->bma2x2_client, 2, (unsigned
					char)data) < 0)
		return -EINVAL;

	if (bma2x2_set_cal_trigger(bma2x2->bma2x2_client, 2) < 0)
		return -EINVAL;

	do {
		mdelay(2);
		bma2x2_get_cal_ready(bma2x2->bma2x2_client, &tmp);

/*		printk(KERN_INFO "wait 2ms cal ready flag is %d\n", tmp);
 */
		timeout++;
		if (timeout == 50) {
			printk(KERN_INFO "get fast calibration ready error\n");
			return -EINVAL;
		};

	} while (tmp == 0);

	printk(KERN_INFO "y axis fast calibration finished\n");
	return count;
}

static ssize_t bma2x2_fast_calibration_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{


	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_offset_target(bma2x2->bma2x2_client, 3, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_fast_calibration_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_offset_target(bma2x2->bma2x2_client, 3, (unsigned
					char)data) < 0)
		return -EINVAL;

	if (bma2x2_set_cal_trigger(bma2x2->bma2x2_client, 3) < 0)
		return -EINVAL;

	do {
		mdelay(2);
		bma2x2_get_cal_ready(bma2x2->bma2x2_client, &tmp);

/*		printk(KERN_INFO "wait 2ms cal ready flag is %d\n", tmp);
 */
		timeout++;
		if (timeout == 50) {
			printk(KERN_INFO "get fast calibration ready error\n");
			return -EINVAL;
		};

	} while (tmp == 0);

	printk(KERN_INFO "z axis fast calibration finished\n");
	return count;
}

static ssize_t accel_calibration_open(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    	struct i2c_client *client = to_i2c_client(dev);
        bma2x2_open_cal(client);
	return 1;
}


static ssize_t bma2x2_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/*struct i2c_client *client = to_i2c_client(dev);*/
	/*struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);*/
	int cal_data[3];
	int err;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;
	int result = 1;

        printk(KERN_INFO "[BMC150] %s\n", __func__);  

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH,
		O_RDONLY, 0666);
	if (IS_ERR(cal_filp)) {
		pr_err("[BMC150] %s: Can't open calibration file\n",
			__func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		return err;
	}
	err = cal_filp->f_op->read(cal_filp,
		(char *)cal_data,
		3 * sizeof(int), &cal_filp->f_pos);
	if (err != 3 * sizeof(int)) {
		pr_err("[BMC150] %s: Can't read the cal data from file\n",
			__func__);
		filp_close(cal_filp, current->files);
		set_fs(old_fs);
		return -EIO;
	}
	if (((cal_data[0] == 0) &&(cal_data[1] == 0) &&(cal_data[2] == 0)))
		result = 0;

	printk(KERN_INFO "[BMC150] %s: result=%d, (%d,%d,%d)",__func__, result, cal_data[0],cal_data[1],cal_data[2]);
    
	return sprintf(buf, "%d %d %d %d\n", result, cal_data[0],cal_data[1],cal_data[2]);

}

static ssize_t bma2x2_calibration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
	int cal_data[3];
	unsigned long data;
	unsigned char mode;    
	signed char tmp;
	unsigned char timeout = 0;
	int err;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	err = strict_strtoul(buf, 10, &data);
	if (err) {
            printk(KERN_ERR "[BMC150] %s: error(%d)\n", __func__, (unsigned char) data);
            return err;
	}

        printk(KERN_INFO "[BMC150] %s: %d\n", __func__, (unsigned char) data);  

	if (bma2x2_get_mode(bma2x2->bma2x2_client, &mode) < 0) {
            printk(KERN_ERR "[BMC150] %s: bma2x2_get_mode error\n", __func__);  
            return -EINVAL;
	}
    
	if (mode != BMA2X2_MODE_NORMAL) {
            printk(KERN_INFO "[BMC150] %s: bma2x2_get_mode %d\n", __func__, mode);  
            bma2x2_set_mode(bma2x2->bma2x2_client, BMA2X2_MODE_NORMAL);
	}
       
	if(data){
		/* check the current z value and set offset_target_z based on it */
		short acc_value_z = 0;
		unsigned char offset_target_z = 0;
		bma2x2_read_accel_z(bma2x2->bma2x2_client, bma2x2->sensor_type,
									 &acc_value_z);
		printk(KERN_INFO "[BMC150] acc_value_z = [%d], while accel calibration\n", acc_value_z);
		if(acc_value_z > 0)
			offset_target_z = 1;
		else
			offset_target_z = 2;

		/* x axis fast calibration */
		if (bma2x2_set_offset_target(bma2x2->bma2x2_client, 1, (unsigned
						char)0) < 0)
			return -EINVAL;

		if (bma2x2_set_cal_trigger(bma2x2->bma2x2_client, 1) < 0)
			return -EINVAL;

		do {
			mdelay(50);
			bma2x2_get_cal_ready(bma2x2->bma2x2_client, &tmp);
			timeout++;
			if (timeout == 50) {
				printk(KERN_INFO "[BMC150] x get fast calibration ready error\n");
				return -EINVAL;
			};
		} while (tmp == 0);

		/* y axis fast calibration */
		if (bma2x2_set_offset_target(bma2x2->bma2x2_client, 2, (unsigned
						char)0) < 0)
			return -EINVAL;
	
		if (bma2x2_set_cal_trigger(bma2x2->bma2x2_client, 2) < 0)
			return -EINVAL;
	
		do {
			mdelay(50);
			bma2x2_get_cal_ready(bma2x2->bma2x2_client, &tmp);
			timeout++;
			if (timeout == 50) {
				printk(KERN_INFO "[BMC150] y get fast calibration ready error\n");
				return -EINVAL;
			};
		} while (tmp == 0);
		
		/* z axis fast calibration */
		/* check the current z value and set offset_target_z based on it */
		if (bma2x2_set_offset_target(bma2x2->bma2x2_client, 3, offset_target_z) < 0)
			return -EINVAL;

		if (bma2x2_set_cal_trigger(bma2x2->bma2x2_client, 3) < 0)
			return -EINVAL;

		do {
			mdelay(50);
			bma2x2_get_cal_ready(bma2x2->bma2x2_client, &tmp);
			timeout++;
			if (timeout == 50) {
				printk(KERN_INFO "[BMC150] z get fast calibration ready error\n");
				return -EINVAL;
			};
		} while (tmp == 0);

		/* calibration */
		bma2x2_get_offset_x(bma2x2->bma2x2_client, (unsigned char*)&cal_data[0]);
		bma2x2_get_offset_y(bma2x2->bma2x2_client, (unsigned char*)&cal_data[1]);
		bma2x2_get_offset_z(bma2x2->bma2x2_client, (unsigned char*)&cal_data[2]);
	
		printk(KERN_INFO "[BMC150] %s: (%d,%d,%d)",__func__, cal_data[0],cal_data[1],cal_data[2]);

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		cal_filp = filp_open(CALIBRATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0666);
		if (IS_ERR(cal_filp)) {
			pr_err("[BMC150] %s: Can't open calibration file\n",
				__func__);
			set_fs(old_fs);
			err = PTR_ERR(cal_filp);
			return err;
		}

		err = cal_filp->f_op->write(cal_filp,
			(char *)cal_data,
			3 * sizeof(int), &cal_filp->f_pos);
		if (err != 3 * sizeof(int)) {
			pr_err("[BMC150] %s: Can't write the cal data to file\n",
				__func__);
			err = -EIO;
		}
		filp_close(cal_filp, current->files);
		set_fs(old_fs);
	}
	else
	{
		/* erase cal data */
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		cal_filp = filp_open(CALIBRATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0666);
		if (IS_ERR(cal_filp)) {
			pr_err("[BMC150] %s: Can't open calibration file\n",
				__func__);
			set_fs(old_fs);
			err = PTR_ERR(cal_filp);
			return err;
		}
		cal_data[0] = 0;
		cal_data[1] = 0;
		cal_data[2] = 0;

		err = cal_filp->f_op->write(cal_filp,
			(char *)cal_data,
			3 * sizeof(int), &cal_filp->f_pos);
		if (err != 3 * sizeof(int)) {
			pr_err("[BMC150] %s: Can't write the cal data to file\n",
				__func__);
			err = -EIO;
		}
		filp_close(cal_filp, current->files);
		set_fs(old_fs);

		bma2x2_set_offset_x(bma2x2->bma2x2_client, (unsigned char)cal_data[0]);
	        bma2x2_set_offset_y(bma2x2->bma2x2_client, (unsigned char)cal_data[1]);
        	bma2x2_set_offset_z(bma2x2->bma2x2_client, (unsigned char)cal_data[2]);		
	}

	return count;
}

static ssize_t bma2x2_SleepDur_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_sleep_duration(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_SleepDur_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_sleep_duration(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_fifo_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_fifo_mode(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_fifo_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_fifo_mode(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}



static ssize_t bma2x2_fifo_trig_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_fifo_trig(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_fifo_trig_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_fifo_trig(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}



static ssize_t bma2x2_fifo_trig_src_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_fifo_trig_src(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_fifo_trig_src_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_fifo_trig_src(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}



static ssize_t bma2x2_fifo_data_sel_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_fifo_data_sel(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_fifo_framecount_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_fifo_framecount(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_temperature_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_read_temperature(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_fifo_data_sel_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_fifo_data_sel(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}



static ssize_t bma2x2_fifo_data_out_frame_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err, i, len;
	signed char fifo_data_out[MAX_FIFO_F_LEVEL * MAX_FIFO_F_BYTES] = {0};
	unsigned char f_count, f_len = 0;
	unsigned char fifo_datasel = 0;

	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_fifo_data_sel(bma2x2->bma2x2_client, &fifo_datasel) < 0)
		return sprintf(buf, "Read data sel error\n");

	if (fifo_datasel)
		f_len = 2;
	else
		f_len = 6;

	if (bma2x2_get_fifo_framecount(bma2x2->bma2x2_client, &f_count) < 0)
		return sprintf(buf, "Read frame count error\n");


	if (bma_i2c_burst_read(bma2x2->bma2x2_client,
			BMA2X2_FIFO_DATA_OUTPUT_REG, fifo_data_out,
							f_count * f_len) < 0)
		return sprintf(buf, "Read byte block error\n");


	if (bma2x2_get_fifo_data_out_reg(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	err = 0;

	len = sprintf(buf, "%lu ", jiffies);
	buf += len;
	err += len;

	len = sprintf(buf, "%u ", f_count);
	buf += len;
	err += len;

	len = sprintf(buf, "%u ", f_len);
	buf += len;
	err += len;

	for (i = 0; i < f_count * f_len; i++)	{
		len = sprintf(buf, "%d ", fifo_data_out[i]);
		buf += len;
		err += len;
	}

	return err;

}




static ssize_t bma2x2_offset_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_offset_x(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_offset_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_offset_x(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_offset_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_offset_y(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_offset_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_offset_y(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_offset_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_offset_z(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_offset_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_offset_z(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
static ssize_t bma2x2_reactive_enable_show(struct device *dev,
					struct device_attribute
						*attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	/*pr_info("[BMC150] %s: state =%d\n", __func__,
		atomic_read(&bma2x2->reactive_state));*/

	return sprintf(buf, "%d\n",
		atomic_read(&bma2x2->reactive_state));
}

static ssize_t bma2x2_reactive_enable_store(struct device *dev,
					struct device_attribute
						*attr, const char *buf,
							size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
	bool onoff = false;
	bool factory_test = false;
	unsigned long value = 0;
	int err = count;

	if (strict_strtoul(buf, 10, &value)) {
		err = -EINVAL;
		return err;
	}

	switch (value) {
	case 0:
		break;
	case 1:
		onoff = true;
		break;
	case 2:
		onoff = true;
		factory_test = true;
		atomic_set(&bma2x2->factory_test, 1);
		break;
	default:
		err = -EINVAL;
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return count;
	}

	if (bma2x2->IRQ) {
		if (!value) {
			disable_irq_wake(bma2x2->IRQ);
			disable_irq(bma2x2->IRQ);
		} else {
			enable_irq(bma2x2->IRQ);
			enable_irq_wake(bma2x2->IRQ);
		}
	}
	mutex_lock(&bma2x2->data_mutex);
	atomic_set(&bma2x2->reactive_enable, onoff);
	if (bma2x2->IRQ)
		bma2x2_set_motion_interrupt(bma2x2->bma2x2_client, onoff, factory_test);
	atomic_set(&bma2x2->reactive_state, false);
	mdelay(1);
	if(!atomic_read(&bma2x2->reactive_enable)&&!atomic_read(&bma2x2->set_mode)){
		pr_info("@@@@%s: ACC NOT WAKEUP DEFENSE CODE @@@@\n", __func__);
		bma2x2_set_mode(bma2x2->bma2x2_client, BMA2X2_MODE_NORMAL);
	}

	mutex_unlock(&bma2x2->data_mutex);

	pr_info("[BMC150] %s: onoff = %d, state =%d\n", __func__,
		atomic_read(&bma2x2->reactive_enable),
		atomic_read(&bma2x2->reactive_state));

	return count;
}
#endif

static DEVICE_ATTR(range, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_range_show, bma2x2_range_store);
static DEVICE_ATTR(bandwidth, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_bandwidth_show, bma2x2_bandwidth_store);
static DEVICE_ATTR(mode, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_mode_show, bma2x2_mode_store);
static DEVICE_ATTR(value, S_IRUGO,
		bma2x2_value_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO,
		bma2x2_raw_data_show, NULL);
static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_delay_show, bma2x2_delay_store);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_enable_show, bma2x2_enable_store);
static DEVICE_ATTR(SleepDur, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_SleepDur_show, bma2x2_SleepDur_store);
static DEVICE_ATTR(fast_calibration_x, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_fast_calibration_x_show,
		bma2x2_fast_calibration_x_store);
static DEVICE_ATTR(fast_calibration_y, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_fast_calibration_y_show,
		bma2x2_fast_calibration_y_store);
static DEVICE_ATTR(fast_calibration_z, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_fast_calibration_z_show,
		bma2x2_fast_calibration_z_store);
static DEVICE_ATTR(fifo_mode, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_fifo_mode_show, bma2x2_fifo_mode_store);
static DEVICE_ATTR(fifo_framecount, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_fifo_framecount_show, NULL);
static DEVICE_ATTR(fifo_trig, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_fifo_trig_show, bma2x2_fifo_trig_store);
static DEVICE_ATTR(fifo_trig_src, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_fifo_trig_src_show, bma2x2_fifo_trig_src_store);
static DEVICE_ATTR(fifo_data_sel, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_fifo_data_sel_show, bma2x2_fifo_data_sel_store);
static DEVICE_ATTR(fifo_data_out_frame, S_IRUGO,
		bma2x2_fifo_data_out_frame_show, NULL);
static DEVICE_ATTR(reg, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_register_show, bma2x2_register_store);
static DEVICE_ATTR(chip_id, S_IRUGO,
		bma2x2_chip_id_show, NULL);
static DEVICE_ATTR(offset_x, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_offset_x_show,
		bma2x2_offset_x_store);
static DEVICE_ATTR(offset_y, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_offset_y_show,
		bma2x2_offset_y_store);
static DEVICE_ATTR(offset_z, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_offset_z_show,
		bma2x2_offset_z_store);
static DEVICE_ATTR(enable_int, S_IWUSR|S_IWGRP,
		NULL, bma2x2_enable_int_store);
static DEVICE_ATTR(int_mode, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_int_mode_show, bma2x2_int_mode_store);
static DEVICE_ATTR(slope_duration, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_slope_duration_show, bma2x2_slope_duration_store);
static DEVICE_ATTR(slope_threshold, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_slope_threshold_show, bma2x2_slope_threshold_store);
static DEVICE_ATTR(slope_no_mot_duration, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_slope_no_mot_duration_show,
			bma2x2_slope_no_mot_duration_store);
static DEVICE_ATTR(slope_no_mot_threshold, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_slope_no_mot_threshold_show,
			bma2x2_slope_no_mot_threshold_store);
static DEVICE_ATTR(high_g_duration, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_high_g_duration_show, bma2x2_high_g_duration_store);
static DEVICE_ATTR(high_g_threshold, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_high_g_threshold_show, bma2x2_high_g_threshold_store);
static DEVICE_ATTR(low_g_duration, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_low_g_duration_show, bma2x2_low_g_duration_store);
static DEVICE_ATTR(low_g_threshold, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_low_g_threshold_show, bma2x2_low_g_threshold_store);
static DEVICE_ATTR(tap_duration, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_tap_duration_show, bma2x2_tap_duration_store);
static DEVICE_ATTR(tap_threshold, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_tap_threshold_show, bma2x2_tap_threshold_store);
static DEVICE_ATTR(tap_quiet, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_tap_quiet_show, bma2x2_tap_quiet_store);
static DEVICE_ATTR(tap_shock, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_tap_shock_show, bma2x2_tap_shock_store);
static DEVICE_ATTR(tap_samp, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_tap_samp_show, bma2x2_tap_samp_store);
static DEVICE_ATTR(orient_mode, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_orient_mode_show, bma2x2_orient_mode_store);
static DEVICE_ATTR(orient_blocking, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_orient_blocking_show, bma2x2_orient_blocking_store);
static DEVICE_ATTR(orient_hyst, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_orient_hyst_show, bma2x2_orient_hyst_store);
static DEVICE_ATTR(orient_theta, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_orient_theta_show, bma2x2_orient_theta_store);
static DEVICE_ATTR(flat_theta, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_flat_theta_show, bma2x2_flat_theta_store);
static DEVICE_ATTR(flat_hold_time, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_flat_hold_time_show, bma2x2_flat_hold_time_store);
static DEVICE_ATTR(selftest, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_selftest_show, bma2x2_selftest_store);
static DEVICE_ATTR(softreset, S_IWUSR|S_IWGRP,
		NULL, bma2x2_softreset_store);
static DEVICE_ATTR(temperature, S_IRUGO,
		bma2x2_temperature_show, NULL);
static DEVICE_ATTR(place, S_IRUGO,
		bma2x2_place_show, NULL);
static DEVICE_ATTR(calibration, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_calibration_show,
		bma2x2_calibration_store);
static DEVICE_ATTR(accel_cal_open, S_IRUGO,
		accel_calibration_open,NULL);
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
static DEVICE_ATTR(reactive_alert, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_reactive_enable_show, bma2x2_reactive_enable_store);
#endif

static struct attribute  *bma2x2_attributes[] = {
	&dev_attr_range.attr,
	&dev_attr_bandwidth.attr,
	&dev_attr_mode.attr,
	&dev_attr_raw_data.attr,
	&dev_attr_value.attr,
	&dev_attr_delay.attr,
	&dev_attr_enable.attr,
	&dev_attr_SleepDur.attr,
	&dev_attr_reg.attr,
	&dev_attr_fast_calibration_x.attr,
	&dev_attr_fast_calibration_y.attr,
	&dev_attr_fast_calibration_z.attr,
	&dev_attr_fifo_mode.attr,
	&dev_attr_fifo_framecount.attr,
	&dev_attr_fifo_trig.attr,
	&dev_attr_fifo_trig_src.attr,
	&dev_attr_fifo_data_sel.attr,
	&dev_attr_fifo_data_out_frame.attr,
	&dev_attr_chip_id.attr,
	&dev_attr_offset_x.attr,
	&dev_attr_offset_y.attr,
	&dev_attr_offset_z.attr,
	&dev_attr_enable_int.attr,
	&dev_attr_int_mode.attr,
	&dev_attr_slope_duration.attr,
	&dev_attr_slope_threshold.attr,
	&dev_attr_slope_no_mot_duration.attr,
	&dev_attr_slope_no_mot_threshold.attr,
	&dev_attr_high_g_duration.attr,
	&dev_attr_high_g_threshold.attr,
	&dev_attr_low_g_duration.attr,
	&dev_attr_low_g_threshold.attr,
	&dev_attr_tap_threshold.attr,
	&dev_attr_tap_duration.attr,
	&dev_attr_tap_quiet.attr,
	&dev_attr_tap_shock.attr,
	&dev_attr_tap_samp.attr,
	&dev_attr_orient_mode.attr,
	&dev_attr_orient_blocking.attr,
	&dev_attr_orient_hyst.attr,
	&dev_attr_orient_theta.attr,
	&dev_attr_flat_theta.attr,
	&dev_attr_flat_hold_time.attr,
	&dev_attr_selftest.attr,
	&dev_attr_softreset.attr,
	&dev_attr_temperature.attr,
	&dev_attr_place.attr,
	&dev_attr_calibration.attr,
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
	&dev_attr_reactive_alert.attr,
#endif
	NULL
};

static struct device_attribute *accelerometer_attrs[] = {
	&dev_attr_raw_data,
	&dev_attr_calibration,
	&dev_attr_accel_cal_open,	
	&dev_attr_selftest,	
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
	&dev_attr_reactive_alert,
#endif
	NULL,
};

static struct attribute_group bma2x2_attribute_group = {
	.attrs = bma2x2_attributes
};


#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
static void bma2x2_work_func_alert(struct work_struct *work)
{
	struct bma2x2_data *bma2x2 = container_of(work,
			struct bma2x2_data, alert_work);
	int result;

	result = bma2x2_get_motion_interrupt(bma2x2->bma2x2_client);
	if (result || bma2x2->factory_mode) {
		/*handle motion recognition*/
		atomic_set(&bma2x2->reactive_state, true);
		bma2x2->factory_mode = false;
		pr_info("[BMC150] %s: motion interrupt happened\n",
			__func__);
		wake_lock_timeout(&bma2x2->reactive_wake_lock,
			msecs_to_jiffies(2000));
	}
}

irqreturn_t bma2x2_acc_irq_thread(int irq, void *dev)
{
	struct bma2x2_data *data = dev;
	schedule_work(&data->alert_work);
	return IRQ_HANDLED;
}
#endif

#if defined(BMA2X2_ENABLE_INT1) || defined(BMA2X2_ENABLE_INT2)
unsigned char *orient[] = {"upward looking portrait upright",   \
	"upward looking portrait upside-down",   \
		"upward looking landscape left",   \
		"upward looking landscape right",   \
		"downward looking portrait upright",   \
		"downward looking portrait upside-down",   \
		"downward looking landscape left",   \
		"downward looking landscape right"};

static void bma2x2_irq_work_func(struct work_struct *work)
{
	struct bma2x2_data *bma2x2 = container_of((struct work_struct *)work,
			struct bma2x2_data, irq_work);

	unsigned char status = 0;
	unsigned char i;
	unsigned char first_value = 0;
	unsigned char sign_value = 0;

	bma2x2_get_interruptstatus1(bma2x2->bma2x2_client, &status);

	switch (status) {

	case 0x01:
		printk(KERN_INFO "Low G interrupt happened\n");
		input_report_rel(bma2x2->input, LOW_G_INTERRUPT,
				LOW_G_INTERRUPT_HAPPENED);
		break;
	case 0x02:
		for (i = 0; i < 3; i++) {
			bma2x2_get_HIGH_first(bma2x2->bma2x2_client, i,
					   &first_value);
			if (first_value == 1) {

				bma2x2_get_HIGH_sign(bma2x2->bma2x2_client,
						   &sign_value);

				if (sign_value == 1) {
					if (i == 0)
						input_report_rel(bma2x2->input,
						HIGH_G_INTERRUPT,
					HIGH_G_INTERRUPT_X_NEGATIVE_HAPPENED);
					if (i == 1)
						input_report_rel(bma2x2->input,
						HIGH_G_INTERRUPT,
					HIGH_G_INTERRUPT_Y_NEGATIVE_HAPPENED);
					if (i == 2)
						input_report_rel(bma2x2->input,
						HIGH_G_INTERRUPT,
					HIGH_G_INTERRUPT_Z_NEGATIVE_HAPPENED);
				} else {
					if (i == 0)
						input_report_rel(bma2x2->input,
						HIGH_G_INTERRUPT,
					HIGH_G_INTERRUPT_X_HAPPENED);
					if (i == 1)
						input_report_rel(bma2x2->input,
						HIGH_G_INTERRUPT,
					HIGH_G_INTERRUPT_Y_HAPPENED);
					if (i == 2)
						input_report_rel(bma2x2->input,
						HIGH_G_INTERRUPT,
					HIGH_G_INTERRUPT_Z_HAPPENED);

				}
			   }

		      printk(KERN_INFO "High G interrupt happened,exis is %d,"
				      "first is %d,sign is %d\n", i,
					   first_value, sign_value);
		}
		   break;
	case 0x04:
		for (i = 0; i < 3; i++) {
			bma2x2_get_slope_first(bma2x2->bma2x2_client, i,
					   &first_value);
			if (first_value == 1) {

				bma2x2_get_slope_sign(bma2x2->bma2x2_client,
						   &sign_value);

				if (sign_value == 1) {
					if (i == 0)
						input_report_rel(bma2x2->input,
						SLOP_INTERRUPT,
					SLOPE_INTERRUPT_X_NEGATIVE_HAPPENED);
					else if (i == 1)
						input_report_rel(bma2x2->input,
						SLOP_INTERRUPT,
					SLOPE_INTERRUPT_Y_NEGATIVE_HAPPENED);
					else if (i == 2)
						input_report_rel(bma2x2->input,
						SLOP_INTERRUPT,
					SLOPE_INTERRUPT_Z_NEGATIVE_HAPPENED);
				} else {
					if (i == 0)
						input_report_rel(bma2x2->input,
								SLOP_INTERRUPT,
						SLOPE_INTERRUPT_X_HAPPENED);
					else if (i == 1)
						input_report_rel(bma2x2->input,
								SLOP_INTERRUPT,
						SLOPE_INTERRUPT_Y_HAPPENED);
					else if (i == 2)
						input_report_rel(bma2x2->input,
								SLOP_INTERRUPT,
						SLOPE_INTERRUPT_Z_HAPPENED);

				}
			}

			printk(KERN_INFO "Slop interrupt happened,exis is %d,"
					"first is %d,sign is %d\n", i,
					first_value, sign_value);
		}
		break;

	case 0x08:
		printk(KERN_INFO "slow/ no motion interrupt happened\n");
		input_report_rel(bma2x2->input, SLOW_NO_MOTION_INTERRUPT,
					SLOW_NO_MOTION_INTERRUPT_HAPPENED);
		break;

	case 0x10:
		printk(KERN_INFO "double tap interrupt happened\n");
		input_report_rel(bma2x2->input, DOUBLE_TAP_INTERRUPT,
					DOUBLE_TAP_INTERRUPT_HAPPENED);
		break;
	case 0x20:
		printk(KERN_INFO "single tap interrupt happened\n");
		input_report_rel(bma2x2->input, SINGLE_TAP_INTERRUPT,
					SINGLE_TAP_INTERRUPT_HAPPENED);
		break;
	case 0x40:
		bma2x2_get_orient_status(bma2x2->bma2x2_client,
				    &first_value);
		printk(KERN_INFO "orient interrupt happened,%s\n",
				orient[first_value]);
		if (first_value == 0)
			input_report_abs(bma2x2->input, ORIENT_INTERRUPT,
				UPWARD_PORTRAIT_UP_INTERRUPT_HAPPENED);
		else if (first_value == 1)
			input_report_abs(bma2x2->input, ORIENT_INTERRUPT,
				UPWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED);
		else if (first_value == 2)
			input_report_abs(bma2x2->input, ORIENT_INTERRUPT,
				UPWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED);
		else if (first_value == 3)
			input_report_abs(bma2x2->input, ORIENT_INTERRUPT,
				UPWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED);
		else if (first_value == 4)
			input_report_abs(bma2x2->input, ORIENT_INTERRUPT,
				DOWNWARD_PORTRAIT_UP_INTERRUPT_HAPPENED);
		else if (first_value == 5)
			input_report_abs(bma2x2->input, ORIENT_INTERRUPT,
				DOWNWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED);
		else if (first_value == 6)
			input_report_abs(bma2x2->input, ORIENT_INTERRUPT,
				DOWNWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED);
		else if (first_value == 7)
			input_report_abs(bma2x2->input, ORIENT_INTERRUPT,
				DOWNWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED);
		break;
	case 0x80:
		bma2x2_get_orient_flat_status(bma2x2->bma2x2_client,
				    &sign_value);
		printk(KERN_INFO "flat interrupt happened,flat status is %d\n",
				    sign_value);
		if (sign_value == 1) {
			input_report_abs(bma2x2->input, FLAT_INTERRUPT,
				FLAT_INTERRUPT_TURE_HAPPENED);
		} else {
			input_report_abs(bma2x2->input, FLAT_INTERRUPT,
				FLAT_INTERRUPT_FALSE_HAPPENED);
		}
		break;
	default:
		break;
	}

}

static irqreturn_t bma2x2_irq_handler(int irq, void *handle)
{


	struct bma2x2_data *data = handle;


	if (data == NULL)
		return IRQ_HANDLED;
	if (data->bma2x2_client == NULL)
		return IRQ_HANDLED;


	schedule_work(&data->irq_work);

	return IRQ_HANDLED;


}
#endif /* defined(BMA2X2_ENABLE_INT1)||defined(BMA2X2_ENABLE_INT2) */

#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
static int bma2x2_setup_irq(struct bma2x2_data *bma2x2)
{
	int rc = -EIO;
	int irq = -1;

	if (NULL == bma2x2->bst_pd) {
		pr_err("[BMC150] %s: bma2x2 irq was not setting\n",__func__);
		return rc;
	}

	rc = gpio_request(bma2x2->bst_pd->irq, "gpio_bma2x2_int");
	if (rc < 0) {
		pr_err("[BMC150] %s: gpio %d request failed (%d)\n",
			__func__, bma2x2->bst_pd->irq, rc);
		return rc;
	}
	printk(KERN_ERR "[BMC150] Request GPIO_%d Sucess!\n", bma2x2->bst_pd->irq);

	rc = gpio_direction_input(bma2x2->bst_pd->irq);
	if (rc < 0) {
		pr_err("[BMC150] %s: failed to set gpio %d as input (%d)\n",
			__func__, bma2x2->bst_pd->irq, rc);
		goto err_gpio_direction_input;
	}

	irq = gpio_to_irq(bma2x2->bst_pd->irq);
	if (irq > 0) {
		rc = request_threaded_irq(irq,
			NULL, bma2x2_acc_irq_thread,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
			"accelerometer", bma2x2);
		pr_info("[BMC150] %s: irq = %d\n", __func__, irq);
	
		if (rc < 0) {
			pr_err("[BMC150] %s request_threaded_irq fail err=%d\n",
				__func__, rc);
			return rc;
		} else {
                    printk(KERN_INFO "[BMC150] request_irq success IRQ_NO:%d\n", irq);
		}
		/* start with interrupts disabled */
		disable_irq(irq);
	}
	bma2x2->IRQ = irq;
	goto done;

err_gpio_direction_input:
	gpio_free(bma2x2->bst_pd->irq);
done:
	return rc;
}
#endif


#if defined(CONFIG_SENSORS_HSCDTD006A) || defined(CONFIG_SENSORS_HSCDTD008A)
static struct bma2x2_data *g_bma2x2 = NULL;
static atomic_t flgEna;
static atomic_t delay;
int accsns_get_acceleration_data(int *xyz)
{
	struct bma2x2acc acc_value;        

        if(g_bma2x2 == NULL) return 1;

	bma2x2_read_accel_xyz(g_bma2x2->bma2x2_client, g_bma2x2->sensor_type, &acc_value);

	mutex_lock(&g_bma2x2->value_mutex);
	g_bma2x2->value.x = acc_value.x;
	g_bma2x2->value.y = acc_value.y;
	g_bma2x2->value.z = acc_value.z;
	mutex_unlock(&g_bma2x2->value_mutex);

	acc_value.x = acc_value.x >>4;
	acc_value.y = acc_value.y >>4;
	acc_value.z = acc_value.z >>4;
    
        xyz[0] = (int)(acc_value.x);
        xyz[1] = (int)(acc_value.y);
        xyz[2] = (int)(acc_value.z);

	//printk(KERN_INFO "[BMC150] Acc_I2C, x:%d, y:%d, z:%d\n", xyz[0], xyz[1], xyz[2]);
	return 0;
}

void accsns_activate(int flgatm, int flg, int dtime)
{
    int pre_enable;

    if(g_bma2x2 == NULL) return;
        
    pre_enable= atomic_read(&g_bma2x2->enable);
    
    printk(KERN_INFO "[BMC150] accsns_activate : flgatm=%d, flg=%d, dtime=%d\n", flgatm, flg, dtime);

    if (flg != 0) flg = 1;

#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
	mutex_lock(&g_bma2x2->data_mutex);
#endif
	mutex_lock(&g_bma2x2->enable_mutex);
    //Power modes
    if (flg == 0) //sleep
    {
        if (pre_enable == 1) {
            bma2x2_set_mode(g_bma2x2->bma2x2_client, BMA2X2_MODE_SUSPEND);
            atomic_set(&g_bma2x2->enable, 0);
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
            atomic_set(&g_bma2x2->set_mode, (unsigned int)BMA2X2_MODE_SUSPEND);
#endif            
        }
    }
    else
    {
        if (pre_enable == 0) {
            bma2x2_set_mode(g_bma2x2->bma2x2_client, BMA2X2_MODE_NORMAL);
            atomic_set(&g_bma2x2->enable, 1);
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
            atomic_set(&g_bma2x2->set_mode, (unsigned int)BMA2X2_MODE_NORMAL);
#endif            
        }
    }
    	mutex_unlock(&g_bma2x2->enable_mutex);
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
	mutex_unlock(&g_bma2x2->data_mutex);
#endif    

    if (flgatm) {
        atomic_set(&flgEna, flg);
        atomic_set(&delay, dtime);
    }
}
EXPORT_SYMBOL(accsns_get_acceleration_data);
EXPORT_SYMBOL(accsns_activate);
#endif

static int bma2x2_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = 0;
	int tempvalue;
	unsigned char tmp_chip_id;
	struct bma2x2_data *data;
	struct input_dev *dev;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_INFO "i2c_check_functionality error\n");
		goto exit;
	}
	data = kzalloc(sizeof(struct bma2x2_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	/* do soft reset */
	if (bma2x2_soft_reset(client) < 0) {
		err = -EINVAL;
		goto kfree_exit;
	}
	mdelay(5);

	/* read chip id */
	tempvalue = i2c_smbus_read_word_data(client, BMA2X2_CHIP_ID_REG);
	tmp_chip_id = tempvalue&0x00ff;

	switch (tmp_chip_id) {
	case BMA255_CHIP_ID:
		data->sensor_type = BMA255_TYPE;
		break;
	case BMA250E_CHIP_ID:
		data->sensor_type = BMA250E_TYPE;
		break;
	case BMA222E_CHIP_ID:
		data->sensor_type = BMA222E_TYPE;
		break;
	case BMA280_CHIP_ID:
		data->sensor_type = BMA280_TYPE;
		break;
	default:
		data->sensor_type = -1;
	}
	if (data->sensor_type != -1) {
		data->chip_id = tmp_chip_id;
		printk(KERN_INFO "Bosch Sensortec Device detected!\n"
				"%s registered I2C driver!\n",
						sensor_name[data->sensor_type]);
	} else{
		printk(KERN_INFO "Bosch Sensortec Device not found"
				"i2c error %d\n", tempvalue);
		err = -ENODEV;
		goto kfree_exit;
	}
	i2c_set_clientdata(client, data);
	data->bma2x2_client = client;
	mutex_init(&data->value_mutex);
	mutex_init(&data->mode_mutex);
	mutex_init(&data->enable_mutex);
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
	mutex_init(&data->data_mutex);
#endif
	err = bma2x2_set_bandwidth(client, BMA2X2_BW_SET);
	if (err < 0){
		printk(KERN_INFO "%s set bandwidth failed!\n",
			sensor_name[data->sensor_type]);
	}

	err = bma2x2_set_range(client, BMA2X2_RANGE_SET);
	if (err < 0){
		printk(KERN_INFO "%s set g-range failed!\n",
			sensor_name[data->sensor_type]);
	}


#if defined(BMA2X2_ENABLE_INT1) || defined(BMA2X2_ENABLE_INT2)
	bma2x2_set_Int_Mode(client, 1);/*latch interrupt 250ms*/
#endif

#ifdef BMA2X2_ENABLE_INT1
	/* maps interrupt to INT1 pin */
	bma2x2_set_int1_pad_sel(client, PAD_LOWG);
	bma2x2_set_int1_pad_sel(client, PAD_HIGHG);
	bma2x2_set_int1_pad_sel(client, PAD_SLOP);
	bma2x2_set_int1_pad_sel(client, PAD_DOUBLE_TAP);
	bma2x2_set_int1_pad_sel(client, PAD_SINGLE_TAP);
	bma2x2_set_int1_pad_sel(client, PAD_ORIENT);
	bma2x2_set_int1_pad_sel(client, PAD_FLAT);
	bma2x2_set_int1_pad_sel(client, PAD_SLOW_NO_MOTION);
#endif

#ifdef BMA2X2_ENABLE_INT2
	/* maps interrupt to INT2 pin */
	bma2x2_set_int2_pad_sel(client, PAD_LOWG);
	bma2x2_set_int2_pad_sel(client, PAD_HIGHG);
	bma2x2_set_int2_pad_sel(client, PAD_SLOP);
	bma2x2_set_int2_pad_sel(client, PAD_DOUBLE_TAP);
	bma2x2_set_int2_pad_sel(client, PAD_SINGLE_TAP);
	bma2x2_set_int2_pad_sel(client, PAD_ORIENT);
	bma2x2_set_int2_pad_sel(client, PAD_FLAT);
	bma2x2_set_int2_pad_sel(client, PAD_SLOW_NO_MOTION);
#endif

#if defined(BMA2X2_ENABLE_INT1) || defined(BMA2X2_ENABLE_INT2)
	data->IRQ = client->irq;
	err = request_irq(data->IRQ, bma2x2_irq_handler, IRQF_TRIGGER_RISING,
			"bma2x2", data);
	if (err)
		printk(KERN_ERR "could not request irq\n");

	INIT_WORK(&data->irq_work, bma2x2_irq_work_func);
#endif

	dev = input_allocate_device();
	if (!dev){
		goto error_input_allocate_device_bma2x2;
	}
	dev->name = SENSOR_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_REL, SLOW_NO_MOTION_INTERRUPT);
	input_set_capability(dev, EV_REL, LOW_G_INTERRUPT);
	input_set_capability(dev, EV_REL, HIGH_G_INTERRUPT);
	input_set_capability(dev, EV_REL, SLOP_INTERRUPT);
	input_set_capability(dev, EV_REL, DOUBLE_TAP_INTERRUPT);
	input_set_capability(dev, EV_REL, SINGLE_TAP_INTERRUPT);
	input_set_capability(dev, EV_ABS, ORIENT_INTERRUPT);
	input_set_capability(dev, EV_ABS, FLAT_INTERRUPT);
	input_set_abs_params(dev, ABS_X, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Y, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Z, ABSMIN, ABSMAX, 0, 0);

	input_set_drvdata(dev, data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		goto error_input_allocate_device_bma2x2;
	}
	data->input = dev;

	err = sysfs_create_group(&data->input->dev.kobj,
			&bma2x2_attribute_group);
	if (err < 0){
		goto error_sysfs;
	}

	INIT_DELAYED_WORK(&data->work, bma2x2_work_func);
	atomic_set(&data->delay, BMA2X2_MAX_DELAY);
	atomic_set(&data->enable, 0);

#ifdef CONFIG_BMA_USE_PLATFORM_DATA
	if (NULL != client->dev.platform_data) {
		data->bst_pd = kzalloc(sizeof(*data->bst_pd),
				GFP_KERNEL);

		if (NULL != data->bst_pd) {
			memcpy(data->bst_pd, client->dev.platform_data,
					sizeof(*data->bst_pd));
			printk(KERN_INFO "[BMC150] bma2x2 place of bma in %s: %d\n",
					data->bst_pd->name,
					data->bst_pd->place);
		}
	}
        else{
            printk(KERN_ERR "[BMC150] could not remap bma2x2 axis\n");
        }
#endif
	
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT

	INIT_WORK(&data->alert_work, bma2x2_work_func_alert);
	wake_lock_init(&data->reactive_wake_lock, WAKE_LOCK_SUSPEND, "reactive_wake_lock");

	err = bma2x2_setup_irq(data);
	if (err) {
		pr_err("%s: could not setup irq\n", __func__);
		goto exit_bma2x2_acc_alert_int;
	}
#endif

	err = sensors_register(&bma2x2_device,
			data, accelerometer_attrs,"accelerometer_sensor");
	if (err < 0) {
        	pr_info("%s: could not sensors_register\n", __func__);
        	goto exit_bma2x2_sensors_register;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = bma2x2_early_suspend;
	data->early_suspend.resume = bma2x2_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

#if defined(CONFIG_SENSORS_HSCDTD006A) || defined(CONFIG_SENSORS_HSCDTD008A)
        g_bma2x2 = data;
#endif

	return 0;

exit_bma2x2_sensors_register:
	sysfs_remove_group(&data->input->dev.kobj,
		&bma2x2_attribute_group);
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
exit_bma2x2_acc_alert_int:
        wake_lock_destroy(&data->reactive_wake_lock);
#endif
#ifdef CONFIG_BMA_USE_PLATFORM_DATA
	if ((NULL != data) && (NULL != data->bst_pd)) {
		kfree(data->bst_pd);
		data->bst_pd = NULL;
	}
#endif
error_sysfs:
	input_unregister_device(data->input);
error_input_allocate_device_bma2x2:
	mutex_destroy(&data->value_mutex);
	mutex_destroy(&data->mode_mutex);
	mutex_destroy(&data->enable_mutex);
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
	mutex_destroy(&data->data_mutex);
#endif
kfree_exit:
	kfree(data);
#if defined(CONFIG_SENSORS_HSCDTD006A) || defined(CONFIG_SENSORS_HSCDTD008A)
        g_bma2x2 = NULL;
#endif
exit:
	return err;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bma2x2_early_suspend(struct early_suspend *h)
{
#if 0    
	struct bma2x2_data *data =
		container_of(h, struct bma2x2_data, early_suspend);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
		if(!atomic_read(&data->reactive_enable)){
			bma2x2_set_mode(data->bma2x2_client, BMA2X2_MODE_SUSPEND);
		}
#else
		bma2x2_set_mode(data->bma2x2_client, BMA2X2_MODE_SUSPEND);
#endif
		cancel_delayed_work_sync(&data->work);
		atomic_set(&data->enable, 0);
	}
	mutex_unlock(&data->enable_mutex);
#endif    
}


static void bma2x2_late_resume(struct early_suspend *h)
{
#if 0    
	struct bma2x2_data *data =
		container_of(h, struct bma2x2_data, early_suspend);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 0) {
		bma2x2_set_mode(data->bma2x2_client, BMA2X2_MODE_NORMAL);
		schedule_delayed_work(&data->work,
				msecs_to_jiffies(atomic_read(&data->delay)));
		atomic_set(&data->enable, 1);
	}
	mutex_unlock(&data->enable_mutex);
#endif    
}
#endif

static int bma2x2_remove(struct i2c_client *client)
{
	struct bma2x2_data *data = i2c_get_clientdata(client);

	bma2x2_set_enable(&client->dev, 0);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
        sensors_unregister(bma2x2_device, accelerometer_attrs);
	sysfs_remove_group(&data->input->dev.kobj, &bma2x2_attribute_group);
	input_unregister_device(data->input);

#ifdef CONFIG_BMA_USE_PLATFORM_DATA
	if (NULL != data->bst_pd) {
		kfree(data->bst_pd);
		data->bst_pd = NULL;
	}
#endif
#ifdef CONFIG_INPUT_BMA2x2_ACC_ALERT_INT
	wake_lock_destroy(&data->reactive_wake_lock);
#endif
	input_unregister_device(data->input);
	kfree(data);

	return 0;
}

void bma2x2_shutdown(struct i2c_client *client)
{
	struct bma2x2_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->enable_mutex);
	bma2x2_set_mode(data->bma2x2_client, BMA2X2_MODE_DEEP_SUSPEND);
	mutex_unlock(&data->enable_mutex);
}

#ifdef CONFIG_PM
#ifndef CONFIG_HAS_EARLYSUSPEND
static int bma2x2_suspend(struct i2c_client *client, pm_message_t mesg)
{
#if 0    
	struct bma2x2_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		bma2x2_set_mode(data->bma2x2_client, BMA2X2_MODE_SUSPEND);
		cancel_delayed_work_sync(&data->work);
	}
	mutex_unlock(&data->enable_mutex);
#endif
	return 0;
}

static int bma2x2_resume(struct i2c_client *client)
{
#if 0    
	struct bma2x2_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		bma2x2_set_mode(data->bma2x2_client, BMA2X2_MODE_NORMAL);
		schedule_delayed_work(&data->work,
				msecs_to_jiffies(atomic_read(&data->delay)));
	}
	mutex_unlock(&data->enable_mutex);
#endif
	return 0;
}
#endif
#else

#define bma2x2_suspend		NULL
#define bma2x2_resume		NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id bma2x2_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, bma2x2_id);

static struct i2c_driver bma2x2_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= SENSOR_NAME,
	},
	.shutdown	= bma2x2_shutdown,
//	.suspend	= bma2x2_suspend,
//	.resume		= bma2x2_resume,
	.id_table	= bma2x2_id,
	.probe		= bma2x2_probe,
	.remove		= bma2x2_remove,

};

static int __init BMA2X2_init(void)
{   
        struct regulator *power_regulator = NULL;

        printk(KERN_INFO "[BMC150] %s\n",__FUNCTION__);

        power_regulator = regulator_get(NULL, "tcxldo_uc");
        if (IS_ERR(power_regulator)){
                printk(KERN_ERR "[BMC150] can not get prox_regulator (SENSOR_3.3V) \n");
        } else {
                int ret = regulator_set_voltage(power_regulator,3300000,3300000);
                printk(KERN_INFO "[BMC150] regulator_set_voltage : %d\n", ret);
                ret = regulator_enable(power_regulator);
                printk(KERN_INFO "[BMC150] regulator_enable : %d\n", ret);
                regulator_put(power_regulator);
        }
        mdelay(10); //for preventing i2c fail of reading chip id
	
	return i2c_add_driver(&bma2x2_driver);
}

static void __exit BMA2X2_exit(void)
{
	i2c_del_driver(&bma2x2_driver);
}

MODULE_AUTHOR("Bosch Sensortec <contact@bosch-sensortec.com>");
MODULE_DESCRIPTION("BMA2X2 accelerometer sensor driver");
MODULE_LICENSE("GPL");

module_init(BMA2X2_init);
module_exit(BMA2X2_exit);

