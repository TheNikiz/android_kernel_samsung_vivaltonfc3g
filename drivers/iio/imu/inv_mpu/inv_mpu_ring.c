/*
* Copyright (C) 2012 Invensense, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

/**
 *  @addtogroup  DRIVERS
 *  @brief       Hardware drivers.
 *
 *  @{
 *      @file    inv_mpu_ring.c
 *      @brief   A sysfs device driver for Invensense gyroscopes.
 *      @details This file is part of inv mpu iio driver code
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>

#include "inv_mpu_iio.h"

/**
 *  reset_fifo_mpu3050() - Reset FIFO related registers
 *  @st:	Device driver instance.
 */
static int reset_fifo_mpu3050(struct iio_dev *indio_dev)
{
	struct inv_reg_map_s *reg;
	int result;
	u8 val, user_ctrl;
	struct inv_mpu_iio_s  *st = iio_priv(indio_dev);
	reg = &st->reg;

	/* disable interrupt */
	result = inv_i2c_single_write(st, reg->int_enable,
				st->plat_data.int_config);
	if (result)
		return result;
	/* disable the sensor output to FIFO */
	result = inv_i2c_single_write(st, reg->fifo_en, 0);
	if (result)
		goto reset_fifo_fail;
	result = inv_i2c_read(st, reg->user_ctrl, 1, &user_ctrl);
	if (result)
		goto reset_fifo_fail;
	/* disable fifo reading */
	user_ctrl &= ~BIT_FIFO_EN;
	st->chip_config.has_footer = 0;
	/* reset fifo */
	val = (BIT_3050_FIFO_RST | user_ctrl);
	result = inv_i2c_single_write(st, reg->user_ctrl, val);
	if (result)
		goto reset_fifo_fail;
	st->last_isr_time = get_time_ns();
	if (st->chip_config.dmp_on) {
		/* enable interrupt when DMP is done */
		result = inv_i2c_single_write(st, reg->int_enable,
				st->plat_data.int_config | BIT_DMP_INT_EN);
		if (result)
			return result;

		result = inv_i2c_single_write(st, reg->user_ctrl,
			BIT_FIFO_EN|user_ctrl);
		if (result)
			return result;
	} else {
		/* enable interrupt */
		if (st->chip_config.accl_fifo_enable ||
		    st->chip_config.gyro_fifo_enable) {
			result = inv_i2c_single_write(st, reg->int_enable,
				st->plat_data.int_config | BIT_DATA_RDY_EN);
			if (result)
				return result;
		}
		/* enable FIFO reading and I2C master interface*/
		result = inv_i2c_single_write(st, reg->user_ctrl,
			BIT_FIFO_EN | user_ctrl);
		if (result)
			return result;
		/* enable sensor output to FIFO and FIFO footer*/
		val = 1;
		if (st->chip_config.accl_fifo_enable)
			val |= BITS_3050_ACCL_OUT;
		if (st->chip_config.gyro_fifo_enable)
			val |= BITS_GYRO_OUT;
		result = inv_i2c_single_write(st, reg->fifo_en, val);
		if (result)
			return result;
	}

	return 0;
reset_fifo_fail:
	if (st->chip_config.dmp_on)
		val = BIT_DMP_INT_EN;
	else
		val = BIT_DATA_RDY_EN;
	inv_i2c_single_write(st, reg->int_enable,
			     st->plat_data.int_config | val);
	pr_err("reset fifo failed\n");

	return result;
}

/**
 *  inv_lpa_mode() - store current low power mode settings
 */
static int inv_lpa_mode(struct inv_mpu_iio_s *st, int lpa_mode)
{
	unsigned long result;
	u8 d;
	struct inv_reg_map_s *reg;

	reg = &st->reg;
	result = inv_i2c_read(st, reg->pwr_mgmt_1, 1, &d);
	if (result)
		return result;
	if (lpa_mode)
		d |= BIT_CYCLE;
	else
		d &= ~BIT_CYCLE;

	result = inv_i2c_single_write(st, reg->pwr_mgmt_1, d);
	if (result)
		return result;
	if (INV_MPU6500 == st->chip_type) {
		if (lpa_mode)
			result = inv_i2c_single_write(st,
						REG_6500_ACCEL_CONFIG2,
						BIT_ACCEL_FCHOCIE_B);
		else
			result = inv_i2c_single_write(st, 0,
						BIT_ACCEL_FCHOCIE_B);

		if (result)
			return result;
	}

	return 0;
}

/**
 *  reset_fifo_itg() - Reset FIFO related registers.
 *  @st:	Device driver instance.
 */
