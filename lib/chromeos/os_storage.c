/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <common.h>
#include <malloc.h>
#include <mmc.h>
#include <part.h>
#include <usb.h>
#include <chromeos/common.h>
#include <chromeos/os_storage.h>
#include <linux/string.h> /* for strcmp */

#include <vboot_api.h>

#define PREFIX "os_storage: "

#define BACKUP_LBA_OFFSET 0x20

struct os_storage *global_os_storage;

static int os_storage_BootDeviceReadLBA(struct os_storage *oss,
		uint64_t lba_start, uint64_t lba_count, void *buffer);
static int os_storage_BootDeviceWriteLBA(struct os_storage *oss,
		uint64_t lba_start, uint64_t lba_count, const void *buffer);


block_dev_desc_t *os_storage_get_bootdev(struct os_storage *oss)
{
	return oss->dev_desc;
}

int os_storage_get_device_number(struct os_storage *oss)
{
	return oss->dev_desc->dev;
}

ulong os_storage_get_offset(struct os_storage *oss)
{
	return oss->offset;
}

ulong os_storage_get_limit(struct os_storage *oss)
{
	return oss->limit;
}

uint64_t os_storage_get_bytes_per_lba(struct os_storage *oss)
{
	if (!oss->dev_desc) {
		VBDEBUG(PREFIX "get_bytes_per_lba: not dev_desc set\n");
		return ~0ULL;
	}

	return (uint64_t) oss->dev_desc->blksz;
}

uint64_t os_storage_get_ending_lba(struct os_storage *oss)
{
	uint8_t static_buf[512];
	uint8_t *buf = NULL;
	uint64_t ret = ~0ULL, bytes_per_lba = ~0ULL;

	bytes_per_lba = os_storage_get_bytes_per_lba(oss);
	if (bytes_per_lba == ~0ULL) {
		VBDEBUG(PREFIX "get_ending_lba: get bytes_per_lba fail\n");
		goto EXIT;
	}

	if (bytes_per_lba > sizeof(static_buf))
		buf = malloc(bytes_per_lba);
	else
		buf = static_buf;

	if (os_storage_BootDeviceReadLBA(oss, 1, 1, buf)) {
		VBDEBUG(PREFIX "get_ending_lba: read primary GPT header fail\n");
		goto EXIT;
	}

	ret = *(uint64_t*) (buf + BACKUP_LBA_OFFSET);

EXIT:
	if (buf != static_buf)
		free(buf);

	return ret;
}

int os_storage_set_bootdev(struct os_storage *oss, char *ifname, int dev,
			   int part)
{
	disk_partition_t part_info;

	global_os_storage = oss;
	if ((oss->dev_desc = get_dev(ifname, dev)) == NULL) {
		VBDEBUG(PREFIX "block device not supported\n");
		goto cleanup;
	}

	if (part == 0) {
		oss->offset = 0;
		oss->limit = oss->dev_desc->lba;
		return 0;
	}

	if (get_partition_info(oss->dev_desc, part, &part_info)) {
		VBDEBUG(PREFIX "cannot find partition\n");
		goto cleanup;
	}

	oss->offset = part_info.start;
	oss->limit = part_info.size;
	return 0;

cleanup:
	oss->dev_desc = NULL;
	oss->offset = 0;
	oss->limit = 0;

	return 1;
}

static int os_storage_BootDeviceReadLBA(struct os_storage *oss,
		uint64_t lba_start, uint64_t lba_count, void *buffer)
{
	block_dev_desc_t *dev_desc;

	if ((dev_desc = oss->dev_desc) == NULL)
		return 1; /* No boot device configured */

	if (lba_start + lba_count > oss->limit)
		return 1; /* read out of range */

	if (lba_count != dev_desc->block_read(dev_desc->dev,
				oss->offset + lba_start, lba_count,
				buffer))
		return 1; /* error reading blocks */

	return 0;
}

static int os_storage_BootDeviceWriteLBA(struct os_storage *oss,
		uint64_t lba_start, uint64_t lba_count, const void *buffer)
{
	block_dev_desc_t *dev_desc;

	if ((dev_desc = oss->dev_desc) == NULL)
		return 1; /* No boot device configured */

	if (lba_start + lba_count > oss->limit)
		return 1; /* read out of range */

	if (lba_count != dev_desc->block_write(dev_desc->dev,
				oss->offset + lba_start, lba_count,
				buffer))
		return 1; /* error reading blocks */

	return 0;
}

int os_storage_initialize_mmc_device(struct os_storage *oss, int dev)
{
	struct mmc *mmc;
	int err;

	mmc = find_mmc_device(dev);
	if (!mmc) {
		VBDEBUG(PREFIX "no mmc device found for dev %d\n", dev);
		return -1;
	}

	if ((err = mmc_init(mmc))) {
		VBDEBUG(PREFIX "mmc_init() failed: %d\n", err);
		return -1;
	}

	return 0;
}

/* For vboot (pre re-factor) */
int BootDeviceReadLBA(uint64_t lba_start, uint64_t lba_count, void *buffer)
{
	return os_storage_BootDeviceReadLBA(global_os_storage, lba_start,
			lba_count, buffer);
}

int BootDeviceWriteLBA(uint64_t lba_start, uint64_t lba_count,
		const void *buffer)
{
	return os_storage_BootDeviceWriteLBA(global_os_storage, lba_start,
			lba_count, buffer);
}

int os_storage_is_mmc_storage_present(struct os_storage *oss,
		int mmc_device_number)
{
	/*
	 * mmc_init tests the SD card's version and tries its operating
	 * condition. It should be the minimally required code for probing
	 * an SD card.
	 */
	struct mmc *mmc = find_mmc_device(mmc_device_number);
	return mmc != NULL && mmc_init(mmc) == 0;
}

int os_storage_is_usb_storage_present(struct os_storage *oss)
{
	/* TODO(waihong): Better way to probe USB than restart it */
	usb_stop();
	if (usb_init() >= 0) {
		/* Scanning bus for storage devices, mode = 1. */
		return usb_stor_scan(1) == 0;
	}

	return 1;
}

int os_storage_is_any_storage_device_plugged(struct os_storage *oss,
		int boot_probed_device)
{
	VBDEBUG(PREFIX "probe external mmc\n");
	if (os_storage_is_mmc_storage_present(oss, MMC_EXTERNAL_DEVICE)) {
		if (boot_probed_device == NOT_BOOT_PROBED_DEVICE) {
			VBDEBUG(PREFIX "probed external mmc\n");
			return 1;
		}

		VBDEBUG(PREFIX "try to set boot device to external mmc\n");
		if (!os_storage_initialize_mmc_device(oss,
					MMC_EXTERNAL_DEVICE) &&
				!os_storage_set_bootdev(oss, "mmc",
						MMC_EXTERNAL_DEVICE, 0)) {
			VBDEBUG(PREFIX "set external mmc as boot device\n");
			return 1;
		}
	}

	VBDEBUG(PREFIX "probe usb\n");
	if (os_storage_is_usb_storage_present(oss)) {
		if (boot_probed_device == NOT_BOOT_PROBED_DEVICE) {
			VBDEBUG(PREFIX "probed usb\n");
			return 1;
		}

		VBDEBUG(PREFIX "try to set boot device to usb\n");
		if (!os_storage_set_bootdev(oss, "usb", 0, 0)) {
			VBDEBUG(PREFIX "set usb as boot device\n");
			return 1;
		}
	}

	VBDEBUG(PREFIX "found nothing\n");
	return 0;
}
