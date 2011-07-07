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
#include <chromeos/cmdline_updater.h>
#include <chromeos/common.h>
#include <chromeos/crossystem_data.h>
#include <chromeos/load_kernel_helper.h>
#include <chromeos/preboot_fdt_update.h>
#include <chromeos/os_storage.h>
#include <chromeos/vboot_nvstorage_helper.h>

#include <load_kernel_fw.h>

#define PREFIX "load_kernel_helper: "

/* defined in common/cmd_bootm.c */
int do_bootm(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);

/**
 * This is a wrapper of LoadKernel, which verifies the kernel image specified
 * by set_bootdev. The caller of this functions must have called set_bootdev
 * first.
 *
 * @param oss is the OS storage interface, for reading from the boot disk
 * @param boot_flags are bitwise-or'ed of flags in load_kernel_fw.h
 * @param gbb_data points to a GBB blob
 * @param gbb_size is the size of the GBB blob
 * @param vbshared_data points to VbSharedData blob
 * @param vbshared_size is the size of the VbSharedData blob
 * @param nvcxt points to a VbNvContext object
 * @return LoadKernel's return value
 */
static int load_kernel_wrapper(struct os_storage *oss,
		LoadKernelParams *params, uint64_t boot_flags,
		void *gbb_data, uint32_t gbb_size,
		void *vbshared_data, uint32_t vbshared_size,
		VbNvContext *nvcxt)
{
	int status = LOAD_KERNEL_NOT_FOUND;

	memset(params, '\0', sizeof(*params));

	params->boot_flags = boot_flags;

	params->gbb_data = gbb_data;
	params->gbb_size = gbb_size;

	params->shared_data_blob = vbshared_data;
	params->shared_data_size = vbshared_size;

	params->bytes_per_lba = os_storage_get_bytes_per_lba(oss);
	params->ending_lba = os_storage_get_ending_lba(oss);

	params->kernel_buffer = (void*)CONFIG_CHROMEOS_KERNEL_LOADADDR;
	params->kernel_buffer_size = CONFIG_CHROMEOS_KERNEL_BUFSIZE;

	params->nv_context = nvcxt;

	VBDEBUG(PREFIX "call LoadKernel() with parameters...\n");
	VBDEBUG(PREFIX "shared_data_blob:     0x%p\n",
			params->shared_data_blob);
	VBDEBUG(PREFIX "bytes_per_lba:        %d\n",
			(int) params->bytes_per_lba);
	VBDEBUG(PREFIX "ending_lba:           0x%08x\n",
			(int) params->ending_lba);
	VBDEBUG(PREFIX "kernel_buffer:        0x%p\n",
			params->kernel_buffer);
	VBDEBUG(PREFIX "kernel_buffer_size:   0x%08x\n",
			(int) params->kernel_buffer_size);
	VBDEBUG(PREFIX "boot_flags:           0x%08x\n",
			(int) params->boot_flags);

	status = LoadKernel(params);

	VBDEBUG(PREFIX "LoadKernel status: %d\n", status);
#ifdef VBOOT_DEBUG
	if (status == LOAD_KERNEL_SUCCESS) {
		VBDEBUG(PREFIX "partition_number:   0x%08x\n",
				(int) params->partition_number);
		VBDEBUG(PREFIX "bootloader_address: 0x%08x\n",
				(int) params->bootloader_address);
		VBDEBUG(PREFIX "bootloader_size:    0x%08x\n",
				(int) params->bootloader_size);
	}
#endif /* VBOOT_DEBUG */

	return status;
}

/* Maximum kernel command-line size */
#define CROS_CONFIG_SIZE 4096

/* Size of the x86 zeropage table */
#define CROS_PARAMS_SIZE 4096

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

/**
 * This sets up bootargs environment variable.
 *
 * @param oss is the OS storage interface, for reading from the boot disk
 * @param params that specifies where to boot from
 * @return LOAD_KERNEL_INVALID if it fails to boot; otherwise it never returns
 *         to its caller
 */
