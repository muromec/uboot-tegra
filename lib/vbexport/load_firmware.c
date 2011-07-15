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
#include <chromeos/firmware_storage.h>
#include <vboot/firmware_cache.h>

/* Import the header files from vboot_reference. */
#include <vboot_api.h>

#define PREFIX			"load_firmware: "

/*
 * Amount of bytes we read each time we call read().
 * See the commit log for more detail about the benchmark results.
 */
#define BLOCK_SIZE		(8 * 1024)

VbError_t VbExHashFirmwareBody(VbCommonParams* cparams,
			       uint32_t firmware_index)
{
	firmware_cache_t *cache = (firmware_cache_t *)cparams->caller_context;
	firmware_storage_t *file = cache->file;
	firmware_info_t *info;
	int index;
	uint8_t *buffer;
	size_t offset, leftover, n;

	if (firmware_index != VB_SELECT_FIRMWARE_A &&
			firmware_index != VB_SELECT_FIRMWARE_B) {
		VbExDebug(PREFIX "Incorrect firmware index: %lu\n",
				firmware_index);
		return 1;
	}

	index = (firmware_index == VB_SELECT_FIRMWARE_A ? 0 : 1);
	info = &cache->infos[index];
	buffer = info->buffer;
	offset = info->offset;

	/*
	 * This loop feeds firmware body into VbUpdateFirmwareBodyHash.
	 */
	for (leftover = info->size;
			leftover > 0;
			leftover -= n, offset += n, buffer += n) {
		n = min(BLOCK_SIZE, leftover);
		if (file->read(file, offset, n, buffer)) {
			VbExDebug(PREFIX "an error has occured "
					"while reading firmware");
			return 1;
		}

		VbUpdateFirmwareBodyHash(cparams, buffer, n);
	}

	return 0;
}
