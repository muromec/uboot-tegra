/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __configs_chromeos_seaboard_common_h__
#define __configs_chromeos_seaboard_common_h__

#include <configs/seaboard.h>

#define CONFIG_CHROMEOS

/*
 * Use the fdt to decide whether to load the environment early in start-up
 * (even before we decide if we're entering developer mode).
 */
#define CONFIG_OF_LOAD_ENVIRONMENT

/* for security reason, Chrome OS kernel must be loaded to specific location */
#define CONFIG_CHROMEOS_KERNEL_LOADADDR	0x00100000
#define CONFIG_CHROMEOS_KERNEL_BUFSIZE	0x00800000

/* graphics display */
#define CONFIG_LCD_BMP_RLE8
#define CONFIG_LZMA
#define CONFIG_SPLASH_SCREEN

/* TODO hard-coded mmc device number here */
#define DEVICE_TYPE			"mmc"
#define MMC_INTERNAL_DEVICE		0
#define MMC_EXTERNAL_DEVICE		1

#define CONFIG_OF_UPDATE_FDT_BEFORE_BOOT

/* load bootcmd from fdt */
#undef CONFIG_BOOTCOMMAND

/*
 * Use a smaller firmware image layout for Seaboard because it has
 * only 16MBit (=2MB) of SPI Flash.
 */

#define CONFIG_FIRMWARE_SIZE		0x00200000 /* 2 MB */

/* -- Region: Read-only ----------------------------------------------------- */

/* ---- Section: Read-only firmware ----------------------------------------- */

#define CONFIG_OFFSET_RO_SECTION	0x00000000
#define CONFIG_LENGTH_RO_SECTION	0x00100000

#define CONFIG_OFFSET_BOOT_STUB		0x00000000
#define CONFIG_LENGTH_BOOT_STUB		0x0008df00

#define CONFIG_OFFSET_RECOVERY		0x0008df00
#define CONFIG_LENGTH_RECOVERY		0x00000000

#define CONFIG_OFFSET_RO_DATA		0x0008df00
#define CONFIG_LENGTH_RO_DATA		0x00002000

#define CONFIG_OFFSET_RO_FRID		0x0008ff00
#define CONFIG_LENGTH_RO_FRID		0x00000100

#define CONFIG_OFFSET_GBB		0x00090000
#define CONFIG_LENGTH_GBB		0x00020000

#define CONFIG_OFFSET_FMAP		0x000d0000
#define CONFIG_LENGTH_FMAP		0x00000400

/* ---- Section: Vital-product data (VPD) ----------------------------------- */

#define CONFIG_OFFSET_RO_VPD		0x000c0000
#define CONFIG_LENGTH_RO_VPD		0x00010000

/* -- Region: Writable ------------------------------------------------------ */

/* ---- Section: Rewritable slot A ------------------------------------------ */

#define CONFIG_OFFSET_RW_SECTION_A	0x00100000
#define CONFIG_LENGTH_RW_SECTION_A	0x00080000

#define CONFIG_OFFSET_FW_MAIN_A		0x00100000
#define CONFIG_LENGTH_FW_MAIN_A		0x0007df00

#define CONFIG_OFFSET_VBLOCK_A		0x0017df00
#define CONFIG_LENGTH_VBLOCK_A		0x00002000

#define CONFIG_OFFSET_RW_FWID_A		0x0017ff00
#define CONFIG_LENGTH_RW_FWID_A		0x00000100

#define CONFIG_OFFSET_DATA_A		0x00100000
#define CONFIG_LENGTH_DATA_A		0x00000000

/* ---- Section: Rewritable slot B ------------------------------------------ */

#define CONFIG_OFFSET_RW_SECTION_B	0x00180000
#define CONFIG_LENGTH_RW_SECTION_B	0x00080000

#define CONFIG_OFFSET_FW_MAIN_B		0x00180000
#define CONFIG_LENGTH_FW_MAIN_B		0x0007df00

#define CONFIG_OFFSET_VBLOCK_B		0x001fdf00
#define CONFIG_LENGTH_VBLOCK_B		0x00002000

#define CONFIG_OFFSET_RW_FWID_B		0x001fff00
#define CONFIG_LENGTH_RW_FWID_B		0x00000100

#define CONFIG_OFFSET_DATA_B		0x00180000
#define CONFIG_LENGTH_DATA_B		0x00000000

/* ---- Section: Rewritable VPD --------------------------------------------- */

#define CONFIG_OFFSET_RW_VPD		0x00200000
#define CONFIG_LENGTH_RW_VPD		0x00000000

/* ---- Section: Rewritable shared ------------------------------------------ */

#define CONFIG_OFFSET_RW_SHARED		0x00200000
#define CONFIG_LENGTH_RW_SHARED		0x00000000

#define CONFIG_OFFSET_DEV_CFG		0x00200000
#define CONFIG_LENGTH_DEV_CFG		0x00000000

#define CONFIG_OFFSET_SHARED_DATA	0x00200000
#define CONFIG_LENGTH_SHARED_DATA	0x00000000

#define CONFIG_OFFSET_VBNVCONTEXT	0x00200000
#define CONFIG_LENGTH_VBNVCONTEXT	0x00000000

/* where are the meanings of these documented? Add a comment/link here */
#define CONFIG_OFFSET_ENV		0x00200000
#define CONFIG_LENGTH_ENV		0x00000000

#endif /* __configs_chromeos_seaboard_common_h__ */