static int reset_fifo_itg(struct iio_dev *indio_dev)
{
	struct inv_reg_map_s *reg;
	int result, data;
	u8 val, int_word;
	struct inv_mpu_iio_s  *st = iio_priv(indio_dev);
	reg = &st->reg;

	if (st->chip_config.lpa_mode) {
		result = inv_lpa_mode(st, 0);
		if (result) {
			pr_err("reset lpa mode failed\n");
			return result;
		}
	}
	/* disable interrupt */
	result = inv_i2c_single_write(st, reg->int_enable, 0);
	if (result) {
		pr_err("int_enable write failed\n");
		return result;
	}
	/* disable the sensor output to FIFO */
	result = inv_i2c_single_write(st, reg->fifo_en, 0);
	if (result)
		goto reset_fifo_fail;
	/* disable fifo reading */
	result = inv_i2c_single_write(st, reg->user_ctrl, 0);
	if (result)
		goto reset_fifo_fail;
	int_word = 0;

	/* MPU6500's BIT_6500_WOM_EN is the same as BIT_MOT_EN */
	if (st->mot_int.mot_on)
		int_word |= BIT_MOT_EN;

	if (st->chip_config.dmp_on) {
		val = (BIT_FIFO_RST | BIT_DMP_RST);
		result = inv_i2c_single_write(st, reg->user_ctrl, val);
		if (result)
			goto reset_fifo_fail;
		st->last_isr_time = get_time_ns();
		if (st->chip_config.dmp_int_on) {
			int_word |= BIT_DMP_INT_EN;
			result = inv_i2c_single_write(st, reg->int_enable,
							int_word);
			if (result)
				return result;
		}
		val = (BIT_DMP_EN | BIT_FIFO_EN);
		if (st->chip_config.compass_enable &
			(!st->chip_config.dmp_event_int_on))
			val |= BIT_I2C_MST_EN;
		result = inv_i2c_single_write(st, reg->user_ctrl, val);
		if (result)
			goto reset_fifo_fail;

		if (st->chip_config.compass_enable) {
			/* I2C_MST_DLY is set according to sample rate,
			   slow down the power*/
			data = max(COMPASS_RATE_SCALE *
				st->chip_config.fifo_rate / ONE_K_HZ,
				st->chip_config.fifo_rate /
				st->chip_config.dmp_output_rate);
			if (data > 0)
				data -= 1;
			result = inv_i2c_single_write(st, REG_I2C_SLV4_CTRL,
							data);
			if (result)
				return result;
		}
		val = 0;
		if (st->chip_config.accl_fifo_enable)
			val |= INV_ACCL_MASK;
		if (st->chip_config.gyro_fifo_enable)
			val |= INV_GYRO_MASK;
		result = inv_send_sensor_data(st, val);
		if (result)
			return result;
		if (st->chip_config.display_orient_on || st->chip_config.tap_on)
			result = inv_send_interrupt_word(st, true);
		else
			result = inv_send_interrupt_word(st, false);
	} else {
		/* reset FIFO and possibly reset I2C*/
		val = BIT_FIFO_RST;
		result = inv_i2c_single_write(st, reg->user_ctrl, val);
		if (result)
			goto reset_fifo_fail;
		st->last_isr_time = get_time_ns();
		/* enable interrupt */
		if (st->chip_config.accl_fifo_enable ||
		    st->chip_config.gyro_fifo_enable ||
		    st->chip_config.compass_enable) {
			int_word |= BIT_DATA_RDY_EN;
		}
		result = inv_i2c_single_write(st, reg->int_enable, int_word);
		if (result)
			return result;
		/* enable FIFO reading and I2C master interface*/
		val = BIT_FIFO_EN;
		if (st->chip_config.compass_enable)
			val |= BIT_I2C_MST_EN;
		result = inv_i2c_single_write(st, reg->user_ctrl, val);
		if (result)
			goto reset_fifo_fail;
		if (st->chip_config.compass_enable) {
			/* I2C_MST_DLY is set according to sample rate,
			   slow down the power*/
			data = COMPASS_RATE_SCALE *
				st->chip_config.fifo_rate / ONE_K_HZ;
			if (data > 0)
				data -= 1;
			result = inv_i2c_single_write(st, REG_I2C_SLV4_CTRL,
							data);
			if (result)
				return result;
		}
		/* enable sensor output to FIFO */
		val = 0;
		if (st->chip_config.gyro_fifo_enable)
			val |= BITS_GYRO_OUT;
		if (st->chip_config.accl_fifo_enable)
			val |= BIT_ACCEL_OUT;
		result = inv_i2c_single_write(st, reg->fifo_en, val);
		if (result)
			goto reset_fifo_fail;
	}
	st->chip_config.normal_compass_measure = 0;
	result = inv_lpa_mode(st, st->chip_config.lpa_mode);
	if (result)
		goto reset_fifo_fail;

	return 0;

reset_fifo_fail:
	if (st->chip_config.dmp_on)
		val = BIT_DMP_INT_EN;
	else
		val = BIT_DATA_RDY_EN;
	inv_i2c_single_write(st, reg->int_enable, val);
	pr_err("reset fifo failed\n");

	return result;
}

