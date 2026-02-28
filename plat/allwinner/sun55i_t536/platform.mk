#
# Copyright (c) 2014, Allwinner Technology Co., Ltd. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
ARM_ARCH_MAJOR     := 8
ARM_ARCH_MINOR     := 2
SUNXI_PSCI_USE_SCPI	:=	0
SUNXI_PSCI_USE_NATIVE	:=	1
SUNXI_GIC_VERSION := v3
GICV3_SUPPORT_GIC600  := 1
HW_ASSISTED_COHERENCY := 1
USE_COHERENT_MEM := 0
ERRATA_A55_1530923 := 1
USE_SPINLOCK_CAS := 0

# This platform is single-cluster and does not require coherency setup.
WARMBOOT_ENABLE_DCACHE_EARLY	:=	1

include drivers/arm/gic/v3/gicv3.mk
# The differences between the platform are covered by the include files.
include plat/allwinner/common/allwinner-common.mk

BL31_SOURCES		+= lib/cpus/aarch64/cortex_a55.S                         \
					${AW_PLAT}/common/drivers/gic600/aw_gic_v3.c         \
					${AW_PLAT}/${PLAT}/sunxi_uart.c



