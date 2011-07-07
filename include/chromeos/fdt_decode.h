/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#ifndef CHROMEOS_FDT_DECODE_H_
#define CHROMEOS_FDT_DECODE_H_

/* Decode Chrome OS specific configuration from fdt */

struct fdt_fmap_entry {
	uint32_t offset;
	uint32_t length;
};

struct fdt_firmware_entry {
	struct fdt_fmap_entry boot;	/* U-Boot */
	struct fdt_fmap_entry vblock;
	struct fdt_fmap_entry firmware_id;
	uint64_t block_lba;
};

/*
 * Only sections that are used during booting are put here. More sections will
 * be added if required.
 */
struct fdt_twostop_fmap {
	struct {
		struct fdt_fmap_entry fmap;
		struct fdt_fmap_entry gbb;
		struct fdt_fmap_entry firmware_id;
	} readonly;

	struct fdt_firmware_entry readwrite_a;
	struct fdt_firmware_entry readwrite_b;
};

int fdt_decode_twostop_fmap(const void *fdt, struct fdt_twostop_fmap *config);

void dump_fmap(struct fdt_twostop_fmap *config);

#endif /* CHROMEOS_FDT_DECODE_H_ */
