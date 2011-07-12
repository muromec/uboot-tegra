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

#define LIST_OF_ENTRIES \
	ACT_ON_ENTRY("/onestop-layout", onestop_layout.onestop_layout); \
	ACT_ON_ENTRY("/firmware-image", onestop_layout.fwbody); \
	ACT_ON_ENTRY("/verification-block", onestop_layout.vblock); \
	ACT_ON_ENTRY("/firmware-id", onestop_layout.fwid); \
	ACT_ON_ENTRY("/readonly", readonly.readonly); \
	ACT_ON_ENTRY("/readonly/ro_onestop", readonly.ro_onestop); \
	ACT_ON_ENTRY("/fmap", readonly.fmap); \
	ACT_ON_ENTRY("/gbb", readonly.gbb); \
	ACT_ON_ENTRY("/readwrite-a", readwrite_a.readwrite_a); \
	ACT_ON_ENTRY("/rw-a-onestop", readwrite_a.rw_a_onestop); \
	ACT_ON_ENTRY("/readwrite-b", readwrite_b.readwrite_b); \
	ACT_ON_ENTRY("/rw-b-onestop", readwrite_b.rw_b_onestop);

int fdt_decode_twostop_fmap(const void *blob, struct fdt_twostop_fmap *config)
{
	int fmap_offset, offset;

	fmap_offset = fdt_node_offset_by_compatible(blob, -1,
			"chromeos,flashmap");
	if (fmap_offset < 0)
		return fmap_offset;

#define ACT_ON_ENTRY(path, entry) \
	offset = fdt_decode_fmap_entry(blob, fmap_offset, path, \
			&config->entry); \
	if (offset < 0) \
		return offset;

	LIST_OF_ENTRIES

#undef ACT_ON_ENTRY

	return 0;
}

void dump_fmap(struct fdt_twostop_fmap *config)
{
#define ACT_ON_ENTRY(path, entry) \
	VBDEBUG(PREFIX "%-20s %08x:%08x\n", path, \
			config->entry.offset, config->entry.length)

	LIST_OF_ENTRIES

#undef ACT_ON_ENTRY
}
