/*****************************************************************************
*  Copyright 2001 - 2012 Broadcom Corporation.  All rights reserved.
*
*  Unless you and Broadcom execute a separate written software license
*  agreement governing use of this software, this software is licensed to you
*  under the terms of the GNU General Public License version 2, available at
*  http://www.gnu.org/licenses/old-license/gpl-2.0.html (the "GPL").
*
*  Notwithstanding the above, under no circumstances may you combine this
*  software in any way with any other Broadcom software provided under a
*  license other than the GPL, without Broadcom's express prior written
*  consent.
*
*****************************************************************************/
#include <linux/version.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/export.h>
#include <linux/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <mach/hardware.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/bcmpmu59xxx.h>
#include <linux/mfd/bcmpmu59xxx_reg.h>
#include <linux/power/bcmpmu-fg.h>
#include <linux/broadcom/bcmpmu-ponkey.h>
#include <mach/rdb/brcm_rdb_include.h>
#ifdef CONFIG_CHARGER_BCMPMU_SPA
#include <linux/spa_power.h>
#endif
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#ifdef CONFIG_KONA_AVS
#include <mach/avs.h>
#endif
#include "pm_params.h"
#include "volt_tbl.h"

#define PMU_DEVICE_I2C_ADDR	0x08
#define PMU_DEVICE_I2C_ADDR1	0x0c
#define PMU_DEVICE_INT_GPIO	29
#define PMU_DEVICE_I2C_BUSNO 4

#define PMU_SR_VOLTAGE_MASK	0x3F
#define PMU_SR_VOLTAGE_SHIFT 0

#define PMU_BATT_CUTOFF_EXTENDED

static int bcmpmu_init_platform_hw(struct bcmpmu59xxx *bcmpmu);
static int bcmpmu_exit_platform_hw(struct bcmpmu59xxx *bcmpmu);

/* Used only when no bcmpmu dts entry found */
static struct bcmpmu59xxx_rw_data __initdata register_init_data[] = {
/* mask 0x00 is invalid value for mask */
	/* pin mux selection for pc3 and simldo1
	 * AUXONb Wakeup enabled */
	{.addr = PMU_REG_GPIOCTRL1, .val = 0xF5, .mask = 0xFF},
	/*  enable PC3 function */
	{.addr = PMU_REG_GPIOCTRL2, .val = 0x0E, .mask = 0xFF},
	/* Selecting 0.87V */
	{.addr = PMU_REG_MMSRVOUT1, .val = 0x30, .mask = 0xFF},
	/* Mask Interrupt */
	{.addr = PMU_REG_INT1MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT2MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT3MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT4MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT5MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT6MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT7MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT8MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT9MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT10MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT11MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT12MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT13MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT14MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT15MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT16MSK, .val = 0xFF, .mask = 0xFF},
	/* Trickle charging timer setting */
	{.addr = PMU_REG_MBCCTRL1, .val = 0x38, .mask = 0x38},
	/*  disable software charger timer */
	{.addr = PMU_REG_MBCCTRL2, .val = 0x0, .mask = 0x04},
	/* SWUP */
	{.addr = PMU_REG_MBCCTRL3, .val = 0x04, .mask = 0x04},

	/*  ICCMAX to 1500mA*/
	{.addr = PMU_REG_MBCCTRL8, .val = 0x0B, .mask = 0xFF},

	/*  TCXCTRL*/
	{.addr = PMU_REG_TCXLDOCTRL, .val = 0xB8, .mask = 0xFF},

	/* set USBOV to 8V */
	{.addr = PMU_REG_CMPCTRL4, .val = 0x21, .mask = 0xFF},
	/* NTC Hot Temperature Comparator*/
	{.addr = PMU_REG_CMPCTRL5, .val = 0x01, .mask = 0xFF},
	/* NTC Hot Temperature Comparator*/
	{.addr = PMU_REG_CMPCTRL6, .val = 0x05, .mask = 0xFF},
	/* NTC Cold Temperature Comparator */
	{.addr = PMU_REG_CMPCTRL7, .val = 0xFF, .mask = 0xFF},
	/* NTC Cold Temperature Comparator */
	{.addr = PMU_REG_CMPCTRL8, .val = 0xFA, .mask = 0xFF},
	/* NTC Hot Temperature Comparator bit 9,8 */
	{.addr = PMU_REG_CMPCTRL9, .val = 0x0F, .mask = 0xFF},

	/*  USB_CC_TRM to keep it down to hard 700mA chrg curr */
	{.addr = PMU_REG_MBCCTRL18, .val = 0x1F, .mask = 0xFF},


	/* ID detection method selection
	 *  current source Trimming */
	/* ID_METHOD=0 prevent leackage from ID detection */
	{.addr = PMU_REG_OTGCTRL8, .val = 0x52, .mask = 0xFF},
	{.addr = PMU_REG_OTGCTRL9, .val = 0x98, .mask = 0xFF},
	{.addr = PMU_REG_OTGCTRL10, .val = 0xF0, .mask = 0xFF},
	/*ADP_THR_RATIO*/
	{.addr = PMU_REG_OTGCTRL11, .val = 0x58, .mask = 0xFF},
	/* Enable ADP_PRB  ADP_DSCHG comparators */
	{.addr = PMU_REG_OTGCTRL12, .val = 0xC3, .mask = 0xFF},

/* Regulator configuration */
/* TODO regulator */
	{.addr = PMU_REG_FG_EOC_TH, .val = 0x64, .mask = 0xFF},

	/* vivaltonfc rev00 Xtal RTC = 18pF shows 32768.12Hz */
	{.addr = PMU_REG_RTC_C2C1_XOTRIM, .val = 0x66, .mask = 0xFF},
	{.addr = PMU_REG_FGOCICCTRL, .val = 0x02, .mask = 0xFF},
	 /* FG power down */
	{.addr = PMU_REG_FGCTRL1, .val = 0x00, .mask = 0xFF},
	/* Enable operation mode for PC3PC2PC1 */
	{.addr = PMU_REG_GPLDO2PMCTRL2, .val = 0x38, .mask = 0xFF},
	 /* PWMLED blovk powerdown */
	{.addr =  PMU_REG_PWMLEDCTRL1, .val = 0x23, .mask = 0xFF},
	{.addr = PMU_REG_HSCP3, .val = 0x00, .mask = 0xFF},
	 /* HS audio powerdown feedback path */
	{.addr =  PMU_REG_IHF_NGMISC, .val = 0x0C, .mask = 0xFF},
	/* NTC BiasSynchronous Mode,Host Enable Control NTC_PM0 Disable*/
	{.addr =  PMU_REG_CMPCTRL14, .val = 0x13, .mask = 0xFF},
	{.addr =  PMU_REG_CMPCTRL15, .val = 0x01, .mask = 0xFF},
	/* BSI Bias Host Control, Synchronous Mode Enable */

