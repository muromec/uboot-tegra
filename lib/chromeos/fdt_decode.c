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

#define GET_ENTRY(path, entry) \
	offset = fdt_decode_fmap_entry(blob, fmap_offset, path, \
			&config->entry); \
	if (offset < 0) \
		return offset;

	GET_ENTRY("/onestop-layout", onestop_layout.onestop_layout);
	GET_ENTRY("/onestop-layout/firmware-image", onestop_layout.fwbody);
	GET_ENTRY("/onestop-layout/verification-block", onestop_layout.vblock);
	GET_ENTRY("/onestop-layout/firmware-id", onestop_layout.fwid);
	GET_ENTRY("/readonly", readonly.readonly);
	GET_ENTRY("/readonly/gbb", readonly.gbb);
	GET_ENTRY("/readonly/fmap", readonly.fmap);
	GET_ENTRY("/readwrite-a", readwrite_a);
	GET_ENTRY("/readwrite-b", readwrite_b);

#undef GET_ENTRY

	return 0;
}

void dump_fmap(struct fdt_onestop_fmap *config)
{
#define PRINT_ENTRY(entry, tag) \
	VBDEBUG(PREFIX "%-20s %08x:%08x\n", tag, \
			config->entry.offset, config->entry.length)

	PRINT_ENTRY(onestop_layout.onestop_layout, "onestop_layout");
	PRINT_ENTRY(onestop_layout.fwbody, "fwbody");
	PRINT_ENTRY(onestop_layout.vblock, "vblock");
	PRINT_ENTRY(onestop_layout.fwid, "fwid");
	PRINT_ENTRY(readonly.readonly, "readonly");
	PRINT_ENTRY(readonly.fmap, "fmap");
	PRINT_ENTRY(readonly.gbb, "gbb");
	PRINT_ENTRY(readwrite_a, "readwrite_a");
	PRINT_ENTRY(readwrite_b, "readwrite_b");

#undef PRINT_ENTRY
}
