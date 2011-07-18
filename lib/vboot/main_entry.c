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
#include <vboot/boot_kernel.h>
#include <vboot/entry_points.h>
#include <vboot/global_data.h>
#include <vboot_api.h>

#define PREFIX		"main: "

static void prepare_cparams(vb_global_t *global, VbCommonParams *cparams)
{
	cparams->gbb_data = global->gbb_data;
	cparams->gbb_size = global->gbb_size;
	cparams->shared_data_blob = global->cdata_blob.vbshared_data;
	cparams->shared_data_size = VB_SHARED_DATA_REC_SIZE;
}

static void prepare_kparams(VbSelectAndLoadKernelParams *kparams)
{
	kparams->kernel_buffer = (void *)CONFIG_CHROMEOS_KERNEL_LOADADDR;
	kparams->kernel_buffer_size = CONFIG_CHROMEOS_KERNEL_BUFSIZE;
}

static VbError_t call_VbSelectAndLoadKernel(VbCommonParams* cparams,
					VbSelectAndLoadKernelParams* kparams)
{
        VbError_t ret;

        VBDEBUG("VbCommonParams:\n");
        VBDEBUG("    gbb_data         : 0x%lx\n", cparams->gbb_data);
        VBDEBUG("    gbb_size         : %lu\n", cparams->gbb_size);
        VBDEBUG("    shared_data_blob : 0x%lx\n", cparams->shared_data_blob);
        VBDEBUG("    shared_data_size : %lu\n", cparams->shared_data_size);
        VBDEBUG("    caller_context   : 0x%lx\n", cparams->caller_context);
        VBDEBUG("VbSelectAndLoadKernelParams:\n");
        VBDEBUG("    kernel_buffer      : 0x%lx\n", kparams->kernel_buffer);
        VBDEBUG("    kernel_buffer_size : %lu\n",
						kparams->kernel_buffer_size);
        VBDEBUG("Calling VbSelectAndLoadKernel()...\n");

        ret = VbSelectAndLoadKernel(cparams, kparams);
        VBDEBUG("Returned 0x%lu\n", ret);

	if (!ret) {
		int i;
	        VBDEBUG("VbSelectAndLoadKernelParams:\n");
		VBDEBUG("    disk_handle        : 0x%lx\n",
						kparams->disk_handle);
		VBDEBUG("    partition_number   : %lu\n",
						kparams->partition_number);
		VBDEBUG("    bootloader_address : 0x%llx\n",
						kparams->bootloader_address);
		VBDEBUG("    bootloader_size    : %lu\n",
						kparams->bootloader_size);
		VBDEBUG("    partition_guid     :");
		for (i = 0; i < 16; i++)
			VBDEBUG(" %02x", kparams->partition_guid[i]);
		VBDEBUG("\n");
	}

	return ret;
}

void main_entry(void)
{
	vb_global_t *global;
	VbCommonParams cparams;
	VbSelectAndLoadKernelParams kparams;
	VbError_t ret;

	/* Get VBoot Global Data which was initialized by bootstub. */
	global = get_vboot_global();
	if (!is_vboot_global_valid(global))
		VbExError(PREFIX "VBoot Global Data is not initialized!\n");

	prepare_cparams(global, &cparams);
	prepare_kparams(&kparams);

	ret = call_VbSelectAndLoadKernel(&cparams, &kparams);

	if (ret)
		VbExError(PREFIX "Failed to select and load kernel!\n");

	/* Do boot partition substitution in kernel cmdline and boot */
	ret = boot_kernel(&kparams, &global->cdata_blob);

	VbExError(PREFIX "boot_kernel_helper returned, %lu\n", ret);
}