	{.addr =  PMU_REG_CMPCTRL16, .val = 0x13, .mask = 0xFF},
	/* BSI_EN_PM0 disable */
	{.addr =  PMU_REG_CMPCTRL17, .val = 0x01, .mask = 0xFF},
	/* Mask RTM conversion */
	{.addr =  PMU_REG_ADCCTRL1, .val = 0x08, .mask = 0x08},
#ifdef CONFIG_MACH_HAWAII_SS_COMMON
	/* EN_SESS_VALID disable ID detection */
	{.addr = PMU_REG_OTGCTRL1 , .val = 0x10, .mask = 0xFF},
#else
	/* EN_SESS_VALID  enable ID detection */
	{.addr = PMU_REG_OTGCTRL1 , .val = 0x18, .mask = 0xFF},
#endif
#ifdef CONFIG_INCREASE_CURRENT_IN_BOOT
		/* MBC_TURBO disable */
		{.addr =  PMU_REG_OTG_BOOSTCTRL3, .val = 0x00, .mask = 0x10},
#endif
	/* MMSR LPM voltage - 0.88V */
	{.addr = PMU_REG_MMSRVOUT2 , .val = 0x4, .mask = 0x3F},
	/* SDSR1 NM1 voltage - 1.24V */
	{.addr = PMU_REG_SDSR1VOUT1 , .val = 0x28, .mask = 0x3F},
	/* SDSR1 LPM voltage - 0.9V */
	{.addr = PMU_REG_SDSR1VOUT2 , .val = 0x6, .mask = 0x3F},
	/* SDSR2 NM1 voltage - 1.24 */
	{.addr = PMU_REG_SDSR2VOUT1 , .val = 0x28, .mask = 0x3F},
	/* SDSR2 LPM voltage - 1.24V */
	{.addr = PMU_REG_SDSR2VOUT2 , .val = 0x28, .mask = 0x3F},
	/* IOSR1 LPM voltage - 1.8V */
	{.addr = PMU_REG_IOSR1VOUT2 , .val = 0x3E, .mask = 0x3F},

	{.addr = PMU_REG_CSRVOUT1 , .val = 0x28, .mask = 0x3F},

	/*from h/w team for power consumption*/
	{.addr = PMU_REG_PASRCTRL1 , .val = 0x00, .mask = 0x06},
	{.addr = PMU_REG_PASRCTRL6 , .val = 0x00, .mask = 0xF0},
	{.addr = PMU_REG_PASRCTRL7 , .val = 0x00, .mask = 0x3F},
	{.addr = PMU_REG_FGCTRL1 , .val = 0x40, .mask = 0xFF},
	{.addr = PMU_REG_FGOPMODCTRL , .val = 0x01, .mask = 0xFF},
	{.addr = PMU_REG_FGOCICCTRL , .val = 0x04, .mask = 0xFF},
	{.addr = PMU_REG_CMPCTRL14 , .val = 0x12, .mask = 0xFF},
	{.addr = PMU_REG_CMPCTRL15 , .val = 0x0, .mask = 0xFF},
	{.addr = PMU_REG_CMPCTRL16 , .val = 0x12, .mask = 0xFF},
	{.addr = PMU_REG_CMPCTRL17 , .val = 0x00, .mask = 0xFF},
	{.addr = PMU_REG_PLLPMCTRL , .val = 0x00, .mask = 0xFF},

	/*RFLDO and AUDLDO pulldown disable MobC00290043*/
	{.addr = PMU_REG_RFLDOCTRL , .val = 0x40, .mask = 0x40},
	{.addr = PMU_REG_AUDLDOCTRL , .val = 0x40, .mask = 0x40},


#if defined(CONFIG_MACH_HAWAII_SS_LOGAN_REV00)

	/* enable PASR mode */
	{.addr = PMU_REG_GPIOCTRL3 , .val = 0x02, .mask = 0xFF},
	{.addr = PMU_REG_PASRCTRL1 , .val = 0x19, .mask = 0xFF},
	{.addr = PMU_REG_PASRCTRL2 , .val = 0x02, .mask = 0xFF},
#endif /* defined(CONFIG_MACH_HAWAII_SS_LOGAN_REV00 */

};

__weak struct regulator_consumer_supply rf_supply[] = {
	{.supply = "rf"},
};
static struct regulator_init_data bcm59xxx_rfldo_data = {
	.constraints = {
			.name = "rfldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS |
			REGULATOR_CHANGE_VOLTAGE,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_STANDBY,
			},
	.num_consumer_supplies = ARRAY_SIZE(rf_supply),
	.consumer_supplies = rf_supply,
};

