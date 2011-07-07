/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#ifndef CHROMEOS_CMDLINE_UPDATER_H_
#define CHROMEOS_CMDLINE_UPDATER_H_

#include <linux/types.h>

/**
 * This replaces:
 *   %D -> device number
 *   %P -> partition number
 *   %U -> GUID
 * in kernel command line.
 *
 * For example:
 *   ("root=/dev/sd%D%P", 2, 3)      -> "root=/dev/sdc3"
 *   ("root=/dev/mmcblk%Dp%P", 0, 5) -> "root=/dev/mmcblk0p5".
 *
 * @param src - input string
 * @param devnum - device number of the storage device we will mount
 * @param partnum - partition number of the root file system we will mount
 * @param guid - guid of the kernel partition
 * @param dst - output string; a copy of [src] with special characters replaced
 */
void update_cmdline(char *src, int devnum, int partnum, uint8_t *guid,
		char *dst);

#endif /* CHROMEOS_CMDLINE_UPDATER_H_ */
