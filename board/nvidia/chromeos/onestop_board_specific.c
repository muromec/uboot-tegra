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
#include <chromeos/onestop.h>

#define PREFIX "onestop_helper: "

extern int is_tegra2_cold_boot;

int is_ro_firmware(void)
{
	if (is_tegra2_cold_boot == ~0U)
		VBDEBUG(PREFIX "error: is_tegra2_cold_boot uninitialized\n");
	return is_tegra2_cold_boot ? 1 : 0;
}
