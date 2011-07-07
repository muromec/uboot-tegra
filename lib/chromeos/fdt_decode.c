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
#include <libfdt.h>
#include <chromeos/common.h>
#include <chromeos/fdt_decode.h>
#include <linux/string.h>

#define PREFIX "chromeos/fdt_decode: "

static int relpath_offset(const void *blob, int offset, const char *in_path)
{
	const char *path = in_path;
	const char *sep;

	/* skip leading '/' */
	while (*path == '/')
		path++;

	for (sep = path; *sep; path = sep + 1) {
		/* this is equivalent to strchrnul() */
		sep = strchr(path, '/');
		if (!sep)
			sep = path + strlen(path);

		offset = fdt_subnode_offset_namelen(blob, offset, path,
				sep - path);
		if (offset < 0) {
			VBDEBUG(PREFIX "Node '%s' is missing\n", in_path);
			return offset;
		}
	}

	return offset;
}

static int decode_fmap_entry(const void *blob, int offset, const char *base,
		const char *name, struct fdt_fmap_entry *entry)
{
	char path[50];
	int length;
	uint32_t *property;

	/* Form the node to look up as <base>-<name> */
	assert(strlen(base) + strlen(name) + 1 < sizeof(path));
	strcpy(path, base);
	strcat(path, "-");
	strcat(path, name);

	offset = relpath_offset(blob, offset, path);
	if (offset < 0)
		return offset;
	property = (uint32_t *)fdt_getprop(blob, offset, "reg", &length);
	if (!property) {
		VBDEBUG(PREFIX "Node '%s' is missing property '%s'\n",
			path, "reg");
		return -FDT_ERR_MISSING;
	}
	entry->offset = fdt32_to_cpu(property[0]);
	entry->length = fdt32_to_cpu(property[1]);

	return 0;
}

static int decode_block_lba(const void *blob, int offset, const char *path,
		uint64_t *out)
{
	int length;
	uint32_t *property;

	offset = relpath_offset(blob, offset, path);
	if (offset < 0)
		return offset;

	property = (uint32_t *)fdt_getprop(blob, offset, "block-lba", &length);
	if (!property) {
		VBDEBUG(PREFIX "failed to load LBA '%s/block-lba'\n", path);
		return -FDT_ERR_MISSING;
	}
	*out = fdt32_to_cpu(*property);
	return 0;
}

int decode_firmware_entry(const char *blob, int fmap_offset, const char *name,
		struct fdt_firmware_entry *entry)
{
	int err;

	err = decode_fmap_entry(blob, fmap_offset, name, "boot", &entry->boot);
	err |= decode_fmap_entry(blob, fmap_offset, name, "vblock",
			&entry->vblock);
	err |= decode_fmap_entry(blob, fmap_offset, name, "firmware-id",
			&entry->firmware_id);
	err |= decode_block_lba(blob, fmap_offset, name, &entry->block_lba);
	return err;
}

int fdt_decode_twostop_fmap(const void *blob, struct fdt_twostop_fmap *config)
{
	int fmap_offset;
	int err;

	fmap_offset = fdt_node_offset_by_compatible(blob, -1,
			"chromeos,flashmap");
	if (fmap_offset < 0) {
		VBDEBUG(PREFIX "chromeos,flashmap node is missing\n");
		return fmap_offset;
	}
	err = decode_firmware_entry(blob, fmap_offset, "rw-a",
			&config->readwrite_a);
	err |= decode_firmware_entry(blob, fmap_offset, "rw-b",
			&config->readwrite_b);

	err |= decode_fmap_entry(blob, fmap_offset, "ro", "fmap",
			&config->readonly.fmap); \
	err |= decode_fmap_entry(blob, fmap_offset, "ro", "gbb",
			&config->readonly.gbb); \
	err |= decode_fmap_entry(blob, fmap_offset, "ro", "firmware-id",
			&config->readonly.firmware_id);

	return 0;
}

void dump_entry(const char *path, struct fdt_fmap_entry *entry)
{
	VBDEBUG(PREFIX "%-20s %08x:%08x\n", path, entry->offset,
		entry->length);
}

void dump_firmware_entry(const char *name, struct fdt_firmware_entry *entry)
{
	VBDEBUG(PREFIX "%s\n", name);
	dump_entry("boot", &entry->boot);
	dump_entry("vblock", &entry->vblock);
	dump_entry("firmware_id", &entry->firmware_id);
	VBDEBUG(PREFIX "%-20s %08llx\n", "LBA", entry->block_lba);
}

void dump_fmap(struct fdt_twostop_fmap *config)
{
	VBDEBUG(PREFIX "rw-a:\n");
	dump_entry("fmap", &config->readonly.fmap);
	dump_entry("gbb", &config->readonly.gbb);
	dump_entry("firmware_id", &config->readonly.firmware_id);
	dump_firmware_entry("rw-a", &config->readwrite_a);
	dump_firmware_entry("rw-b", &config->readwrite_b);
}