/**
 *  inv_clear_kfifo() - clear time stamp fifo
 *  @st:	Device driver instance.
 */
static void inv_clear_kfifo(struct inv_mpu_iio_s *st)
{
	unsigned long flags;

	spin_lock_irqsave(&st->time_stamp_lock, flags);
	kfifo_reset(&st->timestamps);
	spin_unlock_irqrestore(&st->time_stamp_lock, flags);
}

/**
 *  inv_reset_fifo() - Reset FIFO related registers.
 *  @st:	Device driver instance.
 */
static int inv_reset_fifo(struct iio_dev *indio_dev)
{
	struct inv_mpu_iio_s *st = iio_priv(indio_dev);

	inv_clear_kfifo(st);
	if (INV_MPU3050 == st->chip_type)
		return reset_fifo_mpu3050(indio_dev);
	else
		return reset_fifo_itg(indio_dev);
}

/**
 *  set_inv_enable() - Reset FIFO related registers.
 *			This also powers on the chip if needed.
 *  @st:	Device driver instance.
 *  @fifo_enable: enable/disable
 */
int set_inv_enable(struct iio_dev *indio_dev,
			bool enable) {
	struct inv_mpu_iio_s *st = iio_priv(indio_dev);
	struct inv_reg_map_s *reg;
	int result;

	if (st->chip_config.is_asleep)
		return -EINVAL;
	reg = &st->reg;
	if (enable) {
		result = inv_reset_fifo(indio_dev);
		if (result)
			return result;
	} else {
		if ((INV_MPU3050 != st->chip_type)
			&& st->chip_config.lpa_mode) {
			result = inv_lpa_mode(st, 0);
			if (result)
				return result;
		}
		result = inv_i2c_single_write(st, reg->fifo_en, 0);
		if (result)
			return result;
		/* disable fifo reading */
		if (INV_MPU3050 != st->chip_type) {
			result = inv_i2c_single_write(st, reg->int_enable, 0);
			if (result)
				return result;
			result = inv_i2c_single_write(st, reg->user_ctrl, 0);
		} else {
			result = inv_i2c_single_write(st, reg->int_enable,
				st->plat_data.int_config);
		}
		if (result)
			return result;
	}
	st->chip_config.enable = !!enable;

	return 0;
}

/**
 *  inv_irq_handler() - Cache a timestamp at each data ready interrupt.
 */
static irqreturn_t inv_irq_handler(int irq, void *dev_id)
{
	struct inv_mpu_iio_s *st;
	u64 timestamp;
	int catch_up;
	u64 time_since_last_irq;

	st = (struct inv_mpu_iio_s *)dev_id;
	timestamp = get_time_ns();
	time_since_last_irq = timestamp - st->last_isr_time;
	spin_lock(&st->time_stamp_lock);
	catch_up = 0;
	while ((time_since_last_irq > st->irq_dur_ns * 2) &&
	       (catch_up < MAX_CATCH_UP) &&
	       (!st->chip_config.lpa_mode) &&
	       (!st->chip_config.dmp_on)) {
		st->last_isr_time += st->irq_dur_ns;
		kfifo_in(&st->timestamps,
			 &st->last_isr_time, 1);
		time_since_last_irq = timestamp - st->last_isr_time;
		catch_up++;
	}
	kfifo_in(&st->timestamps, &timestamp, 1);
	st->last_isr_time = timestamp;
	spin_unlock(&st->time_stamp_lock);

	return IRQ_WAKE_THREAD;
}

static int put_scan_to_buf(struct iio_dev *indio_dev, u8 *d,
				short *s, int scan_index, int d_ind) {
	struct iio_buffer *ring = indio_dev->buffer;
	int st;
	int i;
	for (i = 0; i < 3; i++) {
		st = iio_scan_mask_query(indio_dev, ring, scan_index + i);
		if (st) {
			memcpy(&d[d_ind], &s[i], sizeof(s[i]));
			d_ind += sizeof(s[i]);
		}
	}
	return d_ind;
}

