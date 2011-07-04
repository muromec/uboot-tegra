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
#include <command.h>
#include <vboot/entry_points.h>

int do_vboot_main(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	main_entry();
	return 0;
}

U_BOOT_CMD(vboot_main, 1, 1, do_vboot_main,
		"verified boot main firmware", NULL);
