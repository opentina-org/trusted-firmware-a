/*
 * Copyright (c) 2017-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <libfdt.h>

#include <platform_def.h>

#include <arch.h>
#include <arch_helpers.h>
#include <common/debug.h>
#include <common/fdt_fixup.h>
#include <common/fdt_wrappers.h>
#include <drivers/console.h>
#include <drivers/generic_delay_timer.h>
#include <drivers/ti/uart/uart_16550.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>

#include <sunxi_def.h>
#include <sunxi_mmap.h>
#include <sunxi_private.h>


static entry_point_info_t bl32_image_ep_info;
static entry_point_info_t bl33_image_ep_info;

console_t console;

#ifdef SUNXI_CPU_GIC600_BASE //gic 600 controller exist, we are using gicv3
#include <drivers/arm/gicv3.h>

uintptr_t rdistif_base_addrs[PLATFORM_CORE_COUNT];

static const gicv3_driver_data_t sunxi_gic_data = {
	.gicd_base = SUNXI_GICD_BASE,
	.gicr_base = SUNXI_GICR_BASE,
	.rdistif_num = PLATFORM_CORE_COUNT,
	.rdistif_base_addrs = rdistif_base_addrs,
	/*
	 * we set no interrupt as group0(handle by EL3) or group1s(handle by S-EL1) for now
	 * all interrupts aussumpt to be handled by NS-EL1
	 */
	.interrupt_props = NULL,
	.interrupt_props_num = 0,
};
#else
#include <drivers/arm/gicv2.h>
static const gicv2_driver_data_t sunxi_gic_data = {
	.gicd_base = SUNXI_GICD_BASE,
	.gicc_base = SUNXI_GICC_BASE,
};
#endif

/*
 * Try to find a DTB loaded in memory by previous stages.
 *
 * At the moment we implement a heuristic to find the DTB attached to U-Boot:
 * U-Boot appends its DTB to the end of the image. Assuming that BL33 is
 * U-Boot, try to find the size of the U-Boot image to learn the DTB address.
 * The generic ARMv8 U-Boot image contains the load address and its size
 * as u64 variables at the beginning of the image. There might be padding
 * or other headers before that data, so scan the first 2KB after the BL33
 * entry point to find the load address, which should be followed by the
 * size. Adding those together gives us the address of the DTB.
 */
static void *sunxi_find_dtb(void)
{
	uint64_t *u_boot_base;
	int i;

	u_boot_base = (void *)SUNXI_BL33_VIRT_BASE;

	for (i = 0; i < 2048 / sizeof(uint64_t); i++) {
		uint32_t *dtb_base;

		if (u_boot_base[i] != PRELOADED_BL33_BASE)
			continue;

		/* Does the suspected U-Boot size look anyhow reasonable? */
		if (u_boot_base[i + 1] >= 256 * 1024 * 1024)
			continue;

		/* end of the image: base address + size */
		dtb_base = (void *)((char *)u_boot_base + u_boot_base[i + 1]);

		if (fdt_check_header(dtb_base) != 0)
			continue;

		return dtb_base;
	}

	return NULL;
}

void bl31_early_platform_setup2(u_register_t arg0, u_register_t arg1,
				u_register_t arg2, u_register_t arg3)
{
	/* Initialize the debug console as soon as possible */
	sunxi_uart_init();

	uint32_t jump_instruction;
#ifdef BL32_BASE
	VERBOSE("BL31: BL32_BASE = 0x%x\n", BL32_BASE);  // 0x48600000
	/*
	 * Auto detect OP-TEE image type (64bit or 32bit) from `tee_spare_head.jump_instruction`.
	 */
	jump_instruction = *(uint32_t *)(unsigned long)BL32_BASE;
	if ((jump_instruction & ~GENMASK(25, 0)) == 0x14000000) {
		NOTICE("BL31: OP-TEE 64bit detected\n");
		SET_PARAM_HEAD(&bl32_image_ep_info, PARAM_EP, VERSION_1, 0);
		SET_SECURITY_STATE(bl32_image_ep_info.h.attr, SECURE);
		bl32_image_ep_info.pc = BL32_BASE;
		bl32_image_ep_info.spsr = SPSR_64(MODE_EL1, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);
		bl32_image_ep_info.args.arg0 = MODE_RW_64;

	} else if ((jump_instruction & ~GENMASK(23, 0)) == 0xEA000000) {
		NOTICE("BL31: OP-TEE 32bit detected\n");
		SET_PARAM_HEAD(&bl32_image_ep_info, PARAM_EP, VERSION_1, 0);
		SET_SECURITY_STATE(bl32_image_ep_info.h.attr, SECURE);
		bl32_image_ep_info.pc = BL32_BASE;
		bl32_image_ep_info.spsr = SPSR_MODE32(MODE32_svc, SPSR_T_ARM, SPSR_E_LITTLE, DISABLE_ALL_EXCEPTIONS);
		bl32_image_ep_info.args.arg0 = MODE_RW_32;

#if !CTX_INCLUDE_AARCH32_REGS
		ERROR("We can't set CTX_INCLUDE_AARCH32_REG to be 0 when using 32bit OP-TEE\n");
		ERROR("Please delete the config `CTX_INCLUDE_AARCH32_REGS := 0` in current plaform.mk\n");
		//panic();	// fix me: when optee does not exist, it will unexpectedly go to the 32-bit branch
#endif
	} else {
		ERROR("BL31: Unknown jump instruction in OP-TEE header\n");
	}
#endif

	/* Populate entry point information for BL33 */
	SET_PARAM_HEAD(&bl33_image_ep_info, PARAM_EP, VERSION_1, 0);
	/*
	 * Tell BL31 where the non-trusted software image
	 * is located and the entry state information
	 */
#ifdef PLAT_SUNXI_NS_IMAGE_OFFSET
	bl33_image_ep_info.pc = PLAT_SUNXI_NS_IMAGE_OFFSET;
	jump_instruction = *(uint32_t *)(unsigned long)PLAT_SUNXI_NS_IMAGE_OFFSET;

	if ((jump_instruction & ~GENMASK(25, 0)) == 0x14000000) {
		//64bit
		NOTICE("BL31: U-BOOT 64bit detected\n");
		bl33_image_ep_info.spsr = SPSR_64(MODE_EL1, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);

	} else if ((jump_instruction & ~GENMASK(23, 0)) == 0xEA000000) {
		NOTICE("BL31: U-BOOT 32bit detected\n");
		bl33_image_ep_info.spsr = SPSR_MODE32(MODE32_svc, SPSR_T_ARM, SPSR_E_LITTLE, DISABLE_ALL_EXCEPTIONS);
	} else {
		ERROR("BL31: Unknown jump instruction in U-BOOT header\n");
	}
#else
	bl33_image_ep_info.pc = PRELOADED_BL33_BASE;
	bl33_image_ep_info.spsr = SPSR_64(MODE_EL2, MODE_SP_ELX,
					  DISABLE_ALL_EXCEPTIONS);
#endif
	SET_SECURITY_STATE(bl33_image_ep_info.h.attr, NON_SECURE);
}

