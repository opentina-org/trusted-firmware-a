/*
 * Copyright (c) 2014, Allwinner Technology Co., Ltd. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <lib/mmio.h>
#include <sunxi_mmap.h>
#include <sunxi_ccu.h>
#include <stdio.h>
#include <drivers/ti/uart/uart_16550.h>
#include <sunxi_def.h>
#include <platform_def.h>
#include <common/debug.h>

extern console_t console;

#define UARTHALT		0xa4
#define	UARTHALT_AT_BUSY	0x2
void sunxi_uart_init(void)
{
	uint32_t i, val = mmio_read_32(SUNXI_UART_BGR_REG);
	static int32_t sclk = -1;
	static int32_t baudrate = -1;
	static int32_t uart_port = -1;
	uint32_t  uart_base = 0;

	/* No need to query uart_port when calling this function again. For static uart_port */
	if (uart_port == -1) {
		// /* Try uart port by uboot head info */
		// if (monitor_head.nboot_base != 0) {
		// 	/* Try uart port by uboot head info */
		// 	uboot_head_t* uboot_hdr = (uboot_head_t*)(uintptr_t)monitor_head.nboot_base;
		// 	uart_port = uboot_hdr->boot_data.uart_port;
		// }

		/* Check uart_port is valid */
		if (uart_port < 0 || uart_port > PLATFORM_UART_MAX_NUM) {
			uart_port = -1; /* fail */
		} else {
			val = mmio_read_32((SUNXI_UART_BGR_REG + (uart_port * 4)));
			if ((val & 0x1) == 0)
				uart_port = -1; /* fail */
		}
	}

	if (uart_port == -1) {
		/* Try uart port by ccu uart registers */
		while (uart_port <= PLATFORM_UART_MAX_NUM) {
			uart_port++;
			val = mmio_read_32((SUNXI_UART_BGR_REG + (uart_port * 4)));
			if (val & 0x1) {
				break;
			} else if (uart_port == PLATFORM_UART_MAX_NUM) {
				/* If all UART_BGR are not enabled, then enable UART0 */
				uart_port = 0;
			}
		}
	}
	uart_base = (SUNXI_UART0_BASE + uart_port * (SUNXI_UART1_BASE - SUNXI_UART0_BASE));

	if (baudrate == -1) {
		/* adaptive select sclk and baudrate */
		uint32_t clk_src, factor_m, divisor;

		/* halt uart for set baudrate */
		val = mmio_read_32(uart_base + UARTHALT);
		val |= UARTHALT_AT_BUSY;
		mmio_write_32(uart_base + UARTHALT, val);

		clk_src = mmio_read_32(SUNXI_UART_CLK_REG) >> APB_UART_CLK_REG_CLK_SRC_SEL_OFFSET;
		factor_m = (mmio_read_32(SUNXI_UART_CLK_REG) & APB_UART_CLK_REG_FACTOR_M_CLEAR_MASK) + 1;

		val = mmio_read_32(uart_base + UARTLCR);
		val |= UARTLCR_DLAB;
		mmio_write_32(uart_base + UARTLCR, val);
		divisor = (mmio_read_32(uart_base + UARTDLL) & 0xFF) | (mmio_read_32(uart_base + UARTDLLM) & 0xFF) << 8;
		val &= ~UARTLCR_DLAB;
		mmio_write_32(uart_base + UARTLCR, val);

		val = mmio_read_32(uart_base + UARTHALT);
		val &= ~UARTHALT_AT_BUSY;
		mmio_write_32(uart_base + UARTHALT, val);

		if (clk_src == SUNXI_UART_CLK_SRC_SEL_HOSC) {
			sclk = 24000000 / factor_m;
			baudrate = sclk / (16 * divisor);
			if (baudrate == 115384)
				baudrate = 115200;
		} else if (clk_src == SUNXI_UART_CLK_SRC_SEL_PERI0_600M_BUS) {
			sclk = 600000000 / factor_m;
			baudrate = sclk / (16 * divisor);
		} else if (clk_src == SUNXI_UART_CLK_SRC_SEL_PERI0_480M_BUS) {
			sclk = 480000000 / factor_m;
			baudrate = sclk / (16 * divisor);
		} else {
			sclk = SUNXI_UART0_CLK_IN_HZ;
			baudrate = SUNXI_UART0_BAUDRATE;
		}
	}

	/* reset */
	uint32_t uart_bgr_reg = SUNXI_UART_BGR_REG + (uart_port * 4);

	val = mmio_read_32(uart_bgr_reg);
	val &= ~(1 << (SUNXI_RST_OFFSET));
	mmio_write_32(uart_bgr_reg, val);
	for (i = 0; i < 100; i++)
		;
	val |= (1 << (SUNXI_RST_OFFSET));
	mmio_write_32(uart_bgr_reg, val);

	/* gate */
	val = mmio_read_32(uart_bgr_reg);
	val &= ~(1 << (SUNXI_GATING_OFFSET));
	mmio_write_32(uart_bgr_reg, val);
	for (i = 0; i < 100; i++)
		;
	val |= (1 << (SUNXI_GATING_OFFSET));
	mmio_write_32(uart_bgr_reg, val);

	/* Initialize the debug console as soon as possible */
	console_16550_register(uart_base, sclk, baudrate, &console);

	NOTICE("uart_init uart_port: %d\n", uart_port);
}