__weak struct regulator_consumer_supply cam1_supply[] = {
	{.supply = "cam1"},
};
static struct regulator_init_data bcm59xxx_camldo1_data = {
	.constraints = {
			.name = "camldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE |
			REGULATOR_CHANGE_VOLTAGE,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(cam1_supply),
	.consumer_supplies = cam1_supply,
};

__weak struct regulator_consumer_supply cam2_supply[] = {
	{.supply = "cam2"},
};
static struct regulator_init_data bcm59xxx_camldo2_data = {
	.constraints = {
			.name = "camldo2",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE |
			REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			.boot_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(cam2_supply),
	.consumer_supplies = cam2_supply,
};

__weak struct regulator_consumer_supply sim1_supply[] = {
	{.supply = "sim_vcc"},
};
static struct regulator_init_data bcm59xxx_simldo1_data = {
	.constraints = {
			.name = "simldo1",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
				REGULATOR_CHANGE_STATUS |
				REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL |
				REGULATOR_MODE_IDLE | REGULATOR_MODE_STANDBY,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(sim1_supply),
	.consumer_supplies = sim1_supply,
};

__weak struct regulator_consumer_supply sim2_supply[] = {
	{.supply = "sim2_vcc"},
};
static struct regulator_init_data bcm59xxx_simldo2_data = {
	.constraints = {
			.name = "simldo2",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE |
			REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL |
				REGULATOR_MODE_IDLE | REGULATOR_MODE_STANDBY,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(sim2_supply),
	.consumer_supplies = sim2_supply,
};

__weak struct regulator_consumer_supply sd_supply[] = {
	{.supply = "sdlo_uc"},
	REGULATOR_SUPPLY("vddmmc", "sdhci.3"), /* 0x3f1b0000.sdhci */
	{.supply = "vdd_sdio"},
};
static struct regulator_init_data bcm59xxx_sdldo_data = {
	.constraints = {
			.name = "sdldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			.initial_mode = REGULATOR_MODE_NORMAL,
			},
	.num_consumer_supplies = ARRAY_SIZE(sd_supply),
	.consumer_supplies = sd_supply,
};
__weak struct regulator_consumer_supply sdx_supply[] = {
	{.supply = "sdxldo_uc"},
	REGULATOR_SUPPLY("vddo", "sdhci.3"), /* 0x3f1b0000.sdhci */
	{.supply = "vdd_sdxc"},
	{.supply = "sddat_debug_bus"},
};
static struct regulator_init_data bcm59xxx_sdxldo_data = {
	.constraints = {
			.name = "sdxldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(sdx_supply),
	.consumer_supplies = sdx_supply,
};

__weak struct regulator_consumer_supply mmc1_supply[] = {
	{.supply = "mmc1_vcc"},
};
static struct regulator_init_data bcm59xxx_mmcldo1_data = {
	.constraints = {
			.name = "mmcldo1",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE |
			REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(mmc1_supply),
	.consumer_supplies = mmc1_supply,
};

__weak struct regulator_consumer_supply mmc2_supply[] = {
	{.supply = "mmc2_vcc"},
};
static struct regulator_init_data bcm59xxx_mmcldo2_data = {
	.constraints = {
			.name = "mmcldo2",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE |
			REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(mmc2_supply),
	.consumer_supplies = mmc2_supply,
};

__weak struct regulator_consumer_supply aud_supply[] = {
	{.supply = "audldo_uc"},
};
static struct regulator_init_data bcm59xxx_audldo_data = {
	.constraints = {
			.name = "audldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS |
					REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL |
						REGULATOR_MODE_IDLE |
						REGULATOR_MODE_STANDBY,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_STANDBY,
			},
	.num_consumer_supplies = ARRAY_SIZE(aud_supply),
	.consumer_supplies = aud_supply,
};

__weak struct regulator_consumer_supply usb_supply[] = {
	{.supply = "usb_vcc"},
};
static struct regulator_init_data bcm59xxx_usbldo_data = {
	.constraints = {
			.name = "usbldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 1,
			},
	.num_consumer_supplies = ARRAY_SIZE(usb_supply),
	.consumer_supplies = usb_supply,
};

__weak struct regulator_consumer_supply mic_supply[] = {
	{.supply = "micldo_uc"},
};
static struct regulator_init_data bcm59xxx_micldo_data = {
	.constraints = {
			.name = "micldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(mic_supply),
	.consumer_supplies = mic_supply,
};


__weak struct regulator_consumer_supply vib_supply[] = {
	{.supply = "vibldo_uc"},
};
static struct regulator_init_data bcm59xxx_vibldo_data = {
	.constraints = {
			.name = "vibldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,

			},
	.num_consumer_supplies = ARRAY_SIZE(vib_supply),
	.consumer_supplies = vib_supply,
};

__weak struct regulator_consumer_supply gpldo1_supply[] = {
	{.supply = "gpldo1_uc"},
};
static struct regulator_init_data bcm59xxx_gpldo1_data = {
	.constraints = {
			.name = "gpldo1",
			.min_uV = 1200000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(gpldo1_supply),
	.consumer_supplies = gpldo1_supply,
};

__weak struct regulator_consumer_supply gpldo2_supply[] = {
	{.supply = "gpldo2_uc"},
};
static struct regulator_init_data bcm59xxx_gpldo2_data = {
	.constraints = {
			.name = "gpldo2",
			.min_uV = 1200000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE |
			REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(gpldo2_supply),
	.consumer_supplies = gpldo2_supply,
};

__weak struct regulator_consumer_supply gpldo3_supply[] = {
	{.supply = "gpldo3_uc"},
};
static struct regulator_init_data bcm59xxx_gpldo3_data = {
	.constraints = {
			.name = "gpldo3",
			.min_uV = 1200000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE |
			REGULATOR_CHANGE_MODE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(gpldo3_supply),
	.consumer_supplies = gpldo3_supply,
};

__weak struct regulator_consumer_supply tcxldo_supply[] = {
	{.supply = "tcxldo_uc"},
};
static struct regulator_init_data bcm59xxx_tcxldo_data = {
	.constraints = {
			.name = "tcxldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE |
			REGULATOR_CHANGE_VOLTAGE,
			.always_on = 1,
			},
	.num_consumer_supplies = ARRAY_SIZE(tcxldo_supply),
	.consumer_supplies = tcxldo_supply,
};

__weak struct regulator_consumer_supply lvldo1_supply[] = {
	{.supply = "lvldo1_uc"},
};
static struct regulator_init_data bcm59xxx_lvldo1_data = {
	.constraints = {
			/* CAM0_1v8 */
			.name = "lvldo1",
			.min_uV = 1000000,
			.max_uV = 1800000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE|
			REGULATOR_CHANGE_MODE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(lvldo1_supply),
	.consumer_supplies = lvldo1_supply,
};

__weak struct regulator_consumer_supply lvldo2_supply[] = {
	{.supply = "lvldo2_uc"},
};
static struct regulator_init_data bcm59xxx_lvldo2_data = {
	.constraints = {
			.name = "lvldo2",
			.min_uV = 1000000,
			.max_uV = 1800000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE |
			REGULATOR_CHANGE_MODE ,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(lvldo2_supply),
	.consumer_supplies = lvldo2_supply,
};

__weak struct regulator_consumer_supply vsr_supply[] = {
	{.supply = "vsr_uc"},
};
static struct regulator_init_data bcm59xxx_vsr_data = {
	.constraints = {
			.name = "vsrldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask =
			REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_VOLTAGE |
			REGULATOR_CHANGE_STATUS,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(vsr_supply),
	.consumer_supplies = vsr_supply,
};

__weak struct regulator_consumer_supply csr_supply[] = {
	{.supply = "csr_uc"},
};

static struct regulator_init_data bcm59xxx_csr_data = {
	.constraints = {
			.name = "csrldo",
			.min_uV = 700000,
			.max_uV = 1440000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_STANDBY,
			},
	.num_consumer_supplies = ARRAY_SIZE(csr_supply),
	.consumer_supplies = csr_supply,
};

__weak struct regulator_consumer_supply mmsr_supply[] = {
	{.supply = "mmsr_uc"},
};

static struct regulator_init_data bcm59xxx_mmsr_data = {
	.constraints = {
			.name = "mmsrldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(mmsr_supply),
	.consumer_supplies = mmsr_supply,
};

__weak struct regulator_consumer_supply sdsr1_supply[] = {
	{.supply = "sdsr1_uc"},
};

static struct regulator_init_data bcm59xxx_sdsr1_data = {
	.constraints = {
			.name = "sdsr1ldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(sdsr1_supply),
	.consumer_supplies = sdsr1_supply,
};

__weak struct regulator_consumer_supply sdsr2_supply[] = {
	{.supply = "sdsr2_uc"},
};

static struct regulator_init_data bcm59xxx_sdsr2_data = {
	.constraints = {
			.name = "sdsr2ldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(sdsr2_supply),
	.consumer_supplies = sdsr2_supply,
};

__weak struct regulator_consumer_supply iosr1_supply[] = {
	{.supply = "iosr1_uc"},
};

static struct regulator_init_data bcm59xxx_iosr1_data = {
	.constraints = {
			.name = "iosr1ldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(iosr1_supply),
	.consumer_supplies = iosr1_supply,
};


__weak struct regulator_consumer_supply iosr2_supply[] = {
	{.supply = "iosr2_uc"},
};

static struct regulator_init_data bcm59xxx_iosr2_data = {
	.constraints = {
			.name = "iosr2ldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(iosr2_supply),
	.consumer_supplies = iosr2_supply,
};


struct bcmpmu59xxx_regulator_init_data
	bcm59xxx_regulators[BCMPMU_REGULATOR_MAX] = {
		[BCMPMU_REGULATOR_RFLDO] = {
			.id = BCMPMU_REGULATOR_RFLDO,
			.initdata = &bcm59xxx_rfldo_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC2),
			.name = "rf",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_CAMLDO1] = {
			.id = BCMPMU_REGULATOR_CAMLDO1,
			.initdata = &bcm59xxx_camldo1_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC2|PMU_PC3),
			.name = "cam1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_CAMLDO2] = {
			.id = BCMPMU_REGULATOR_CAMLDO2,
			.initdata = &bcm59xxx_camldo2_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "cam2",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SIMLDO1] = {
			.id = BCMPMU_REGULATOR_SIMLDO1,
			.initdata = &bcm59xxx_simldo1_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC1),
			.name = "sim1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SIMLDO2] = {
			.id = BCMPMU_REGULATOR_SIMLDO2,
			.initdata = &bcm59xxx_simldo2_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1),
			.name = "sim2",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SDLDO] = {
			.id = BCMPMU_REGULATOR_SDLDO,
			.initdata = &bcm59xxx_sdldo_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "sd",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SDXLDO] = {
			.id = BCMPMU_REGULATOR_SDXLDO,
			.initdata = &bcm59xxx_sdxldo_data,
			.pc_pins_map =
				 PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "sdx",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_MMCLDO1] = {
			.id = BCMPMU_REGULATOR_MMCLDO1,
			.initdata = &bcm59xxx_mmcldo1_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "mmc1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_MMCLDO2] = {
			.id = BCMPMU_REGULATOR_MMCLDO2,
			.initdata = &bcm59xxx_mmcldo2_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "mmc2",
			.req_volt = 0,
		},

