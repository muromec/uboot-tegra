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
#include <chromeos/common.h>
#include <chromeos/cros_gpio.h>
#include <vboot/global_data.h>

#include <gbb_header.h>
#include <vboot_api.h>

#define PREFIX		"global_data: "

DECLARE_GLOBAL_DATA_PTR;

/* The VBoot Global Data is stored in a region just above the stack region. */
#define VBGLOBAL_BASE	(CONFIG_STACKBASE + CONFIG_STACKSIZE)

vb_global_t *get_vboot_global(void)
{
	return (vb_global_t *)(VBGLOBAL_BASE);
}

/*
 * Loads the minimal required part of GBB from firmware, including:
 *  - header,
 *  - hwid (required by crossystem data,
 *  - rootkey (required by VbLoadFirmware() to verify firmware).
 */
static int load_minimal_gbb(firmware_storage_t *file, uint32_t source,
			    void *gbb_base)
{
	GoogleBinaryBlockHeader *gbb = (GoogleBinaryBlockHeader *)gbb_base;

	if (file->read(file, source, sizeof(*gbb), gbb)) {
		VBDEBUG(PREFIX "Failed to read GBB header!\n");
		return 1;
	}

	if (file->read(file, source + gbb->hwid_offset, gbb->hwid_size,
			gbb_base + gbb->hwid_offset)) {
		VBDEBUG(PREFIX "Failed to read HWID in GBB!\n");
		return 1;
	}

	if (file->read(file, source + gbb->rootkey_offset, gbb->rootkey_size,
			gbb_base + gbb->rootkey_offset)) {
		VBDEBUG(PREFIX "Failed to read Root Key in GBB!\n");
		return 1;
	}

	return 0;
}

/* Loads the BMP block in GBB from firmware. */
int load_bmpblk_in_gbb(vb_global_t *global, firmware_storage_t *file)
{
	void *fdt_ptr = (void *)gd->blob;
	struct fdt_twostop_fmap fmap;
	GoogleBinaryBlockHeader *gbb =
			(GoogleBinaryBlockHeader *)global->gbb_data;

	if (fdt_decode_twostop_fmap(fdt_ptr, &fmap)) {
		VBDEBUG(PREFIX "Failed to load fmap config from fdt!\n");
		return 1;
	}

	if (file->read(file,
			fmap.readonly.gbb.offset + gbb->bmpfv_offset,
			gbb->bmpfv_size,
			global->gbb_data + gbb->bmpfv_offset)) {
		VBDEBUG(PREFIX "Failed to read BMP Block in GBB!\n");
		return 1;
	}

	return 0;
}

/* Loads the recovery key in GBB from firmware. */
int load_reckey_in_gbb(vb_global_t *global, firmware_storage_t *file)
{
	void *fdt_ptr = (void *)gd->blob;
	struct fdt_twostop_fmap fmap;
	GoogleBinaryBlockHeader *gbb =
			(GoogleBinaryBlockHeader *)global->gbb_data;

	if (fdt_decode_twostop_fmap(fdt_ptr, &fmap)) {
		VBDEBUG(PREFIX "Failed to load fmap config from fdt!\n");
		return 1;
	}

	if (file->read(file,
			fmap.readonly.gbb.offset + gbb->recovery_key_offset,
			gbb->recovery_key_size,
			global->gbb_data + gbb->recovery_key_offset)) {
		VBDEBUG(PREFIX "Failed to read Recovery Key in GBB!\n");
		return 1;
	}

	return 0;
}

int init_vboot_global(vb_global_t *global, firmware_storage_t *file)
{
	void *fdt_ptr = (void *)gd->blob;
	cros_gpio_t wpsw, recsw, devsw;
	struct fdt_twostop_fmap fmap;
	char frid[ID_LEN];
	uint8_t nvraw[VBNV_BLOCK_SIZE];

	global->version = VBGLOBAL_VERSION;
	memcpy(global->signature, VBGLOBAL_SIGNATURE,
			VBGLOBAL_SIGNATURE_SIZE);

	/* Gets GPIO status */
	if (cros_gpio_fetch(CROS_GPIO_WPSW, fdt_ptr, &wpsw) ||
			cros_gpio_fetch(CROS_GPIO_RECSW, fdt_ptr, &recsw) ||
			cros_gpio_fetch(CROS_GPIO_DEVSW, fdt_ptr, &devsw)) {
		VBDEBUG(PREFIX "Failed to fetch GPIO!\n");
		return 1;
        }

	if (fdt_decode_twostop_fmap(fdt_ptr, &fmap)) {
		VBDEBUG(PREFIX "Failed to load fmap config from fdt!\n");
		return 1;
	}

	/* Loads a minimal required part of GBB from SPI */
	if (fmap.readonly.gbb.length > GBB_MAX_LENGTH) {
		VBDEBUG(PREFIX "The GBB size declared in FDT is too big!\n");
		return 1;
	}
	global->gbb_size = fmap.readonly.gbb.length;
	if (load_minimal_gbb(file,
			fmap.readonly.gbb.offset,
			global->gbb_data)) {
		VBDEBUG(PREFIX "Failed to read GBB!\n");
		return 1;
	}

	if (fmap.readonly.firmware_id.length > ID_LEN) {
		VBDEBUG(PREFIX "The FWID size declared in FDT is too big!\n");
		return 1;
	}
	if (file->read(file,
			fmap.readonly.firmware_id.offset,
			fmap.readonly.firmware_id.length,
			frid)) {
		VBDEBUG(PREFIX "Failed to read frid!\n");
		return 1;
	}

	if (VbExNvStorageRead(nvraw)) {
		VBDEBUG(PREFIX "Failed to read NvStorage!\n");
		return 1;
	}

	if (crossystem_data_init(&global->cdata_blob, frid,
			fmap.readonly.fmap.offset, global->gbb_data, nvraw,
			&wpsw, &recsw, &devsw)) {
		VBDEBUG(PREFIX "Failed to init crossystem data!\n");
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