static int put_scan_to_buf_q(struct iio_dev *indio_dev, u8 *d,
				int *s, int scan_index, int d_ind) {
	struct iio_buffer *ring = indio_dev->buffer;
	int st;
	int i;
	for (i = 0; i < 4; i++) {
		st = iio_scan_mask_query(indio_dev, ring, scan_index + i);
		if (st) {
			memcpy(&d[d_ind], &s[i], sizeof(s[i]));
			d_ind += sizeof(s[i]);
		}
	}
	return d_ind;
}

static void inv_report_data_3050(struct iio_dev *indio_dev, s64 t,
			int has_footer, u8 *data)
{
	struct inv_mpu_iio_s *st = iio_priv(indio_dev);
	struct iio_buffer *ring = indio_dev->buffer;
	int ind, i, d_ind;
	struct inv_chip_config_s *conf;
	short g[THREE_AXIS], a[THREE_AXIS];
	s64 buf[8];
	u8 *tmp;
	int bytes_per_datum, scan_count;
	conf = &st->chip_config;

	scan_count = bitmap_weight(indio_dev->active_scan_mask,
				       indio_dev->masklength);
	bytes_per_datum = scan_count * 2;

	ind = 0;
	if (has_footer)
		ind += 2;
	tmp = (u8 *)buf;
	d_ind = 0;

	if (conf->gyro_fifo_enable) {
		for (i = 0; i < ARRAY_SIZE(g); i++) {
			g[i] = be16_to_cpup((__be16 *)(&data[ind + i * 2]));
			st->raw_gyro[i] = g[i];
		}
		ind += BYTES_PER_SENSOR;
		d_ind = put_scan_to_buf(indio_dev, tmp, g,
			INV_MPU_SCAN_GYRO_X, d_ind);
	}
	if (conf->accl_fifo_enable) {
		st->mpu_slave->combine_data(&data[ind], a);
		for (i = 0; i < ARRAY_SIZE(a); i++)
			st->raw_accel[i] = a[i];

		ind += BYTES_PER_SENSOR;
		d_ind = put_scan_to_buf(indio_dev, tmp, a,
			INV_MPU_SCAN_ACCL_X, d_ind);
	}

	i = (bytes_per_datum + 7) / 8;
	if (ring->scan_timestamp)
		buf[i] = t;
	ring->access->store_to(indio_dev->buffer, (u8 *)buf);
}

/**
 *  inv_read_fifo_mpu3050() - Transfer data from FIFO to ring buffer for
 *                            mpu3050.
 */
irqreturn_t inv_read_fifo_mpu3050(int irq, void *dev_id)
{

	struct inv_mpu_iio_s *st = (struct inv_mpu_iio_s *)dev_id;
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	int bytes_per_datum;
	u8 data[64];
	int result;
	short fifo_count, byte_read;
	u32 copied;
	s64 timestamp;
	struct inv_reg_map_s *reg;
	reg = &st->reg;
	/* It is impossible that chip is asleep or enable is
	zero when interrupt is on
	*  because interrupt is now connected with enable */
	if (st->chip_config.dmp_on)
		bytes_per_datum = BYTES_FOR_DMP;
	else
		bytes_per_datum = (st->chip_config.accl_fifo_enable +
			st->chip_config.gyro_fifo_enable)*BYTES_PER_SENSOR;
	if (st->chip_config.has_footer)
		byte_read = bytes_per_datum + MPU3050_FOOTER_SIZE;
	else
		byte_read = bytes_per_datum;

	fifo_count = 0;
	if (byte_read != 0) {
		result = inv_i2c_read(st, reg->fifo_count_h,
				FIFO_COUNT_BYTE, data);
		if (result)
			goto end_session;
		fifo_count = be16_to_cpup((__be16 *)(&data[0]));
		if (fifo_count < byte_read)
			goto end_session;
		if (fifo_count & 1)
			goto flush_fifo;
		if (fifo_count > FIFO_THRESHOLD)
			goto flush_fifo;
		/* Timestamp mismatch. */
		if (kfifo_len(&st->timestamps) <
			fifo_count / byte_read)
			goto flush_fifo;
		if (kfifo_len(&st->timestamps) >
			fifo_count / byte_read + TIME_STAMP_TOR) {
			if (st->chip_config.dmp_on) {
				result = kfifo_to_user(&st->timestamps,
				&timestamp, sizeof(timestamp), &copied);
				if (result)
					goto flush_fifo;
			} else {
				goto flush_fifo;
			}
		}
	}
	while ((bytes_per_datum != 0) && (fifo_count >= byte_read)) {
		result = inv_i2c_read(st, reg->fifo_r_w, byte_read, data);
		if (result)
			goto flush_fifo;

		result = kfifo_to_user(&st->timestamps,
			&timestamp, sizeof(timestamp), &copied);
		if (result)
			goto flush_fifo;
		inv_report_data_3050(indio_dev, timestamp,
				     st->chip_config.has_footer, data);
		fifo_count -= byte_read;
		if (st->chip_config.has_footer == 0) {
			st->chip_config.has_footer = 1;
			byte_read = bytes_per_datum + MPU3050_FOOTER_SIZE;
		}
	}

end_session:
	return IRQ_HANDLED;

flush_fifo:
	/* Flush HW and SW FIFOs. */
	inv_reset_fifo(indio_dev);
	inv_clear_kfifo(st);
	return IRQ_HANDLED;
}