		[BCMPMU_REGULATOR_AUDLDO] = {
			.id = BCMPMU_REGULATOR_AUDLDO,
			.initdata = &bcm59xxx_audldo_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC2|PMU_PC3),
			.name = "aud",
			.req_volt = 0,
#if defined(CONFIG_MACH_HAWAII_SS_COMMON)
			.reg_value = 0x01,
			.reg_value2 = 0x05, /* 0x11 in Capri */
			.off_value = 0x02,
			.off_value2 = 0x0a, /* 0x22 in Capri */
#endif
		},

		[BCMPMU_REGULATOR_MICLDO] = {
			.id = BCMPMU_REGULATOR_MICLDO,
			.initdata = &bcm59xxx_micldo_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, 0),  /*Not used*/
			.name = "mic",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_USBLDO] = {
			.id = BCMPMU_REGULATOR_USBLDO,
			.initdata = &bcm59xxx_usbldo_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "usb",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_VIBLDO] = {
			.id = BCMPMU_REGULATOR_VIBLDO,
			.initdata = &bcm59xxx_vibldo_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "vib",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_GPLDO1] = {
			.id = BCMPMU_REGULATOR_GPLDO1,
			.initdata = &bcm59xxx_gpldo1_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, 0), /*Not used*/
			.name = "gp1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_GPLDO2] = {
			.id = BCMPMU_REGULATOR_GPLDO2,
			.initdata = &bcm59xxx_gpldo2_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "gp2",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_GPLDO3] = {
			.id = BCMPMU_REGULATOR_GPLDO3,
			.initdata = &bcm59xxx_gpldo3_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "gp3",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_TCXLDO] = {
			.id = BCMPMU_REGULATOR_TCXLDO,
			.initdata = &bcm59xxx_tcxldo_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, 0),
			.name = "tcx",
			.req_volt = 0,
		},
	#if defined(CONFIG_MACH_HAWAII_SS_VIVALTODS5M_REV00)
		[BCMPMU_REGULATOR_LVLDO1] = {
			.id = BCMPMU_REGULATOR_LVLDO1, /* CAM0_1V8 */
			.initdata = &bcm59xxx_lvldo1_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "lv1",
			.req_volt = 0,
		},
	#else
		[BCMPMU_REGULATOR_LVLDO1] = {
			.id = BCMPMU_REGULATOR_LVLDO1,
			.initdata = &bcm59xxx_lvldo1_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, 0), /*Not used*/
			.name = "lv1",
			.req_volt = 0,
		},
	#endif
		[BCMPMU_REGULATOR_LVLDO2] = {
			.id = BCMPMU_REGULATOR_LVLDO2,
			.initdata = &bcm59xxx_lvldo2_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "lv2",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_VSR] = {
			.id = BCMPMU_REGULATOR_VSR,
			.initdata = &bcm59xxx_vsr_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "vsr",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_CSR] = {
			.id = BCMPMU_REGULATOR_CSR,
			.initdata = &bcm59xxx_csr_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC3),
			.name = "csr",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_MMSR] = {
			.id = BCMPMU_REGULATOR_MMSR,
			.initdata = &bcm59xxx_mmsr_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC2),
			.name = "mmsr",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SDSR1] = {
			.id = BCMPMU_REGULATOR_SDSR1,
			.initdata = &bcm59xxx_sdsr1_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "sdsr1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SDSR2] = {
			.id = BCMPMU_REGULATOR_SDSR2,
			.initdata = &bcm59xxx_sdsr2_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC2|PMU_PC3),
			.name = "sdsr2",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_IOSR1] = {
			.id = BCMPMU_REGULATOR_IOSR1,
			.initdata = &bcm59xxx_iosr1_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "iosr1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_IOSR2] = {
			.id = BCMPMU_REGULATOR_IOSR2,
			.initdata = &bcm59xxx_iosr2_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, 0), /*not used*/
			.name = "iosr2",
			.req_volt = 0,
		},

	};

/*Ponkey platform data*/
struct pkey_timer_act pkey_t3_action = {
	.flags = PKEY_SMART_RST_PWR_EN,
	.action = PKEY_ACTION_SMART_RESET,
	.timer_dly = PKEY_ACT_DELAY_7S,
	.timer_deb = PKEY_ACT_DEB_1S,
	.ctrl_params = PKEY_SR_DLY_30MS,
};

struct bcmpmu59xxx_pkey_pdata pkey_pdata = {
	.press_deb = PKEY_DEB_100MS,
	.release_deb = PKEY_DEB_100MS,
	.wakeup_deb = PKEY_WUP_DEB_1000MS,
	.t3 = &pkey_t3_action,
};

struct bcmpmu59xxx_audio_pdata audio_pdata = {
	.ihf_autoseq_dis = 100,
};

struct bcmpmu59xxx_rpc_pdata rpc_pdata = {
	.delay = 30000, /*rpc delay - 30 sec*/
	.fw_delay = 5000, /* for fw_cnt use this */
	.fw_cnt = 4,
	.poll_time = 120000, /* 40c-60c 120 sec */
	.htem_poll_time = 8000, /* > 60c 8 sec */
	.mod_tem = 400, /* 40 C*/
	.htem = 600, /* 60 C*/
};

struct bcmpmu59xxx_regulator_pdata rgltr_pdata = {
	.bcmpmu_rgltr = bcm59xxx_regulators,
	.num_rgltr = ARRAY_SIZE(bcm59xxx_regulators),
};

static struct bcmpmu_adc_lut batt_temp_map[] = {
	{16, 1000},   /* 100 C */
	{20, 950},   /* 95 C */
	{24, 900},   /* 90 C */
	{28, 850},   /* 85 C */
	{32, 800},   /* 80 C */
	{35, 750},   /* 75 C */
	{43, 700},   /* 70 C */
	{55, 650},   /* 65 C */
	{66, 600},   /* 60 C */
	{77, 550},   /* 55 C */
	{92, 500},   /* 50 C */
	{109, 450},   /* 45 C */
	{130, 400},   /* 40 C */
	{157, 350},   /* 35 C */
	{186, 300},   /* 30 C */
	{220, 250},   /* 25 C */
	{256, 200},   /* 20 C */
	{308, 150},   /* 15 C */
	{361, 100},   /* 10 C */
	{422, 50},   /* 5 C */
	{490, 0},   /* 0 C */
	{553, -50},   /* -5 C */
	{620, -100},   /* -10 C */
	{690, -150},   /* -15 C */
	{754, -200},   /* -20 C */
	{800, -250},   /* -25 C */
	{855, -300},   /* -30 C */
	{890, -350},   /* -35 C */
	{920, -400},   /* -40 C */
};

struct bcmpmu_adc_pdata adc_pdata[PMU_ADC_CHANN_MAX] = {
	[PMU_ADC_CHANN_VMBATT] = {
					.flag = 0,
					.volt_range = 4800,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "vmbatt",
					.reg = PMU_REG_ADCCTRL3,
	},
	[PMU_ADC_CHANN_VBBATT] = {
					.flag = 0,
					.volt_range = 4800,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "vbbatt",
					.reg = PMU_REG_ADCCTRL5,
	},
	[PMU_ADC_CHANN_VBUS] = {
					.flag = 0,
					.volt_range = 14400,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "vbus",
					.reg = PMU_REG_ADCCTRL9,
	},
	[PMU_ADC_CHANN_IDIN] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "idin",
					.reg = PMU_REG_ADCCTRL11,
	},