void bl31_plat_arch_setup(void)
{
	sunxi_configure_mmu_el3(0);
}

void bl31_platform_setup(void)
{
	const char *soc_name;
	uint16_t soc_id = sunxi_read_soc_id();
	void *fdt;

	switch (soc_id) {
	case SUNXI_SOC_A64:
		soc_name = "A64/H64/R18";
		break;
	case SUNXI_SOC_H5:
		soc_name = "H5";
		break;
	case SUNXI_SOC_H6:
		soc_name = "H6";
		break;
	case SUNXI_SOC_H616:
		soc_name = "H616";
		break;
	default:
		soc_name = "unknown";
		break;
	}
	NOTICE("BL31: Detected Allwinner %s SoC (%04x)\n", soc_name, soc_id);

	generic_delay_timer_init();

	fdt = sunxi_find_dtb();
	if (fdt) {
		const char *model;
		int length;

		model = fdt_getprop(fdt, 0, "model", &length);
		NOTICE("BL31: Found U-Boot DTB at %p, model: %s\n", fdt,
		     model ?: "unknown");
	} else {
		NOTICE("BL31: No DTB found.\n");
	}

#ifdef SUNXI_CPU_GIC600_BASE //gic 600 controller exist, we are using gicv3
	gicv3_driver_init(&sunxi_gic_data);
	gicv3_distif_init();
	gicv3_rdistif_init(plat_my_core_pos());
	gicv3_cpuif_enable(plat_my_core_pos());
#else
	/* Configure the interrupt controller */
	gicv2_driver_init(&sunxi_gic_data);
	gicv2_distif_init();
	gicv2_pcpu_distif_init();
	gicv2_cpuif_enable();

	gic_cpuif_init();
	gic_distributor_init();
#endif

	sunxi_security_setup();

	/*
	 * On the A64 U-Boot's SPL sets the bus clocks to some conservative
	 * values, to work around FEL mode instabilities with SRAM C accesses.
	 * FEL mode is gone when we reach ATF, so bring the AHB1 bus
	 * (the "main" bus) clock frequency back to the recommended 200MHz,
	 * for improved performance.
	 */
	if (soc_id == SUNXI_SOC_A64)
		mmio_write_32(SUNXI_CCU_BASE + 0x54, 0x00003180);

	/*
	 * U-Boot or the kernel don't setup AHB2, which leaves it at the
	 * AHB1 frequency (200 MHz, see above). However Allwinner recommends
	 * 300 MHz, for improved Ethernet and USB performance. Switch the
	 * clock to use "PLL_PERIPH0 / 2".
	 */
	if (soc_id == SUNXI_SOC_A64 || soc_id == SUNXI_SOC_H5)
		mmio_write_32(SUNXI_CCU_BASE + 0x5c, 0x1);

	sunxi_pmic_setup(soc_id, fdt);

	sunxi_prepare_dtb(fdt);

	INFO("BL31: Platform setup done\n");
}

entry_point_info_t *bl31_plat_get_next_image_ep_info(uint32_t type)
{
	assert(sec_state_is_valid(type) != 0);

	if (type == NON_SECURE)
		return &bl33_image_ep_info;

	if ((type == SECURE) && bl32_image_ep_info.pc)
		return &bl32_image_ep_info;

	return NULL;
}