static int inv_report_gyro_accl_compass(struct iio_dev *indio_dev,
					u8 *data, s64 t)
{
	struct inv_mpu_iio_s *st = iio_priv(indio_dev);
	struct iio_buffer *ring = indio_dev->buffer;
	short g[THREE_AXIS], a[THREE_AXIS], c[THREE_AXIS];
	int q[4];
	int result, ind, d_ind;
	s64 buf[8];
	u32 word;
	u8 d[8], compass_divider;
	u8 *tmp;
	int source, i;
	struct inv_chip_config_s *conf;

	conf = &st->chip_config;
	ind = 0;
	if (conf->quaternion_on & conf->dmp_on) {
		for (i = 0; i < ARRAY_SIZE(q); i++) {
			q[i] = be32_to_cpup((__be32 *)(&data[ind + i * 4]));
			st->raw_quaternion[i] = q[i];
		}
		ind += QUATERNION_BYTES;
	}
	if (conf->accl_fifo_enable) {
		for (i = 0; i < ARRAY_SIZE(a); i++) {
			a[i] = be16_to_cpup((__be16 *)(&data[ind + i * 2]));
			a[i] *= st->chip_info.multi;
			st->raw_accel[i] = a[i];
		}
		ind += BYTES_PER_SENSOR;
	}
	if (conf->gyro_fifo_enable) {
		for (i = 0; i < ARRAY_SIZE(g); i++) {
			g[i] = be16_to_cpup((__be16 *)(&data[ind + i * 2]));
			st->raw_gyro[i] = g[i];
		}
		ind += BYTES_PER_SENSOR;
	}
	if (conf->dmp_on && (conf->tap_on || conf->display_orient_on)) {
		word = (u32)(be32_to_cpup((u32 *)&data[ind]));
		source = ((word >> 16) & 0xff);
		if (source) {
			st->tap_data = (DMP_MASK_TAP & (word & 0xff));
			st->display_orient_data =
			((DMP_MASK_DIS_ORIEN & (word & 0xff)) >>
			  DMP_DIS_ORIEN_SHIFT);
		}

		/* report tap information */
		if (source & INT_SRC_TAP)
			sysfs_notify(&indio_dev->dev.kobj, NULL, "event_tap");
		/* report orientation information */
		if (source & INT_SRC_DISPLAY_ORIENT)
			sysfs_notify(&indio_dev->dev.kobj, NULL,
				     "event_display_orientation");
	}
	/*divider and counter is used to decrease the speed of read in
		high frequency sample rate*/
	if (conf->compass_fifo_enable) {
		c[0] = 0;
		c[1] = 0;
		c[2] = 0;
		if (conf->dmp_on)
			compass_divider = st->compass_dmp_divider;
		else
			compass_divider = st->compass_divider;
		if (compass_divider == st->compass_counter) {
			/*read from external sensor data register */
			result = inv_i2c_read(st, REG_EXT_SENS_DATA_00,
					      NUM_BYTES_COMPASS_SLAVE, d);
			/* d[7] is status 2 register */
			/*for AKM8975, bit 2 and 3 should be all be zero*/
			/* for AMK8963, bit 3 should be zero*/
			if ((DATA_AKM_DRDY == d[0]) &&
			    (0 == (d[7] & DATA_AKM_STAT_MASK)) &&
			    (!result)) {
				u8 *sens;
				sens = st->chip_info.compass_sens;
				c[0] = (short)((d[2] << 8) | d[1]);
				c[1] = (short)((d[4] << 8) | d[3]);
				c[2] = (short)((d[6] << 8) | d[5]);
				c[0] = (short)(((int)c[0] *
					       (sens[0] + 128)) >> 8);
				c[1] = (short)(((int)c[1] *
					       (sens[1] + 128)) >> 8);
				c[2] = (short)(((int)c[2] *
					       (sens[2] + 128)) >> 8);
				st->raw_compass[0] = c[0];
				st->raw_compass[1] = c[1];
				st->raw_compass[2] = c[2];
			}
			st->compass_counter = 0;
		} else if (compass_divider != 0) {
			st->compass_counter++;
		}
		if (!conf->normal_compass_measure) {
			c[0] = 0;
			c[1] = 0;
			c[2] = 0;
			conf->normal_compass_measure = 1;
		}
	}

	tmp = (u8 *)buf;
	d_ind = 0;
	if (conf->quaternion_on & conf->dmp_on)
		d_ind = put_scan_to_buf_q(indio_dev, tmp, q,
				INV_MPU_SCAN_QUAT_R, d_ind);
	if (conf->gyro_fifo_enable)
		d_ind = put_scan_to_buf(indio_dev, tmp, g,
				INV_MPU_SCAN_GYRO_X, d_ind);
	if (conf->accl_fifo_enable)
		d_ind = put_scan_to_buf(indio_dev, tmp, a,
				INV_MPU_SCAN_ACCL_X, d_ind);
	if (conf->compass_fifo_enable)
		d_ind = put_scan_to_buf(indio_dev, tmp, c,
				INV_MPU_SCAN_MAGN_X, d_ind);
	if (ring->scan_timestamp)
		buf[(d_ind + 7) / 8] = t;
	/* if nothing is enabled, send nothing */
	if (d_ind)
		ring->access->store_to(indio_dev->buffer, (u8 *)buf);

	return 0;
}

