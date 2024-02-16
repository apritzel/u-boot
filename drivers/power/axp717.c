// SPDX-License-Identifier: GPL-2.0+
/*
 * AXP717 SPL driver
 * (C) Copyright 2024 Arm Ltd.
 */

#include <common.h>
#include <command.h>
#include <errno.h>
#include <asm/arch/pmic_bus.h>
#include <axp_pmic.h>

enum axp717_reg {
	AXP717_CHIP_VERSION = 0x3,
	AXP717_SHUTDOWN = 0x27,
	AXP717_OUTPUT_CTRL1 = 0x80,
	AXP717_DCDC3_VOLTAGE = 0x85,
};

#define AXP717_CHIP_VERSION_MASK	0xc8

#define AXP717_OUTPUT_CTRL1_DCDC3_EN	(1 << 2)

#define AXP717_POWEROFF			(1 << 0)

#define AXP717_DCDC3_1220MV_OFFSET	71

static u8 axp_mvolt_to_cfg(int mvolt, int min, int max, int div)
{
	if (mvolt < min)
		mvolt = min;
	else if (mvolt > max)
		mvolt = max;

	return (mvolt - min) / div;
}

int axp_set_dcdc3(unsigned int mvolt)
{
	int ret;
	u8 cfg;

	if (mvolt >= 1220)
		cfg = AXP717_DCDC3_1220MV_OFFSET +
			axp_mvolt_to_cfg(mvolt, 1220, 1840, 20);
	else
		cfg = axp_mvolt_to_cfg(mvolt, 500, 1200, 10);

	if (mvolt == 0)
		return pmic_bus_clrbits(AXP717_OUTPUT_CTRL1,
					AXP717_OUTPUT_CTRL1_DCDC3_EN);

	ret = pmic_bus_write(AXP717_DCDC3_VOLTAGE, cfg);
	if (ret)
		return ret;

	return pmic_bus_setbits(AXP717_OUTPUT_CTRL1,
				AXP717_OUTPUT_CTRL1_DCDC3_EN);
}

int axp_init(void)
{
	u8 axp_chip_id;
	int ret;

	ret = pmic_bus_init();
	if (ret)
		return ret;

	ret = pmic_bus_read(AXP717_CHIP_VERSION, &axp_chip_id);
	if (ret)
		return ret;

	if ((axp_chip_id & AXP717_CHIP_VERSION_MASK) != 0x00)
		return -ENODEV;

	return ret;
}

#if !CONFIG_IS_ENABLED(ARM_PSCI_FW) && !IS_ENABLED(CONFIG_SYSRESET_CMD_POWEROFF)
int do_poweroff(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	pmic_bus_write(AXP305_SHUTDOWN, AXP305_POWEROFF);

	/* infinite loop during shutdown */
	while (1) {}

	/* not reached */
	return 0;
}
#endif
