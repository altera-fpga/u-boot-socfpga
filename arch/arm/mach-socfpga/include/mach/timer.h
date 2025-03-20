/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *  Copyright (C) 2012 Altera Corporation <www.altera.com>
 */

#ifndef _SOCFPGA_TIMER_H_
#define _SOCFPGA_TIMER_H_

#include <asm/barriers.h>
#include <div64.h>

struct socfpga_timer {
	u32	load_val;
	u32	curr_val;
	u32	ctrl;
	u32	eoi;
	u32	int_stat;
};

static __always_inline u64 __socfpga_get_time_stamp(void)
{
	u64 cntpct;

	isb();
	asm volatile("mrs %0, cntpct_el0" : "=r" (cntpct));
	return cntpct;
}

static __always_inline u64 __socfpga_usec_to_tick(u64 usec)
{
	u64 tick = usec;
	u64 cntfrq;

	asm volatile("mrs %0, cntfrq_el0" : "=r" (cntfrq));
	tick *= cntfrq;
	do_div(tick, 1000000);
	return tick;
}

static __always_inline void __socfpga_udelay(u64 usec)
{
	u64 tmp = __socfpga_get_time_stamp() + __socfpga_usec_to_tick(usec);

	while (__socfpga_get_time_stamp() < tmp + 1)
		;
}

#endif