	[PMU_ADC_CHANN_BSI] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "bsi",
					.reg = PMU_REG_ADCCTRL15,
	},
	[PMU_ADC_CHANN_BOM] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "bom",
					.reg = PMU_REG_ADCCTRL17,
	},
	[PMU_ADC_CHANN_DIE_TEMP] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "dietemp",
					.reg = PMU_REG_ADCCTRL25,
	},
	[PMU_ADC_CHANN_NTC] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = batt_temp_map,
					.lut_len = ARRAY_SIZE(batt_temp_map),
					.name = "ntc",
					.reg = PMU_REG_ADCCTRL13,
	},
	/* Channel 32Ktemp, ALS, mapped to PATEM */
	[PMU_ADC_CHANN_32KTEMP] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = batt_temp_map,
					.lut_len = ARRAY_SIZE(batt_temp_map),
					.name = "32ktemp",
					.reg = PMU_REG_ADCCTRL21,
	},
	[PMU_ADC_CHANN_PATEMP] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = batt_temp_map,
					.lut_len = ARRAY_SIZE(batt_temp_map),
					.name = "patemp",
					.reg = PMU_REG_ADCCTRL21,
	},
	[PMU_ADC_CHANN_ALS] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = batt_temp_map,
					.lut_len = ARRAY_SIZE(batt_temp_map),
					.name = "als",
					.reg = PMU_REG_ADCCTRL21,
	},
};

struct bcmpmu_acld_pdata acld_pdata = {
	.acld_vbus_margin = 200,	/*mV*/
	.acld_vbus_thrs = 5950,
	.acld_vbat_thrs = 3500,
	/* CIG22H2R2MNE, rated current 1.6A  */
	.i_sat = 1600,		/* saturation current in mA
						for chrgr while using ACLD */
	.i_def_dcp = 700,
	.i_max_cc = 2200,
	.acld_cc_lmt = 1500,
	.otp_cc_trim = 0x1F,
};