static void inv_process_motion(struct inv_mpu_iio_s *st)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	s32 diff, true_motion;
	s64 timestamp;
	int result;
	u8 data[1];

	/* motion interrupt */
	result = inv_i2c_read(st, REG_INT_STATUS, 1, data);
	if (result)
		return;

	if (data[0] & BIT_MOT_INT) {
		timestamp = get_time_ns();
		diff = (int)(((timestamp - st->mpu6500_last_motion_time) >>
			NS_PER_MS_SHIFT));
		if (diff > st->mot_int.mot_dur) {
			st->mpu6500_last_motion_time = timestamp;
			true_motion = 1;
		} else {
			true_motion = 0;
		}
		if (true_motion)
			sysfs_notify(&indio_dev->dev.kobj, NULL,
					"event_accel_motion");
	}
}

static int get_bytes_per_datum(struct inv_mpu_iio_s *st)
{
	int bytes_per_datum;

	bytes_per_datum = 0;
	if (st->chip_config.dmp_on) {
		if (st->chip_config.quaternion_on)
			bytes_per_datum += QUATERNION_BYTES;
		if (st->chip_config.tap_on ||
			st->chip_config.display_orient_on)
			bytes_per_datum += BYTES_FOR_EVENTS;
	}
	if (st->chip_config.accl_fifo_enable)
		bytes_per_datum += BYTES_PER_SENSOR;
	if (st->chip_config.gyro_fifo_enable)
		bytes_per_datum += BYTES_PER_SENSOR;

	return bytes_per_datum;
}

/**
 *  inv_read_fifo() - Transfer data from FIFO to ring buffer.
 */
