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
#include <vboot_api.h>
#include <chromeos/gpio.h>
#include <vboot/global_data.h>

#define PREFIX		"global_data: "

/* The VBoot Global Data is stored in a region just above the stack region. */
#define VBGLOBAL_BASE	(CONFIG_STACKBASE + CONFIG_STACKSIZE)

vb_global_t *get_vboot_global(void)
{
	return (vb_global_t *)(VBGLOBAL_BASE);
}

int init_vboot_global(vb_global_t *global, firmware_storage_t *file)
{
	char frid[ID_LEN];
	uint8_t nvraw[VBNV_BLOCK_SIZE];
	int write_protect_sw, recovery_sw, developer_sw;

	global->version = VBGLOBAL_VERSION;
	memcpy(global->signature, VBGLOBAL_SIGNATURE,
			VBGLOBAL_SIGNATURE_SIZE);

	/* Load GBB from SPI */
	global->gbb_size = CONFIG_LENGTH_GBB;
	if (firmware_storage_read(file, CONFIG_OFFSET_GBB,
			CONFIG_LENGTH_GBB, global->gbb_data)) {
		VbExDebug(PREFIX "Failed to read GBB!\n");
		return 1;
	}

	/* Get GPIO status */
	write_protect_sw = is_firmware_write_protect_gpio_asserted();
	recovery_sw = is_recovery_mode_gpio_asserted();
	developer_sw = is_developer_mode_gpio_asserted();

	if (firmware_storage_read(file, CONFIG_OFFSET_RO_FRID,
			CONFIG_LENGTH_RO_FRID, frid)) {
		VbExDebug(PREFIX "Failed to read frid!\n");
		return 1;
	}

	if (VbExNvStorageRead(nvraw)) {
		VbExDebug(PREFIX "Failed to read NvStorage!\n");
		return 1;
	}

	if (crossystem_data_init(&global->cdata_blob, frid,
			CONFIG_OFFSET_FMAP, global->gbb_data, nvraw,
			write_protect_sw, recovery_sw, developer_sw)) {
		VbExDebug(PREFIX "Failed to init crossystem data!\n");
		return 1;
	}

	global->cdata_size = sizeof(crossystem_data_t);

	return 0;
}

int is_vboot_global_valid(vb_global_t *global)
{
	return (global && memcmp(global->signature,
		VBGLOBAL_SIGNATURE, VBGLOBAL_SIGNATURE_SIZE) == 0);
}
