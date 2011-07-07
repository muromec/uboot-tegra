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
#include <chromeos/common.h>
#include <chromeos/crossystem_data.h>

#define PREFIX "preboot_fdt_update: "

/*
 * We uses a static variable to communicate with fit_update_fdt_before_boot().
 * For more information, please see commit log.
 */
static crossystem_data_t *g_crossystem_data = NULL;

void set_crossystem_data(crossystem_data_t *cdata)
{
	g_crossystem_data = cdata;
}

int fit_update_fdt_before_boot(char *fdt, ulong *new_size)
{
	uint32_t ns;

	if (!g_crossystem_data) {
		VBDEBUG(PREFIX "warning: g_crossystem_data is NULL\n");
		return 1;
	}

	if (crossystem_data_embed_into_fdt(g_crossystem_data, fdt, &ns)) {
		VBDEBUG(PREFIX "crossystem_data_embed_into_fdt() failed\n");
		return 1;
	}

	*new_size = ns;
	return 0;
}
