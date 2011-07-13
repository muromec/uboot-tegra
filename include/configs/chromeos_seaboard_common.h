/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __configs_chromeos_seaboard_common_h__
#define __configs_chromeos_seaboard_common_h__

#include <configs/seaboard.h>

#define CONFIG_CHROMEOS

/* don't load the environment until we enter developer mode */
#define CONFIG_DELAY_ENVIRONMENT

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

#endif /* __configs_chromeos_seaboard_common_h__ */
