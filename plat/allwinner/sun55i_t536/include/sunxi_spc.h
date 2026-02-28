/*
 * Copyright (c) 2014, Allwinner Technology Co., Ltd. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SUNXI_SPC_H
#define SUNXI_SPC_H

#define SUNXI_SPC_DECPORT_STA_REG(p)	(SUNXI_SPC_BASE + 0x0000 + 0x10 * (p))
#define SUNXI_SPC_DECPORT_SET_REG(p)	(SUNXI_SPC_BASE + 0x0004 + 0x10 * (p))
#define SUNXI_SPC_DECPORT_CLR_REG(p)	(SUNXI_SPC_BASE + 0x0008 + 0x10 * (p))
#define TZPC_DECPORT_STA_REG(x) (SUNXI_TZPC_BASE + 0x10 * (x) + 0x0)
#define TZPC_DECPORT_SET_REG(x) (SUNXI_TZPC_BASE + 0x10 * (x) + 0x4)
#define TZPC_DECPORT_CLR_REG(x) (SUNXI_TZPC_BASE + 0x10 * (x) + 0x8)

#define SUNXI_TIMER0_CLK_REG		(SUNXI_CCU_BASE + TIMER0_CLK_REG)
#define SUNXI_TIMER_BGR_REG		(SUNXI_CCU_BASE + TIMER_BGR_REG)
#define SUNXI_TIMER0_CLK_REG		(SUNXI_CCU_BASE + TIMER0_CLK_REG)

#define SUNXI_SPC_NUM_PORTS		14
#endif /* SUNXI_SPC_H */
