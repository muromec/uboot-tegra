/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

/*
 * VBoot Global Data keeps the common data which can be shared between
 * different firmware variants, like the crossystem data between bootstub,
 * normal/developer firmware, and kernel, the GBB data between bootstub and
 * recovery/developer firmware. This data is stored in a fixed memory
 * address, a region just above the stack region, such that u-boot
 * with differnet symbol table can also locates it.
 */

#ifndef VBOOT_GLOBAL_DATA_H_
#define VBOOT_GLOBAL_DATA_H_

#include <common.h>
#include <chromeos/firmware_storage.h>
#include <chromeos/crossystem_data.h>
#include <vboot_struct.h>

#define VBGLOBAL_SIGNATURE	"VBGLOBAL"
#define VBGLOBAL_SIGNATURE_SIZE	8
#define VBGLOBAL_VERSION	1
#define GBB_MAX_LENGTH		0x20000

typedef struct {
	uint8_t			signature[VBGLOBAL_SIGNATURE_SIZE];
	uint32_t		version;
	uint32_t		gbb_size;
	uint8_t			gbb_data[GBB_MAX_LENGTH];
	uint32_t		cdata_size;
	crossystem_data_t	cdata_blob;
} vb_global_t;

/* Get vboot global data pointer. */
vb_global_t *get_vboot_global(void);

/* Initialize vboot global data. It also loads the GBB data from SPI. */
int init_vboot_global(vb_global_t *global, firmware_storage_t *file);

/* Check if vboot global data valid or not. */
int is_vboot_global_valid(vb_global_t *global);

#endif /* VBOOT_GLOBAL_DATA_H */
