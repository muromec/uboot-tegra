/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#ifndef CHROMEOS_OS_STORAGE_H_
#define CHROMEOS_OS_STORAGE_H_

#include <linux/types.h>
#include <part.h>

/* Information about the current OS storage device */
struct os_storage {
	block_dev_desc_t *dev_desc;
	ulong offset, limit;
};

/* Set boot device.
 *
 * Set partition number in argument part (starting from 1).  Pass part=0 for
 * the entire device.
 * Return zero if succees and non-zero if error.
 */
int os_storage_set_bootdev(struct os_storage *oss, char *ifname, int dev,
		int part);

/* Return block device descriptor that was set by calls to set_bootdev(). */
block_dev_desc_t *os_storage_get_bootdev(struct os_storage *oss);

/* Return the device number */
int os_storage_get_device_number(struct os_storage *oss);

/* Return the offset (in blocks) of the partition or zero if part=0. */
ulong os_storage_get_offset(struct os_storage *oss);

/* Return the size (in blocks) of the partition or entire device if part=0. */
ulong os_storage_get_limit(struct os_storage *oss);

uint64_t os_storage_get_bytes_per_lba(struct os_storage *oss);
uint64_t os_storage_get_ending_lba(struct os_storage *oss);

int os_storage_initialize_mmc_device(struct os_storage *oss, int dev);

/* Probe whether external storage device presents */
int os_storage_is_mmc_storage_present(struct os_storage *oss,
		int mmc_device_number);
int os_storage_is_usb_storage_present(struct os_storage *oss);

/*
 * When BOOT_PROBED_DEVICE is passed to is_any_storage_device_plugged(), the
 * function will also call set_bootdev() on the successfully-probed device, and
 * returns true if set_bootdev() also succeeds.
 *
 * When NOT_BOOT_PROBED_DEVICE is passed to is_any_storage_device_plugged(), the
 * function simply returns true after a storage device is successfully probed.
 */
enum {
	NOT_BOOT_PROBED_DEVICE,
	BOOT_PROBED_DEVICE
};

int os_storage_is_any_storage_device_plugged(struct os_storage *oss,
		int boot_probed_device);

#endif /* CHROMEOS_OS_STORAGE_H_ */
