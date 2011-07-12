/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#ifndef CHROMEOS_ONESTOP_H_
#define CHROMEOS_ONESTOP_H_

#include <vboot_nvstorage.h>
#include <chromeos/crossystem_data.h>
#include <chromeos/fdt_decode.h>
#include <chromeos/firmware_storage.h>

/*
 * Although second-stage firmware does not trust anything first-stage firmware
 * tells to it because TPM says so. There are certain data that second-stage
 * firmware cannot acquire by itself, and has to rely on first-stage firmware.
 * Specifically, these data are whether crossystem data, such as active main
 * firmware (A or B).
 */
#define CROSSYSTEM_DATA_ADDRESS 0x00100000

int is_ro_firmware(void);

int is_tpm_trust_ro_firmware(void);

uint32_t boot_rw_firmware(firmware_storage_t *file,
		struct fdt_twostop_fmap *fmap, int dev_mode,
		uint8_t *gbb_data, crossystem_data_t *cdata,
		VbNvContext *nvcxt);

#endif /* CHROMEOS_ONESTOP_H_ */