irqreturn_t inv_read_fifo(int irq, void *dev_id)
{

	struct inv_mpu_iio_s *st = (struct inv_mpu_iio_s *)dev_id;
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	size_t bytes_per_datum;
	int result;
	u8 data[BYTES_FOR_DMP + QUATERNION_BYTES];
	u16 fifo_count;
	u32 copied;
	s64 timestamp;
	struct inv_reg_map_s *reg;
	s64 buf[8];
	s8 *tmp;

	mutex_lock(&indio_dev->mlock);
	if (!(iio_buffer_enabled(indio_dev)))
		goto end_session;

	reg = &st->reg;
	if (!(st->chip_config.accl_fifo_enable |
		st->chip_config.gyro_fifo_enable |
		st->chip_config.dmp_on |
		st->chip_config.compass_fifo_enable |
		st->mot_int.mot_on))
		goto end_session;
	if (st->mot_int.mot_on)
		inv_process_motion(st);
	if (st->chip_config.lpa_mode) {
		result = inv_i2c_read(st, reg->raw_accl,
				      BYTES_PER_SENSOR, data);
		if (result)
			goto end_session;
		inv_report_gyro_accl_compass(indio_dev, data,
					     get_time_ns());
		goto end_session;
	}
	bytes_per_datum = get_bytes_per_datum(st);
	fifo_count = 0;
	if (bytes_per_datum != 0) {
		result = inv_i2c_read(st, reg->fifo_count_h,
				FIFO_COUNT_BYTE, data);
		if (result)
			goto end_session;
		fifo_count = be16_to_cpup((__be16 *)(&data[0]));
		if (fifo_count < bytes_per_datum)
			goto end_session;
		/* fifo count can't be odd number */
		if (fifo_count & 1)
			goto flush_fifo;
		if (fifo_count >  FIFO_THRESHOLD)
			goto flush_fifo;
		/* timestamp mismatch. */
		if (kfifo_len(&st->timestamps) <
			fifo_count / bytes_per_datum)
			goto flush_fifo;
		if (kfifo_len(&st->timestamps) >
			fifo_count / bytes_per_datum + TIME_STAMP_TOR) {
			if (st->chip_config.dmp_on) {
				result = kfifo_to_user(&st->timestamps,
				&timestamp, sizeof(timestamp), &copied);
				if (result)
					goto flush_fifo;
			} else {
				goto flush_fifo;
			}
		}
	} else {
		result = kfifo_to_user(&st->timestamps,
			&timestamp, sizeof(timestamp), &copied);
		if (result)
			goto flush_fifo;
	}
	tmp = (s8 *)buf;

	while ((bytes_per_datum != 0) && (fifo_count >= bytes_per_datum)) {
		/* Ignoring the coverity issue: bytest_per_datum can range from
		 * 1 to 499. Since bytes_per_datum does not go above 32 which
		 * is coming from get_bytes_per_datum() function
		 */
		// coverity[overrun-buffer-arg]
		result = inv_i2c_read(st, reg->fifo_r_w, bytes_per_datum,
			data);
		if (result)
			goto flush_fifo;

		result = kfifo_to_user(&st->timestamps,
			&timestamp, sizeof(timestamp), &copied);
		if (result)
			goto flush_fifo;
		inv_report_gyro_accl_compass(indio_dev, data, timestamp);
		fifo_count -= bytes_per_datum;
	}
	if (bytes_per_datum == 0 && st->chip_config.compass_fifo_enable)
		inv_report_gyro_accl_compass(indio_dev, data, timestamp);

end_session:
	mutex_unlock(&indio_dev->mlock);

	return IRQ_HANDLED;

flush_fifo:
	/* Flush HW and SW FIFOs. */
	inv_reset_fifo(indio_dev);
	inv_clear_kfifo(st);
	mutex_unlock(&indio_dev->mlock);

	return IRQ_HANDLED;
}

void inv_mpu_unconfigure_ring(struct iio_dev *indio_dev)
{
	struct inv_mpu_iio_s *st = iio_priv(indio_dev);
	free_irq(st->client->irq, st);
	iio_kfifo_free(indio_dev->buffer);
};

static int inv_postenable(struct iio_dev *indio_dev)
{
	return set_inv_enable(indio_dev, true);
}

static int inv_predisable(struct iio_dev *indio_dev)
{
	return set_inv_enable(indio_dev, false);
}

static void inv_scan_query(struct iio_dev *indio_dev)
{
	struct inv_mpu_iio_s  *st = iio_priv(indio_dev);
	struct iio_buffer *ring = indio_dev->buffer;
	int result;

	if (iio_scan_mask_query(indio_dev, ring, INV_MPU_SCAN_GYRO_X) ||
	    iio_scan_mask_query(indio_dev, ring, INV_MPU_SCAN_GYRO_Y) ||
	    iio_scan_mask_query(indio_dev, ring, INV_MPU_SCAN_GYRO_Z))
		st->chip_config.gyro_fifo_enable = 1;
	else
		st->chip_config.gyro_fifo_enable = 0;

	if (iio_scan_mask_query(indio_dev, ring, INV_MPU_SCAN_ACCL_X) ||
	    iio_scan_mask_query(indio_dev, ring, INV_MPU_SCAN_ACCL_Y) ||
	    iio_scan_mask_query(indio_dev, ring, INV_MPU_SCAN_ACCL_Z))
		st->chip_config.accl_fifo_enable = 1;
	else
		st->chip_config.accl_fifo_enable = 0;

	if (iio_scan_mask_query(indio_dev, ring, INV_MPU_SCAN_MAGN_X) ||
	    iio_scan_mask_query(indio_dev, ring, INV_MPU_SCAN_MAGN_Y) ||
	    iio_scan_mask_query(indio_dev, ring, INV_MPU_SCAN_MAGN_Z))
		st->chip_config.compass_fifo_enable = 1;
	else
		st->chip_config.compass_fifo_enable = 0;

	/* check to make sure engine is turned on if fifo is turned on */
	if (st->chip_config.gyro_fifo_enable &&
		(!st->chip_config.gyro_enable)) {
		result = st->switch_gyro_engine(st, true);
		if (result)
			return;
		st->chip_config.gyro_enable = true;
	}
	if (st->chip_config.accl_fifo_enable &&
		(!st->chip_config.accl_enable)) {
		result = st->switch_accl_engine(st, true);
		if (result)
			return;
		st->chip_config.accl_enable = true;
	}
}

