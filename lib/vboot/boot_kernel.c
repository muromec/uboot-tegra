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
#include <part.h>
#include <chromeos/cmdline_updater.h>
#include <chromeos/crossystem_data.h>
#include <chromeos/preboot_fdt_update.h>
#include <vboot/boot_kernel.h>

#include <load_kernel_fw.h>
#include <vboot_api.h>

#define PREFIX "boot_kernel: "

/* defined in common/cmd_bootm.c */
int do_bootm(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);

/* Maximum kernel command-line size */
#define CROS_CONFIG_SIZE	4096

/* Size of the x86 zeropage table */
#define CROS_PARAMS_SIZE	4096

/* Extra buffer to string replacement */
#define EXTRA_BUFFER		4096

/**
 * This loads kernel command line from the buffer that holds the loaded kernel
 * image. This function calculates the address of the command line from the
 * bootloader address.
 *
 * @param bootloader_address is the address of the bootloader in the buffer
 * @return kernel config address
 */
static char *get_kernel_config(char *bootloader_address)
{
	/* Use the bootloader address to find the kernel config location. */
	return bootloader_address - CROS_PARAMS_SIZE - CROS_CONFIG_SIZE;
}

static uint32_t get_dev_num(const block_dev_desc_t *dev)
{
	return dev->dev;
}

/**
 * This boots kernel specified in [kparmas].
 *
 * @param kparams	kparams returned from VbSelectAndLoadKernel()
 * @param cdata		crossystem data pointer
 * @return LOAD_KERNEL_INVALID if it fails to boot; otherwise it never returns
 *         to its caller
 */
int boot_kernel(VbSelectAndLoadKernelParams *kparams, crossystem_data_t *cdata)
{
	char cmdline_buf[CROS_CONFIG_SIZE + EXTRA_BUFFER];
	char cmdline_out[CROS_CONFIG_SIZE + EXTRA_BUFFER];
	char load_address[32];
	char *argv[2] = {"bootm", load_address};
	char *cmdline;

	/*
	 * casting bootloader_address of uint64_t type to uint32_t before
	 * further casting it to char * to avoid compiler warning
	 * "cast to pointer from integer of different size"
	 */
	cmdline = get_kernel_config((char *)
			(uint32_t)kparams->bootloader_address);
	strncpy(cmdline_buf, cmdline, CROS_CONFIG_SIZE);

	/* if we have init bootargs, append it */
	if ((cmdline = getenv("bootargs"))) {
		strcat(cmdline_buf, " ");
		strncat(cmdline_buf, cmdline, EXTRA_BUFFER - 1);
	}

	VbExDebug(PREFIX "cmdline before update: %s\n", cmdline_buf);

	update_cmdline(cmdline_buf,
			get_dev_num(kparams->disk_handle),
			kparams->partition_number + 1,
			kparams->partition_guid,
			cmdline_out);

	setenv("bootargs", cmdline_out);
	VbExDebug(PREFIX "cmdline after update: %s\n", getenv("bootargs"));

	set_crossystem_data(cdata);

	sprintf(load_address, "0x%p", kparams->kernel_buffer);
	do_bootm(NULL, 0, sizeof(argv)/sizeof(*argv), argv);

	VbExDebug(PREFIX "Failed to boot; is kernel broken?\n");
	return LOAD_KERNEL_INVALID;
}
