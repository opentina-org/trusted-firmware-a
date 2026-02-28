/*
 * Copyright (c) 2014, Allwinner Technology Co., Ltd. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SUNXI_COMMON_H
#define SUNXI_COMMON_H

#include <lib/utils_def.h>
#include <common/debug.h>

/* Combine @h and @l to a 64bit value */
#define MK_DWORD(h, l)		( (uint64_t)( ( ((uint64_t)(h)) << 32 ) | ( (l) & ~GENMASK(63, 32) ) ) )

typedef unsigned long phys_addr_t;  /* u64 */

void sunxi_dump(void *addr, unsigned int size);
#define hexdump		sunxi_dump

#endif /* SUNXI_COMMON_H */
