/*
 * Copyright (c) 2018-2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <common/runtime_svc.h>
#include <lib/cpus/errata_report.h>
#include <lib/cpus/wa_cve_2017_5715.h>
#include <lib/cpus/wa_cve_2018_3639.h>
#include <lib/smccc.h>
#include <services/arm_arch_svc.h>
#include <smccc_helpers.h>
#include <plat/common/platform.h>
#include <lib/el3_runtime/context_mgmt.h>

static int32_t smccc_version(void)
{
	return MAKE_SMCCC_VERSION(SMCCC_MAJOR_VERSION, SMCCC_MINOR_VERSION);
}

static int32_t smccc_arch_features(u_register_t arg1)
{
	switch (arg1) {
	case SMCCC_VERSION:
	case SMCCC_ARCH_FEATURES:
		return SMC_ARCH_CALL_SUCCESS;
	case SMCCC_ARCH_SOC_ID:
		return plat_is_smccc_feature_available(arg1);
#if WORKAROUND_CVE_2017_5715
	case SMCCC_ARCH_WORKAROUND_1:
		if (check_wa_cve_2017_5715() == ERRATA_NOT_APPLIES)
			return 1;
		return 0; /* ERRATA_APPLIES || ERRATA_MISSING */
#endif

#if WORKAROUND_CVE_2018_3639
	case SMCCC_ARCH_WORKAROUND_2: {
#if DYNAMIC_WORKAROUND_CVE_2018_3639
		unsigned long long ssbs;

		/*
		 * Firmware doesn't have to carry out dynamic workaround if the
		 * PE implements architectural Speculation Store Bypass Safe
		 * (SSBS) feature.
		 */
		ssbs = (read_id_aa64pfr1_el1() >> ID_AA64PFR1_EL1_SSBS_SHIFT) &
			ID_AA64PFR1_EL1_SSBS_MASK;

		/*
		 * If architectural SSBS is available on this PE, no firmware
		 * mitigation via SMCCC_ARCH_WORKAROUND_2 is required.
		 */
		if (ssbs != SSBS_UNAVAILABLE)
			return 1;

		/*
		 * On a platform where at least one CPU requires
		 * dynamic mitigation but others are either unaffected
		 * or permanently mitigated, report the latter as not
		 * needing dynamic mitigation.
		 */
		if (wa_cve_2018_3639_get_disable_ptr() == NULL)
			return 1;
		/*
		 * If we get here, this CPU requires dynamic mitigation
		 * so report it as such.
		 */
		return 0;
#else
		/* Either the CPUs are unaffected or permanently mitigated */
		return SMC_ARCH_CALL_NOT_REQUIRED;
#endif
	}
#endif

	/* Fallthrough */

	default:
		return SMC_UNK;
	}
}

/* return soc revision or soc version on success otherwise
 * return invalid parameter */
static int32_t smccc_arch_id(u_register_t arg1)
{
	if (arg1 == SMCCC_GET_SOC_REVISION) {
		return plat_get_soc_revision();
	}
	if (arg1 == SMCCC_GET_SOC_VERSION) {
		return plat_get_soc_version();
	}
	return SMC_ARCH_CALL_INVAL_PARAM;
}

enum {
	ARCH_ARM = 0,	/* arm */
	ARCH_ARM64,	/* arm64 */
};

/*******************************************************************************
 * This function programs EL3 registers and performs other setup to enable entry
 * into the next image after BL31 at the next ERET.
 ******************************************************************************/