static int inv_check_quaternion(struct iio_dev *indio_dev)
{
	struct inv_mpu_iio_s  *st = iio_priv(indio_dev);
	struct iio_buffer *ring = indio_dev->buffer;
	int result;

	if (st->chip_config.dmp_on) {
		if (
		  iio_scan_mask_query(indio_dev, ring, INV_MPU_SCAN_QUAT_R) ||
		  iio_scan_mask_query(indio_dev, ring, INV_MPU_SCAN_QUAT_X) ||
		  iio_scan_mask_query(indio_dev, ring, INV_MPU_SCAN_QUAT_Y) ||
		  iio_scan_mask_query(indio_dev, ring, INV_MPU_SCAN_QUAT_Z))
			st->chip_config.quaternion_on = 1;
		else
			st->chip_config.quaternion_on = 0;

		result = inv_send_quaternion(st,
				st->chip_config.quaternion_on);
		if (result)
			return result;
	} else {
		st->chip_config.quaternion_on = 0;
		clear_bit(INV_MPU_SCAN_QUAT_R, ring->scan_mask);
		clear_bit(INV_MPU_SCAN_QUAT_X, ring->scan_mask);
		clear_bit(INV_MPU_SCAN_QUAT_Y, ring->scan_mask);
		clear_bit(INV_MPU_SCAN_QUAT_Z, ring->scan_mask);
	}

	return 0;
}

static int inv_check_conflict_sysfs(struct iio_dev *indio_dev)
{
	struct inv_mpu_iio_s  *st = iio_priv(indio_dev);
	struct iio_buffer *ring = indio_dev->buffer;
	int result;

	if (st->chip_config.lpa_mode) {
		/* dmp cannot run with low power mode on */
		st->chip_config.dmp_on = 0;
		result = st->gyro_en(st, ring, false);
		if (result)
			return result;
		result = st->compass_en(st, ring, false);
		if (result)
			return result;
		result = st->quaternion_en(st, ring, false);
		if (result)
			return result;

		result = st->accl_en(st, ring, true);
		if (result)
			return result;
	}
	result = inv_check_quaternion(indio_dev);
	if (result)
		return result;

	return result;
}

static int inv_preenable(struct iio_dev *indio_dev)
{
	int result;

	result = inv_check_conflict_sysfs(indio_dev);
	if (result)
		return result;
	inv_scan_query(indio_dev);
	result = iio_sw_buffer_preenable(indio_dev);

	return result;
}

static const struct iio_buffer_setup_ops inv_mpu_ring_setup_ops = {
	.preenable = &inv_preenable,
	.postenable = &inv_postenable,
	.predisable = &inv_predisable,
};

int inv_mpu_configure_ring(struct iio_dev *indio_dev)
{
	int ret;
	struct inv_mpu_iio_s *st = iio_priv(indio_dev);
	struct iio_buffer *ring;

	ring = iio_kfifo_allocate(indio_dev);
	if (!ring)
		return -ENOMEM;
	indio_dev->buffer = ring;
	/* setup ring buffer */
	ring->scan_timestamp = true;
	indio_dev->setup_ops = &inv_mpu_ring_setup_ops;
	/*scan count double count timestamp. should subtract 1. but
	number of channels still includes timestamp*/
	if (INV_MPU3050 == st->chip_type)
		ret = request_threaded_irq(st->client->irq, inv_irq_handler,
			inv_read_fifo_mpu3050,
			IRQF_TRIGGER_RISING | IRQF_SHARED, "inv_irq", st);
	else
		ret = request_threaded_irq(st->client->irq, inv_irq_handler,
			inv_read_fifo,
			IRQF_TRIGGER_RISING | IRQF_SHARED, "inv_irq", st);
	if (ret)
		goto error_iio_sw_rb_free;

	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;
	return 0;
error_iio_sw_rb_free:
	iio_kfifo_free(indio_dev->buffer);
	return ret;
}

/**
 *  @}
 */

