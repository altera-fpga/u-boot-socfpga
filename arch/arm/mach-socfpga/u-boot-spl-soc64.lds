/*
 * (C) Copyright 2018
 * Intel Corporation <www.intel.com>
 *
 * (C) Copyright 2013
 * David Feng <fenghua@phytium.com.cn>
 *
 * (C) Copyright 2002
 * Gary Jennejohn, DENX Software Engineering, <garyj@denx.de>
 *
 * (C) Copyright 2010
 * Texas Instruments, <www.ti.com>
 *	Aneesh V <aneesh@ti.com>
 *
 * SPDX-License-Identifier: GPL-2.0
 */

MEMORY { .sram : ORIGIN = CONFIG_SPL_TEXT_BASE,
		LENGTH = CONFIG_SPL_MAX_SIZE }
MEMORY { .sdram : ORIGIN = CONFIG_SPL_BSS_START_ADDR,
		LENGTH = CONFIG_SPL_BSS_MAX_SIZE }

OUTPUT_FORMAT("elf64-littleaarch64", "elf64-littleaarch64", "elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)
SECTIONS
{
	.text : {
		. = ALIGN(8);
		__image_copy_start = .;
		CPUDIR/start.o (.text*)
		*(.text*)
	} >.sram

	.rodata : {
		. = ALIGN(8);
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.rodata*)))
	} >.sram

	.data : {
		. = ALIGN(8);
		/* Run time .data section starting at this location */
		__data_start = .;
		*(.data*)
		/* Run time .data section ending at this location */
		__data_end = .;
	} >.sram

	. = ALIGN(8);
	/* Preserve original .data section starting at this location */
	__preserve_data_start = .;
	. = __preserve_data_start + (__data_end - __data_start);
	/* Preserve original .data section ending at this location */
	__preserve_data_end = .;

	. = ALIGN(8);
	__u_boot_list . : {
		KEEP(*(SORT(__u_boot_list*)));
	} >.sram

	.image_copy_end : {
		. = ALIGN(8);
		*(.__image_copy_end)
	} >.sram

	_end = .;
	_image_binary_end = .;

	.bss : {
		__bss_start = .;
		*(.bss*)
		. = ALIGN(8);
		__bss_end = .;
	} >.sdram

	/DISCARD/ : { *(.dynsym) }
	/DISCARD/ : { *(.dynstr*) }
	/DISCARD/ : { *(.dynamic*) }
	/DISCARD/ : { *(.plt*) }
	/DISCARD/ : { *(.interp*) }
	/DISCARD/ : { *(.gnu*) }
}