/* SS Elentech Vivalto3G profile 2014.4.25 */
static struct batt_volt_cap_map ss_vivaltods5m_volt_cap_lut[] = {
	{4320, 100},
	{4252, 95 },
	{4190, 90 },
	{4133, 85 },
	{4088, 80 },
	{4043, 75 },
	{3998, 70 },
	{3958, 65 },
	{3917, 60 },
	{3873, 56 },
	{3837, 51 },
	{3813, 46 },
	{3795, 41 },
	{3781, 36 },
	{3772, 31 },
	{3766, 26 },
	{3749, 21 },
	{3716, 16 },
	{3671, 11 },
	{3667, 10 },
	{3663, 9  },
	{3658, 8  },
	{3652, 7  },
	{3646, 6  },
	{3638, 5  },
	{3624, 4  },
	{3597, 3  },
	{3549, 2  },

#if defined(PMU_BATT_CUTOFF_EXTENDED)
	{3400, 1  },
	{3300, 0  },
#else
	{3484, 1  },
	{3400, 0  },
#endif

};

/* SS Elentech vivaltods5m off -15% profile 2014.2.12 */
static struct batt_eoc_curr_cap_map ss_vivaltods5m_eoc_cap_lut[] = {
	 {408, 90 },
	 {408, 91 },
	 {395, 92 },
	 {359, 93 },
	 {326, 94 },
	 {289, 95 },
	 {253, 96 },
	 {217, 97 },
	 {176, 98 },
	 {140, 99 },
	 {125, 100},
	 {0, 100},
};


#if defined(PMU_BATT_CUTOFF_EXTENDED)
static struct batt_cutoff_cap_map ss_vivaltods5m_cutoff_cap_lut[] = {
	{3400, 2  },
	{3350, 1  },
	{3300, 0},

};
#else
static struct batt_cutoff_cap_map ss_vivaltods5m_cutoff_cap_lut[] = {
	{3520, 2  },
	{3450, 1  },
	{3350, 0},

};
#endif


/* SS Elentech Vivalto3G profile 2014.4.21 1490mAh */
static struct batt_esr_temp_lut ss_vivaltods5m_esr_temp_lut[] = {
	{
		.temp = -200,
		.reset = 0, .fct = 228, .guardband = 50,
		.esr_vl_lvl = 3835, .esr_vl_slope = -8395, .esr_vl_offset = 34461,
		.esr_vm_lvl = 4001, .esr_vm_slope = -681,  .esr_vm_offset = 4881,
		.esr_vh_lvl = 4252, .esr_vh_slope = 742,   .esr_vh_offset = -813,
				    .esr_vf_slope = -10150, .esr_vf_offset = 45503,
	},

	{
		.temp = -100,
		.reset = 0, .fct = 551, .guardband = 50,
		.esr_vl_lvl = 3812, .esr_vl_slope = -12148, .esr_vl_offset = 47638,
		.esr_vm_lvl = 4135, .esr_vm_slope = 405,    .esr_vm_offset = -208,
		.esr_vh_lvl = 4252, .esr_vh_slope = -826,   .esr_vh_offset = 4882,
				    .esr_vf_slope = -7913, .esr_vf_offset = 35018,
	},

	{
		.temp = 0,
		.reset = 0, .fct = 762, .guardband = 30,
		.esr_vl_lvl = 3671, .esr_vl_slope = -27104, .esr_vl_offset = 101331,
		.esr_vm_lvl = 3812, .esr_vm_slope = -8081,  .esr_vm_offset = 31496,
		.esr_vh_lvl = 4090, .esr_vh_slope = 764,    .esr_vh_offset = -2217,
				    .esr_vf_slope = -2022, .esr_vf_offset = 9177,
	},

	{
		.temp = 100,
		.reset = 0, .fct = 890, .guardband = 30,
		.esr_vl_lvl = 3716, .esr_vl_slope = -7837, .esr_vl_offset = 30047,
		.esr_vm_lvl = 3794, .esr_vm_slope = -6620, .esr_vm_offset = 25524,
		.esr_vh_lvl = 4046, .esr_vh_slope = 609,   .esr_vh_offset = -1899,
				    .esr_vf_slope = -1030, .esr_vf_offset = 4732,
	},

	{
		.temp=250,
		.reset = 0, .fct = 1000, .guardband = 30,
		.esr_vl_lvl = 3653, .esr_vl_slope = -2184, .esr_vl_offset = 8588,
		.esr_vm_lvl = 3780, .esr_vm_slope = -2874, .esr_vm_offset = 11108,
		.esr_vh_lvl = 3922, .esr_vh_slope = 577,   .esr_vh_offset = -1937,
				    .esr_vf_slope = -320, .esr_vf_offset = 1581,
	},
};

/* SS Elentech vivaltods5m profile 2014.2.12 */
static struct bcmpmu_batt_property ss_vivaltods5m_props = {
	.model = "SS Elentech",
#if defined(PMU_BATT_CUTOFF_EXTENDED)
	.min_volt = 3300,
#else
	.min_volt = 3350,
#endif
	.max_volt = 4350,
	.full_cap = 1500 * 3600,
	.one_c_rate = 1500,
	.volt_cap_lut = ss_vivaltods5m_volt_cap_lut,
	.volt_cap_lut_sz = ARRAY_SIZE(ss_vivaltods5m_volt_cap_lut),
	.esr_temp_lut = ss_vivaltods5m_esr_temp_lut,
	.esr_temp_lut_sz = ARRAY_SIZE(ss_vivaltods5m_esr_temp_lut),
	.eoc_cap_lut = ss_vivaltods5m_eoc_cap_lut,
	.eoc_cap_lut_sz = ARRAY_SIZE(ss_vivaltods5m_eoc_cap_lut),
	.cutoff_cap_lut = ss_vivaltods5m_cutoff_cap_lut,
	.cutoff_cap_lut_sz = ARRAY_SIZE(ss_vivaltods5m_cutoff_cap_lut),
};

static struct bcmpmu_batt_cap_levels ss_vivaltods5m_cap_levels = {
	.critical = 2,
	.low = 15,
	.normal = 75,
	.high = 95,
};

/* SS Elentech vivaltods5m profile 2014.2.12 */
static struct bcmpmu_batt_volt_levels ss_vivaltods5m_volt_levels = {
#if defined(PMU_BATT_CUTOFF_EXTENDED)
	.critical = 3400,
#else
	.critical = 3510,
#endif
	.low = 3685,
	.normal = 4060,
	.high   = 4250,
	.crit_cutoff_cnt = 3,
	.vfloat_lvl = 0x13, /* 4.345 V */
	.vfloat_max = 0x13,
	.vfloat_gap = 150, /* in mV */
};

static struct bcmpmu_batt_cal_data ss_vivaltods5m_cal_data = {
	.volt_low = 3300,
	.cap_low = 0,
};