void prepare_nonsec_os_entry(uint64_t kernel_addr, uint64_t dtb_addr, uint64_t arch, bool is_hyper)
{
	entry_point_info_t next_image_info;
	uint32_t image_type;
	unsigned long sctlr;

	/* Determine which image to execute next */
	image_type = NON_SECURE;

	/* Program EL3 registers to enable entry into the next EL */
	memset(&next_image_info, 0, sizeof(next_image_info));
	SET_SECURITY_STATE(next_image_info.h.attr, NON_SECURE);

	if (arch == ARCH_ARM) {
		next_image_info.spsr = SPSR_MODE32(MODE32_svc, SPSR_T_ARM, SPSR_E_LITTLE, DISABLE_ALL_EXCEPTIONS);
		next_image_info.pc = kernel_addr;
		next_image_info.args.arg0 = 0;
		next_image_info.args.arg1 = 0xffffffff;
		next_image_info.args.arg2 = dtb_addr;
		VERBOSE("BL3-1: ARCH_ARM: dtb address = 0x%llx\n", dtb_addr);
	} else if (arch == ARCH_ARM64) {
		if (is_hyper)
			next_image_info.spsr = SPSR_64(MODE_EL2, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);
		else
			next_image_info.spsr = SPSR_64(MODE_EL1, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);
		next_image_info.pc = kernel_addr;
		next_image_info.args.arg0 = dtb_addr;
		VERBOSE("BL3-1: ARCH_ARM64: dtb address = 0x%llx\n", dtb_addr);
	}
#ifdef TIMERSTAMP_BASE
	mmio_write_32(TIMERSTAMP_BASE+CNT_LOW_REG, 0);
	mmio_write_32(TIMERSTAMP_BASE+CNT_HI_REG, 0);
#endif
	NOTICE("BL3-1: Next image address = 0x%llx\n", (unsigned long long)next_image_info.pc);
	NOTICE("BL3-1: Next image spsr = 0x%x\n", next_image_info.spsr);

	/* Disable the data cache and mmu */
	__asm("mrs %0, SCTLR_EL1\n" : "=r" (sctlr));
	sctlr &= ~(0x5<<0);
	__asm volatile("msr SCTLR_EL1, %0\n" : : "r" (sctlr));

	/* Clean and invalidate all data from the L1 Data cache */
	dcsw_op_all(DCCISW);

	cm_init_my_context(&next_image_info);
	cm_prepare_el3_exit(image_type);
}

#define ARM_SVC_RUNNSOS                         0x8000ff04

/*
 * Top-level Arm Architectural Service SMC handler.
 */
static uintptr_t arm_arch_svc_smc_handler(uint32_t smc_fid,
	u_register_t x1,
	u_register_t x2,
	u_register_t x3,
	u_register_t x4,
	void *cookie,
	void *handle,
	u_register_t flags)
{
	switch (smc_fid) {
	case SMCCC_VERSION:
		SMC_RET1(handle, smccc_version());
	case SMCCC_ARCH_FEATURES:
		SMC_RET1(handle, smccc_arch_features(x1));
	case SMCCC_ARCH_SOC_ID:
		SMC_RET1(handle, smccc_arch_id(x1));
	case ARM_SVC_RUNNSOS:
		prepare_nonsec_os_entry(x1, x2, x3, false);
		SMC_RET0(handle);
#if WORKAROUND_CVE_2017_5715
	case SMCCC_ARCH_WORKAROUND_1:
		/*
		 * The workaround has already been applied on affected PEs
		 * during entry to EL3.  On unaffected PEs, this function
		 * has no effect.
		 */
		SMC_RET0(handle);
#endif
#if WORKAROUND_CVE_2018_3639
	case SMCCC_ARCH_WORKAROUND_2:
		/*
		 * The workaround has already been applied on affected PEs
		 * requiring dynamic mitigation during entry to EL3.
		 * On unaffected or statically mitigated PEs, this function
		 * has no effect.
		 */
		SMC_RET0(handle);
#endif
	default:
		WARN("Unimplemented Arm Architecture Service Call: 0x%x \n",
			smc_fid);
		SMC_RET1(handle, SMC_UNK);
	}
}

/* Register Standard Service Calls as runtime service */
DECLARE_RT_SVC(
		arm_arch_svc,
		OEN_ARM_START,
		OEN_ARM_END,
		SMC_TYPE_FAST,
		NULL,
		arm_arch_svc_smc_handler
);
