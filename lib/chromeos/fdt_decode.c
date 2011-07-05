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

static int fdt_decode_fmap_entry(const void *blob, int offset, const char *path,
		struct fdt_fmap_entry *config)
{
	const char *sep;
	int length;
	uint32_t *property;

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
		if (offset < 0)
			return offset;
	}

	property = (uint32_t *)fdt_getprop(blob, offset, "reg", &length);
	if (!property)
		return length;

	config->offset = fdt32_to_cpu(property[0]);
	config->length = fdt32_to_cpu(property[1]);

	return 0;
}

int fdt_decode_onestop_fmap(const void *blob, struct fdt_onestop_fmap *config)
{
	int fmap_offset, offset;

	fmap_offset = fdt_node_offset_by_compatible(blob, -1,
			"chromeos,flashmap");
	if (fmap_offset < 0)
		return fmap_offset;

	offset = fdt_decode_fmap_entry(blob, fmap_offset,
			"/onestop-layout/firmware-image",
			&config->onestop_layout.fwbody);
	if (offset < 0)
		return offset;

	offset = fdt_decode_fmap_entry(blob, fmap_offset,
			"/onestop-layout/verification-block",
			&config->onestop_layout.vblock);
	if (offset < 0)
		return offset;

	offset = fdt_decode_fmap_entry(blob, fmap_offset,
			"/onestop-layout/firmware-id",
			&config->onestop_layout.fwid);
	if (offset < 0)
		return offset;

	offset = fdt_decode_fmap_entry(blob, fmap_offset,
			"/readonly/gbb", &config->readonly.gbb);
	if (offset < 0)
		return offset;

	offset = fdt_decode_fmap_entry(blob, fmap_offset,
			"/readonly/fmap", &config->readonly.fmap);
	if (offset < 0)
		return offset;

	offset = fdt_decode_fmap_entry(blob, fmap_offset,
			"/readwrite-a", &config->readwrite_a);
	if (offset < 0)
		return offset;

	offset = fdt_decode_fmap_entry(blob, fmap_offset,
			"/readwrite-b", &config->readwrite_b);
	if (offset < 0)
		return offset;

	return 0;
}

static void dump_fmap_entry(struct fdt_fmap_entry *e, const char *name)
{
	VBDEBUG(PREFIX "%-20s %08x:%08x\n", name, e->offset, e->length);
}

void dump_fmap(struct fdt_onestop_fmap *config)
{
	dump_fmap_entry(&config->onestop_layout.fwbody, "fwbody");
	dump_fmap_entry(&config->onestop_layout.vblock, "vblock");
	dump_fmap_entry(&config->onestop_layout.fwid, "fwid");
	dump_fmap_entry(&config->readonly.fmap, "fmap");
	dump_fmap_entry(&config->readonly.gbb, "gbb");
	dump_fmap_entry(&config->readwrite_a, "readwrite_a");
	dump_fmap_entry(&config->readwrite_b, "readwrite_b");
}