static struct bcmpmu_fg_pdata fg_pdata = {
	.batt_prop = &ss_vivaltods5m_props,
	.cap_levels = &ss_vivaltods5m_cap_levels,
	.volt_levels = &ss_vivaltods5m_volt_levels,
	.cal_data = &ss_vivaltods5m_cal_data,
	.sns_resist = 10,
	.sys_impedence = 33,
	/* End of charge current in mA */ /* Samsung spec TBD */
	.eoc_current = 100,
	/* enable HW EOC of PMU */
	.hw_maintenance_charging = false,
	/* floor during sleep from Hawaii HW workshop Dec7 2012 */
	.sleep_current_ua = 1460,
	.sleep_sample_rate = 32000,

	/* vivaltods5m R01 bd: 21 Nov 2013. Max err 0.786 */
	.fg_factor = 972,

	.poll_rate_low_batt =  30000, /* every 30 seconds */
	.poll_rate_crit_batt = 5000, /* every 5 Seconds */
	.ntc_high_temp = 680, /*battery too hot shdwn temp*/
};

struct bcmpmu59xxx_accy_pdata accy_pdata = {
	.flags = ACCY_USE_PM_QOS,
	.qos_pi_id = PI_MGR_PI_ID_ARM_SUB_SYSTEM,
};

#ifdef CONFIG_CHARGER_BCMPMU_SPA
struct bcmpmu59xxx_spa_pb_pdata spa_pb_pdata = {
	.chrgr_name = "bcmpmu_charger",
};
#endif /*CONFIG_CHARGER_BCMPMU_SPA*/

#ifdef CONFIG_SEC_CHARGING_FEATURE
struct spa_power_data spa_data = {
	.charger_name = "bcmpmu_charger",
	.batt_cell_name = "Elentech",

	.suspend_temp_hot   =  600,
	.recovery_temp_hot  =  460,
	.suspend_temp_cold  = -50,
	.recovery_temp_cold = 0,

#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
	.eoc_current = 150, //Battery side 150mA
	.backcharging_time = 35, //min
	.recharging_eoc = 40, //Battery side 40mA
#else
	.eoc_current = 100,
#endif
	.recharge_voltage = 4300,
	.charging_cur_usb = 510, //output regulation
	.charging_cur_wall = 650, //output regulation
	.charging_cur_cdp_usb = 650, //output regulation
	.charge_timer_limit = CHARGE_TIMER_6HOUR,
};

static struct platform_device spa_power_device = {
	.name = "spa_power",
	.id = -1,
	.dev.platform_data = &spa_data,
};

static struct platform_device spa_ps_device = {
	.name = "spa_ps",
	.id = -1,
};

static struct platform_device *spa_devices[] = {
	&spa_power_device,
	&spa_ps_device,
};
#endif /*CONFIG_SEC_CHARGING_FEATURE*/

/* The subdevices of the bcmpmu59xxx */
static struct mfd_cell pmu59xxx_devs[] = {
	{
		.name = "bcmpmu59xxx-regulator",
		.id = -1,
		.platform_data = &rgltr_pdata,
		.pdata_size = sizeof(rgltr_pdata),
	},
	{
		.name = "bcmpmu_charger",
		.id = -1,
	},
	{
		.name = "bcmpmu59xxx-ponkey",
		.id = -1,
		.platform_data = &pkey_pdata,
		.pdata_size = sizeof(pkey_pdata),
	},
	{
		.name = "bcmpmu59xxx_rtc",
		.id = -1,
	},
	{
		.name = "bcmpmu_audio",
		.id = -1,
		.platform_data = &audio_pdata,
		.pdata_size = sizeof(audio_pdata),
	},
	{
		.name = "bcmpmu_accy",
		.id = -1,
		.platform_data = &accy_pdata,
		.pdata_size = sizeof(accy_pdata),
	},
	{
		.name = "bcmpmu_adc",
		.id = -1,
		.platform_data = adc_pdata,
		.pdata_size = sizeof(adc_pdata),
	},
#ifdef CONFIG_CHARGER_BCMPMU_SPA
	{
		.name = "bcmpmu_spa_pb",
		.id = -1,
		.platform_data = &spa_pb_pdata,
		.pdata_size = sizeof(spa_pb_pdata),
	},
#endif /*CONFIG_CHARGER_BCMPMU_SPA*/
	{
		.name = "bcmpmu_otg_xceiv",
		.id = -1,
	},
	{
		.name = "bcmpmu_rpc",
		.id = -1,
		.platform_data = &rpc_pdata,
		.pdata_size = sizeof(rpc_pdata),
	},
	{
		.name = "bcmpmu_fg",
		.id = -1,
		.platform_data = &fg_pdata,
		.pdata_size = sizeof(fg_pdata),
	},

};

static struct i2c_board_info pmu_i2c_companion_info[] = {
	{
	I2C_BOARD_INFO("bcmpmu_map1", PMU_DEVICE_I2C_ADDR1),
	},
};

static struct bcmpmu59xxx_platform_data bcmpmu_i2c_pdata = {
#if defined(CONFIG_KONA_PMU_BSC_HS_MODE)
	.i2c_pdata = { ADD_I2C_SLAVE_SPEED(BSC_BUS_SPEED_HS), },
#elif defined(CONFIG_KONA_PMU_BSC_HS_1MHZ)
	.i2c_pdata = { ADD_I2C_SLAVE_SPEED(BSC_BUS_SPEED_HS_1MHZ), },
#elif defined(CONFIG_KONA_PMU_BSC_HS_1625KHZ)
	.i2c_pdata = { ADD_I2C_SLAVE_SPEED(BSC_BUS_SPEED_HS_1625KHZ), },
#else
	.i2c_pdata = { ADD_I2C_SLAVE_SPEED(BSC_BUS_SPEED_50K), },
#endif
	.init = bcmpmu_init_platform_hw,
	.exit = bcmpmu_exit_platform_hw,
	.companion = BCMPMU_DUMMY_CLIENTS,
	.i2c_companion_info = pmu_i2c_companion_info,
	.i2c_adapter_id = PMU_DEVICE_I2C_BUSNO,
	.i2c_pagesize = 256,
	.init_data = register_init_data,
	.init_max = ARRAY_SIZE(register_init_data),
#ifdef CONFIG_CHARGER_BCMPMU_SPA
	.flags = BCMPMU_SPA_EN,
	.bc = BC_EXT_DETECT,
#else
	.bc = BCMPMU_BC_PMU_BC12,
#endif

	.force_adc_mode = PMU_ADC_REQ_RTM_MODE,
};

