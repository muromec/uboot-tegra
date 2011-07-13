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
#include <fdt_decode.h>
#include <vboot_api.h>
#include <chromeos/gpio.h>
#include <vboot/global_data.h>

#define PREFIX		"global_data: "

DECLARE_GLOBAL_DATA_PTR;

/* The VBoot Global Data is stored in a region just above the stack region. */
#define VBGLOBAL_BASE	(CONFIG_STACKBASE + CONFIG_STACKSIZE)

vb_global_t *get_vboot_global(void)
{
	return (vb_global_t *)(VBGLOBAL_BASE);
}

int init_vboot_global(vb_global_t *global, firmware_storage_t *file)
{
	void *fdt_ptr = (void *)gd->blob;
	int polarity_write_protect_sw, polarity_recovery_sw,
	    polarity_developer_sw;
	int write_protect_sw, recovery_sw, developer_sw;
	struct fdt_twostop_fmap fmap;
	char frid[ID_LEN];
	uint8_t nvraw[VBNV_BLOCK_SIZE];

	global->version = VBGLOBAL_VERSION;
	memcpy(global->signature, VBGLOBAL_SIGNATURE,
			VBGLOBAL_SIGNATURE_SIZE);

	/* Get GPIO status */
	polarity_write_protect_sw = fdt_decode_get_config_int(fdt_ptr,
			"polarity_write_protect_switch", GPIO_ACTIVE_HIGH);
	polarity_recovery_sw = fdt_decode_get_config_int(fdt_ptr,
			"polarity_recovery_switch", GPIO_ACTIVE_HIGH);
	polarity_developer_sw = fdt_decode_get_config_int(fdt_ptr,
			"polarity_developer_switch", GPIO_ACTIVE_HIGH);

	write_protect_sw = is_firmware_write_protect_gpio_asserted(
			polarity_write_protect_sw);
	recovery_sw = is_recovery_mode_gpio_asserted(
			polarity_recovery_sw);
	developer_sw = is_developer_mode_gpio_asserted(
			polarity_developer_sw);

	if (fdt_decode_twostop_fmap(fdt_ptr, &fmap)) {
		VbExDebug(PREFIX "Failed to load fmap config from fdt!\n");
		return 1;
	}

	/* Load GBB from SPI */
	if (fmap.readonly.gbb.length > CONFIG_LENGTH_GBB) {
		VbExDebug(PREFIX "The GBB size declared in FDT is too big!\n");
		return 1;
	}
	global->gbb_size = fmap.readonly.gbb.length;
	if (file->read(file,
			fmap.readonly.gbb.offset,
			fmap.readonly.gbb.length,
			global->gbb_data)) {
		VbExDebug(PREFIX "Failed to read GBB!\n");
		return 1;
	}

	if (fmap.onestop_layout.fwid.length > ID_LEN) {
		VbExDebug(PREFIX "The FWID size declared in FDT is too big!\n");
		return 1;
	}
	if (file->read(file,
			fmap.onestop_layout.fwid.offset,
			fmap.onestop_layout.fwid.length,
			frid)) {
		VbExDebug(PREFIX "Failed to read frid!\n");
		return 1;
	}

	if (VbExNvStorageRead(nvraw)) {
		VbExDebug(PREFIX "Failed to read NvStorage!\n");
		return 1;
	}

	if (crossystem_data_init(&global->cdata_blob, fdt_ptr, frid,
			fmap.readonly.fmap.offset, global->gbb_data, nvraw,
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
