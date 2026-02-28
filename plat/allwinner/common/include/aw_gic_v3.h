/*
 * Copyright (c) 2014, Allwinner Technology Co., Ltd. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef AW_GIC_V3_H
#define AW_GIC_V3_H

#include <drivers/arm/gicv3.h>
#include <lib/mmio.h>

void aw_gic_driver_init(void);
void aw_gic_init(void);
void aw_gic_set_pending(uint32_t irq);
void aw_gic_distif_save(void);
void aw_gic_distif_restore(void);
void aw_gic_rdistif_init(void);
void aw_gic_rdistif_save(void);
void aw_gic_rdistif_restore(void);
void aw_gic_rdistif_restore_all(void);
void gic_sgi_save_all(void);
void gic_sgi_restore_all(void);
uint32_t aw_irq_get_pending(uint32_t irq);
void aw_irq_set_pending(uint32_t irq);

#endif /* AW_GIC_V3_H */