static int setup_kernel_command_line(struct os_storage *oss,
		LoadKernelParams *params)
{
	enum { EXTRA_BUFFER = 4096 };

	char cmdline_buf[CROS_CONFIG_SIZE + EXTRA_BUFFER];
	char cmdline_out[CROS_CONFIG_SIZE + EXTRA_BUFFER];
	char *cmdline;

	/*
	 * casting bootloader_address of uint64_t type to uint32_t before
	 * further casting it to char * to avoid compiler warning
	 * "cast to pointer from integer of different size"
	 */
	cmdline = get_kernel_config((char *)
			(uint32_t)params->bootloader_address);
	strncpy(cmdline_buf, cmdline, CROS_CONFIG_SIZE);

	/* if we have init bootargs, append it */
	if ((cmdline = getenv("bootargs"))) {
		strncat(cmdline_buf, " ", EXTRA_BUFFER);
		strncat(cmdline_buf, cmdline, EXTRA_BUFFER - 1);
	}

	VBDEBUG(PREFIX "cmdline before update: %s\n", cmdline_buf);
	update_cmdline(cmdline_buf, os_storage_get_device_number(oss),
			params->partition_number + 1, params->partition_guid,
			cmdline_out);

	setenv("bootargs", cmdline_out);
	VBDEBUG(PREFIX "cmdline after update:  %s\n", getenv("bootargs"));

	return 0;
}

/**
 * This boots kernel specified in [parmas].
 *
 * @param oss		OS storage interface
 * @param params	Specifies where to boot from
 * @param cdata		kernel shared data pointer
 * @return LOAD_KERNEL_INVALID if it fails to boot; otherwise it never returns
 *         to its caller
 */
static int boot_kernel_helper(struct os_storage *oss, LoadKernelParams *params,
		crossystem_data_t *cdata)
{
	char load_address[32];
	char *argv[2] = { "bootm", load_address };

	VBDEBUG(PREFIX "boot_kernel\n");
	VBDEBUG(PREFIX "kernel_buffer:      0x%p\n",
			params->kernel_buffer);
	VBDEBUG(PREFIX "bootloader_address: 0x%08x\n",
			(int) params->bootloader_address);

	if (setup_kernel_command_line(oss, params)) {
		VBDEBUG(PREFIX "failed to set up kernel command-line\n");
		return LOAD_KERNEL_INVALID;
	}

	set_crossystem_data(cdata);

	sprintf(load_address, "0x%p", params->kernel_buffer);
	do_bootm(NULL, 0, sizeof(argv)/sizeof(*argv), argv);

	VBDEBUG(PREFIX "failed to boot; is kernel broken?\n");
	return LOAD_KERNEL_INVALID;
}

int load_and_boot_kernel(struct os_storage *oss, uint64_t boot_flags,
		void *gbb_data, uint32_t gbb_size,
		void *vbshared_data, uint32_t vbshared_size,
		VbNvContext *nvcxt, crossystem_data_t *cdata)
{
	LoadKernelParams params;
	int status;

	status = load_kernel_wrapper(oss, &params, boot_flags, gbb_data,
			gbb_size, vbshared_data, vbshared_size, nvcxt);

	VBDEBUG(PREFIX "load_kernel_wrapper returns %d\n", status);

	/*
	 * Failure of tearing down or writing back VbNvContext is non-fatal. So
	 * we just complain about it, but don't return error code for it.
	 */
	if (VbNvTeardown(nvcxt))
		VBDEBUG(PREFIX "fail to tear down VbNvContext\n");
	else if (nvcxt->raw_changed && write_nvcontext(nvcxt))
		VBDEBUG(PREFIX "fail to write back nvcontext\n");

	if (status == LOAD_KERNEL_SUCCESS)
		return boot_kernel_helper(oss, &params, cdata);

	return status;
}
