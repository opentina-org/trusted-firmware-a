/*
 * Copyright (c) 2014, Allwinner Technology Co., Ltd. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../drivers/arm/gic/v3/gicv3_private.h"
#include <plat/common/platform.h>
#include <bl31/interrupt_mgmt.h>
#include <common/bl_common.h>
#include <common/debug.h>
#include <aw_gic_v3.h>
#include <sunxi_mmap.h>

#define SGI_MASK 0xffff

static uintptr_t rdistif_base_addrs[PLATFORM_CORE_COUNT];
static uint32_t rdist_has_saved[PLATFORM_CORE_COUNT];

/* we save and restore the GICv3 context on system suspend */
gicv3_dist_ctx_t dist_ctx;

static unsigned int aw_mpidr_to_core_pos(u_register_t mpidr)
{
	return plat_core_pos_by_mpidr(mpidr);
}

gicv3_driver_data_t aw_gicv3_data = {
	.gicd_base = SUNXI_GICD_BASE,
	.gicr_base = SUNXI_GICR_BASE,
	.rdistif_num = PLATFORM_CORE_COUNT,
	.rdistif_base_addrs = rdistif_base_addrs,
	.mpidr_to_core_pos = aw_mpidr_to_core_pos,
};

struct gic_chip_data {
	/* All cores share the same configuration */
	unsigned int saved_group;
	unsigned int saved_enable;
	unsigned int saved_conf0;
	unsigned int saved_conf1;
	unsigned int saved_grpmod;
	/* Per-core sgi */
	unsigned int saved_sgi[PLATFORM_CORE_COUNT];
};

static struct gic_chip_data gic_data;

void aw_gic_driver_init(void)
{
	gicv3_driver_init(&aw_gicv3_data);
}

void aw_gic_set_pending(uint32_t irq)
{
	gicv3_set_interrupt_pending(irq, plat_my_core_pos());
}

void aw_gic_distif_save(void)
{
	gicv3_distif_save(&dist_ctx);
}

void aw_gic_distif_restore(void)
{
	gicv3_distif_init_restore(&dist_ctx);
}

void aw_gic_rdistif_init(void)
{
	unsigned int proc_num;
	unsigned int index;
	uintptr_t gicr_base;

	proc_num = plat_my_core_pos();
	gicr_base = gicv3_driver_data->rdistif_base_addrs[proc_num];

	/* set all SGI/PPI as non-secure GROUP1 by default */
	mmio_write_32(gicr_base + GICR_IGROUPR0, ~0U);
	mmio_write_32(gicr_base + GICR_IGRPMODR0, 0x0);

	/* setup the default PPI/SGI priorities */
	for (index = 0; index < TOTAL_PCPU_INTR_NUM; index += 4U)
		gicr_write_ipriorityr(gicr_base, index,
				GICD_IPRIORITYR_DEF_VAL);
}

void aw_gic_rdistif_save(void)
{
	unsigned int proc_num;
	uintptr_t gicr_base;

	proc_num = plat_my_core_pos();
	gicr_base = gicv3_driver_data->rdistif_base_addrs[proc_num];

	gic_data.saved_group = mmio_read_32(gicr_base + GICR_IGROUPR0);
	gic_data.saved_enable = mmio_read_32(gicr_base + GICR_ISENABLER0);
	gic_data.saved_conf0 = mmio_read_32(gicr_base + GICR_ICFGR0);
	gic_data.saved_conf1 = mmio_read_32(gicr_base + GICR_ICFGR1);
	gic_data.saved_grpmod = mmio_read_32(gicr_base + GICR_IGRPMODR0);

	rdist_has_saved[proc_num] = 1;
}

void aw_gic_rdistif_restore(void)
{
	unsigned int proc_num;
	uintptr_t gicr_base;

	proc_num = plat_my_core_pos();
	if (rdist_has_saved[proc_num] == 1) {
		gicr_base = gicv3_driver_data->rdistif_base_addrs[proc_num];
		mmio_write_32(gicr_base + GICR_IGROUPR0, gic_data.saved_group);
		mmio_write_32(gicr_base + GICR_ISENABLER0,
			gic_data.saved_enable);
		mmio_write_32(gicr_base + GICR_ICFGR0, gic_data.saved_conf0);
		mmio_write_32(gicr_base + GICR_ICFGR1, gic_data.saved_conf1);
		mmio_write_32(gicr_base + GICR_IGRPMODR0,
			gic_data.saved_grpmod);
	}
}

void aw_gic_rdistif_restore_all(void)
{
	unsigned int proc_num;
	uintptr_t gicr_base;

	for (proc_num = 0; proc_num < PLATFORM_CORE_COUNT; proc_num++) {
		gicr_base = gicv3_driver_data->rdistif_base_addrs[proc_num];
		mmio_write_32(gicr_base + GICR_IGROUPR0, gic_data.saved_group);
		mmio_write_32(gicr_base + GICR_ISENABLER0,
			gic_data.saved_enable);
		mmio_write_32(gicr_base + GICR_ICFGR0, gic_data.saved_conf0);
		mmio_write_32(gicr_base + GICR_ICFGR1, gic_data.saved_conf1);
		mmio_write_32(gicr_base + GICR_IGRPMODR0,
			gic_data.saved_grpmod);
	}
}

void gic_sgi_save_all(void)
{
	unsigned int proc_num;
	uintptr_t gicr_base;

	for (proc_num = 0; proc_num < PLATFORM_CORE_COUNT; proc_num++) {
		gicr_base = gicv3_driver_data->rdistif_base_addrs[proc_num];
		gic_data.saved_sgi[proc_num] =
			mmio_read_32(gicr_base + GICR_ISPENDR0) & SGI_MASK;
	}
}

void gic_sgi_restore_all(void)
{
	unsigned int proc_num;
	uintptr_t gicr_base;

	for (proc_num = 0; proc_num < PLATFORM_CORE_COUNT; proc_num++) {
		gicr_base = gicv3_driver_data->rdistif_base_addrs[proc_num];
		mmio_write_32(gicr_base + GICR_ICPENDR0, SGI_MASK);
		mmio_write_32(gicr_base + GICR_ISPENDR0,
			gic_data.saved_sgi[proc_num] & SGI_MASK);
	}
}

void aw_gic_init(void)
{
	gicv3_distif_init();
	gicv3_rdistif_init(plat_my_core_pos());
	gicv3_cpuif_enable(plat_my_core_pos());
}

uint32_t aw_irq_get_pending(uint32_t irq)
{
	uint32_t val;

	val = mmio_read_32(SUNXI_GICD_BASE + GICD_ISPENDR +
		irq / 32 * 4);
	val = (val >> (irq % 32)) & 1U;
	return val;
}


void aw_irq_set_pending(uint32_t irq)
{
	uint32_t bit = 1U << (irq % 32);

	mmio_write_32(SUNXI_GICD_BASE + GICD_ISPENDR +
		irq / 32 * 4, bit);
}
