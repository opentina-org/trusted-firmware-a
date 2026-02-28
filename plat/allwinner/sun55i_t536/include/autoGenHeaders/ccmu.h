/*
 * Copyright (c) 2014, Allwinner Technology Co., Ltd. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define CCMU_SEC_SWITCH_REG			0x00001f00
#define TIMER0_CLK_REG				    0x00000800 //TIMER0 Clock Register
#define TIMER_BGR_REG			      0x00000850 //TIMER Bus Gating Reset Register
#define UART0_BGR_REG			      0x00000e00 //UART0 Bus Gating Reset Register
#define APB_UART_CLK_REG			    0x00000538 //APB_UART Clock Register
#define APB_UART_CLK_REG_CLK_SRC_SEL_OFFSET	    24
#define APB_UART_CLK_REG_CLK_SRC_SEL_CLEAR_MASK	    0x07000000
#define APB_UART_CLK_REG_CLK_SRC_SEL_HOSC	    0b000
#define APB_UART_CLK_REG_CLK_SRC_SEL_CLK32K	    0b001
#define APB_UART_CLK_REG_CLK_SRC_SEL_CLK16M_RC	    0b010
#define APB_UART_CLK_REG_CLK_SRC_SEL_PERI0_600M_BUS 0b011
#define APB_UART_CLK_REG_CLK_SRC_SEL_PERI0_480M_BUS 0b100
#define APB_UART_CLK_REG_FACTOR_M_OFFSET	    0
#define APB_UART_CLK_REG_FACTOR_M_CLEAR_MASK	    0x0000001f