static struct i2c_board_info __initdata bcmpmu_i2c_info[] = {
	{
		I2C_BOARD_INFO("bcmpmu59xxx_i2c", PMU_DEVICE_I2C_ADDR),
		.platform_data = &bcmpmu_i2c_pdata,
		.irq = gpio_to_irq(PMU_DEVICE_INT_GPIO),
	},
};

int bcmpmu_get_pmu_mfd_cell(struct mfd_cell **pmu_cell)
{
	*pmu_cell  = pmu59xxx_devs;
	return ARRAY_SIZE(pmu59xxx_devs);
}
EXPORT_SYMBOL(bcmpmu_get_pmu_mfd_cell);

void bcmpmu_set_pullup_reg(void)
{
	u32 val1, val2;

	val1 = readl(KONA_CHIPREG_VA + CHIPREG_SPARE_CONTROL0_OFFSET);
	val2 = readl(KONA_PMU_BSC_VA + I2C_MM_HS_PADCTL_OFFSET);
	val1 |= (1 << 20 | 1 << 22);
	val2 |= (1 << I2C_MM_HS_PADCTL_PULLUP_EN_SHIFT);
	writel(val1, KONA_CHIPREG_VA + CHIPREG_SPARE_CONTROL0_OFFSET);

	/* GPIO16/GPI17 for sec_nfc_i2c 1.06kohm */
	val2 = readl(KONA_CHIPREG_VA + CHIPREG_SPARE_CONTROL0_OFFSET);
	val2 |= 0x00050000;
	writel(val2, KONA_CHIPREG_VA + CHIPREG_SPARE_CONTROL0_OFFSET);
}
static struct bcmpmu59xxx *pmu;

int bcmpmu_init_sr_volt()
{
#ifdef CONFIG_KONA_AVS
	int msr_ret_vlt;
	u8 sdsr_vlt = 0;
	int adj_vlt;
	u8 sdsr_ret_reg = 0;
	int sdsr_vret;

	pr_info("REG: pmu_init_platform_hw called\n");
	BUG_ON(!pmu);
	/* ADJUST MSR RETN VOLTAGE */
	msr_ret_vlt = get_vddvar_retn_vlt_id();
	if (msr_ret_vlt < 0) {
		pr_err("%s: Wrong retn voltage value\n", __func__);
		return -EINVAL;
	}
	pr_info("MSR Retn Voltage ID: 0x%x", msr_ret_vlt);
	pmu->write_dev(pmu, PMU_REG_MMSRVOUT2, (u8)msr_ret_vlt);
	/* ADJUST SDSR1 ACTIVE VOLTAGE */
	pmu->read_dev(pmu, PMU_REG_SDSR1VOUT1, &sdsr_vlt);
	adj_vlt = get_vddfix_vlt_adj(sdsr_vlt & PMU_SR_VOLTAGE_MASK);
	if (adj_vlt < 0) {
		pr_err("%s: Wrong Voltage val for SDSR active\n", __func__);
		return -EINVAL;
	}
	sdsr_vlt &= ~PMU_SR_VOLTAGE_MASK;
	sdsr_vlt |= adj_vlt << PMU_SR_VOLTAGE_SHIFT;
	pr_info("SDSR1 Active Voltage ID: 0x%x", sdsr_vlt);
	pmu->write_dev(pmu, PMU_REG_SDSR1VOUT1, sdsr_vlt);
	/* ADJUST SDSR1 RETN VOLTAGE */
	pmu->read_dev(pmu, PMU_REG_SDSR1VOUT2, &sdsr_ret_reg);
	sdsr_vret = get_vddfix_retn_vlt_id(sdsr_ret_reg & PMU_SR_VOLTAGE_MASK);
	if (sdsr_vret < 0) {
		pr_err("%s: Wrong Voltage val for SDSR retn\n", __func__);
		return -EINVAL;
	}
	sdsr_ret_reg &= ~PMU_SR_VOLTAGE_MASK;
	sdsr_ret_reg |= sdsr_vret << PMU_SR_VOLTAGE_SHIFT;
	pr_info("SDSR1 Retn voltage ID: 0x%x\n", sdsr_ret_reg);
	pmu->write_dev(pmu, PMU_REG_SDSR1VOUT2, sdsr_ret_reg);
#endif
	return 0;
}

void bcmpmu_populate_volt_dbg_log(struct pmu_volt_dbg *dbg_log)
{
	pmu->read_dev(pmu, PMU_REG_MMSRVOUT2, &dbg_log->msr_retn);
	pmu->read_dev(pmu, PMU_REG_SDSR1VOUT1, &dbg_log->sdsr1[0]);
	pmu->read_dev(pmu, PMU_REG_SDSR1VOUT2, &dbg_log->sdsr1[1]);
	pmu->read_dev(pmu, PMU_REG_SDSR2VOUT1, &dbg_log->sdsr2[0]);
	pmu->read_dev(pmu, PMU_REG_SDSR2VOUT2, &dbg_log->sdsr2[1]);
	pr_info("Populated voltage settings for debug");
}

static int bcmpmu_init_platform_hw(struct bcmpmu59xxx *bcmpmu)
{
	pmu = bcmpmu;
	return 0;
}

static int bcmpmu_exit_platform_hw(struct bcmpmu59xxx *bcmpmu)
{
	pr_info("REG: pmu_exit_platform_hw called\n");
	return 0;
}

int __init board_bcm59xx_init(void)
{
	int             ret = 0;
	int             irq;

	bcmpmu_set_pullup_reg();
	ret = gpio_request(PMU_DEVICE_INT_GPIO, "bcmpmu59xxx-irq");
	if (ret < 0) {
		pr_err("<%s> failed at gpio_request\n", __func__);
		goto exit;
	}
	ret = gpio_direction_input(PMU_DEVICE_INT_GPIO);
	if (ret < 0) {

		pr_err("%s filed at gpio_direction_input.\n",
				__func__);
		goto exit;
	}
	irq = gpio_to_irq(PMU_DEVICE_INT_GPIO);
	bcmpmu_i2c_pdata.irq = irq;
	ret  = i2c_register_board_info(PMU_DEVICE_I2C_BUSNO,
			bcmpmu_i2c_info, ARRAY_SIZE(bcmpmu_i2c_info));
#ifdef CONFIG_CHARGER_BCMPMU_SPA
	platform_add_devices(spa_devices, ARRAY_SIZE(spa_devices));
#endif
	return 0;
exit:
	return ret;
}

__init int board_pmu_init(void)
{
	return board_bcm59xx_init();
}
arch_initcall(board_pmu_init);
