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
#include <vboot/entry_points.h>
#include <vboot_api.h>

#define PREFIX		"main: "

void main_entry(void)
{
	VbExDebug("Here is Main Firmware.\n");
	while (1);
}
