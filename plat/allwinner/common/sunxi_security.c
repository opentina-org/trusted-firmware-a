/*
 * Copyright (c) 2017-2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <lib/mmio.h>

#include <sunxi_ccu.h>
#include <sunxi_mmap.h>
#include <sunxi_private.h>
#include <sunxi_spc.h>

#define DMA_SEC_REG		0x20

void sunxi_prcm_security_setup(void)
{
	unsigned int reg_value = 0;
	int i;

	for (i = 0; i < 3; i++) {
		mmio_write_32(TZPC_DECPORT_SET_REG(i), 0xffffffff);
	}

	reg_value = mmio_read_32(SUNXI_R_PRCM_SEC_SWITCH_REG);
	reg_value |= (0x7 << 0);
	mmio_write_32(SUNXI_R_PRCM_SEC_SWITCH_REG, reg_value);

}

#define SUNXI_TZMA_EN	(SUNXI_TZMA_BASE + 0x0)
/*
 * Setup the peripherals to be accessible by non-secure world.
 * This will not work for the Secure Peripherals Controller (SPC) unless
 * a fuse it burnt (seems to be an erratum), but we do it nevertheless,
 * to allow booting on boards using secure boot.
 */
void sunxi_security_setup(void)
{
	int i;

	INFO("Configuring SPC Controller\n");

	for (i = 0; i <= 13; i++)
		mmio_write_32(SUNXI_SPC_DECPORT_SET_REG(i), 0xffffffff);
	for (i = 0; i <= 8; i++)
		mmio_write_32(0x100 + SUNXI_SPC_DECPORT_SET_REG(i), 0xffffffff);

	mmio_write_32(SUNXI_TZMA_EN, 0x0);

	sunxi_prcm_security_setup();

	mmio_write_32(SUNXI_CCU_SEC_SWITCH_REG, 0x7);

	mmio_write_32(SUNXI_DMA_BASE + DMA_SEC_REG, 0xffff);

	mmio_write_32(SUNXI_DMAC1_BASE + DMA_SEC_REG, 0xffff);

	mmio_write_32(SUNXI_SPC_BASE + 0xE0, 0x3);

	mmio_write_32(SUNXI_TIMER0_CLK_REG, 0x80000002);
	for (i = 0; i < 100; i++)
		;
	mmio_write_32(SUNXI_TIMER_BGR_REG, 0x10001);
	for (i = 0; i < 100; i++)
		;
	mmio_write_32(SUNXI_TIMER_BASE + 0x08, 0);
}