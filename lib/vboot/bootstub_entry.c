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
#include <chromeos/fdt_decode.h>
#include <chromeos/firmware_storage.h>
#include <chromeos/power_management.h>
#include <vboot/entry_points.h>
#include <vboot/firmware_cache.h>
#include <vboot/global_data.h>

#include <vboot_api.h>

#define PREFIX		"bootstub: "

DECLARE_GLOBAL_DATA_PTR;

static void prepare_cparams(vb_global_t *global, VbCommonParams *cparams)
{
	cparams->gbb_data = global->gbb_data;
	cparams->gbb_size = global->gbb_size;
	cparams->shared_data_blob = global->cdata_blob.vbshared_data;
	cparams->shared_data_size = VB_SHARED_DATA_REC_SIZE;
}

static void prepare_iparams(vb_global_t *global, VbInitParams *iparams)
{
	crossystem_data_t *cdata = &global->cdata_blob;
	iparams->flags = VB_INIT_FLAG_RO_NORMAL_SUPPORT;
	if (cdata->developer_sw)
		iparams->flags |= VB_INIT_FLAG_DEV_SWITCH_ON;
	if (cdata->recovery_sw)
		iparams->flags |= VB_INIT_FLAG_REC_BUTTON_PRESSED;
	if (cdata->write_protect_sw)
		iparams->flags |= VB_INIT_FLAG_WP_ENABLED;
}

static int read_verification_block(firmware_storage_t *file,
		const off_t vblock_offset, void **vblock_ptr,
		uint32_t *vblock_size_ptr, uint32_t *body_size_ptr)
{
	VbKeyBlockHeader kbh;
	VbFirmwarePreambleHeader fph;
	uint32_t key_block_size, vblock_size;
	void *vblock;

	/* read key block header */
	if (file->read(file, vblock_offset, sizeof(kbh), &kbh)) {
		VBDEBUG(PREFIX "Failed to read key block!\n");
		return -1;
	}
	key_block_size = kbh.key_block_size;

	/* read firmware preamble header */
	if (file->read(file, vblock_offset + key_block_size,
				sizeof(fph), &fph)) {
		VBDEBUG(PREFIX "Failed to read preamble!\n");
		return -1;
	}
	vblock_size = key_block_size + fph.preamble_size;

	vblock = VbExMalloc(vblock_size);

	if (file->read(file, vblock_offset, vblock_size, vblock)) {
		VBDEBUG(PREFIX "Failed to read verification block!\n");
		VbExFree(vblock);
		return -1;
	}

	*vblock_ptr = vblock;
	*vblock_size_ptr = vblock_size;
	*body_size_ptr = fph.body_signature.data_size;

	return 0;
}

static void prepare_fparams(firmware_storage_t *file,
			    firmware_cache_t *cache,
			    struct fdt_twostop_fmap *fmap,
			    VbSelectFirmwareParams *fparams)
{
	uint32_t fw_main_a_size, fw_main_b_size;

	if (read_verification_block(file,
			fmap->readwrite_a.vblock.offset,
			&fparams->verification_block_A,
			&fparams->verification_size_A,
			&fw_main_a_size))
		VbExError(PREFIX "Failed to read verification block A!\n");

	if (read_verification_block(file,
			fmap->readwrite_b.vblock.offset,
			&fparams->verification_block_B,
			&fparams->verification_size_B,
			&fw_main_b_size))
		VbExError(PREFIX "Failed to read verification block B!\n");

	/* Prepare the firmware cache which is passed as caller_context. */
	init_firmware_cache(cache,
			file,
			fmap->readwrite_a.boot.offset,
			fw_main_a_size,
			fmap->readwrite_b.boot.offset,
			fw_main_b_size);
}

static void release_fparams(VbSelectFirmwareParams *fparams)
{
	VbExFree(fparams->verification_block_A);
	VbExFree(fparams->verification_block_B);
}

static void clear_ram_not_in_use(void)
{
	/*
	 * TODO(waihong@chromium.org)
	 * Copy the code from common/cmd_cros_rec.
	 */
}

typedef void (*firmware_entry_t)(void);

static void jump_to_firmware(firmware_entry_t firmware_entry)
{
	VBDEBUG(PREFIX "Jump to firmware %p...\n", firmware_entry);

	cleanup_before_linux();

	/* Jump and never return */
	(*firmware_entry)();

	VbExError(PREFIX "Firmware %p returned!\n", firmware_entry);
}

static VbError_t call_VbInit(VbCommonParams *cparams, VbInitParams *iparams)
{
	VbError_t ret;

	VBDEBUG("VbCommonParams:\n");
	VBDEBUG("    gbb_data         : 0x%lx\n", cparams->gbb_data);
	VBDEBUG("    gbb_size         : %lu\n", cparams->gbb_size);
	VBDEBUG("    shared_data_blob : 0x%lx\n", cparams->shared_data_blob);
	VBDEBUG("    shared_data_size : %lu\n", cparams->shared_data_size);
	VBDEBUG("    caller_context   : 0x%lx\n", cparams->caller_context);
	VBDEBUG("VbInitParams:\n");
	VBDEBUG("    flags         : 0x%lx\n", iparams->flags);
	VBDEBUG("Calling VbInit()...\n");

	ret = VbInit(cparams, iparams);
	VBDEBUG("Returned 0x%lu\n", ret);

	if (!ret) {
		VBDEBUG("VbInitParams:\n");
		VBDEBUG("    out_flags     : 0x%lx\n", iparams->out_flags);
	}

	return ret;
}

static VbError_t call_VbSelectFirmware(VbCommonParams *cparams,
                                       VbSelectFirmwareParams *fparams)
{
	VbError_t ret;

	VBDEBUG("VbCommonParams:\n");
	VBDEBUG("    gbb_data         : 0x%lx\n", cparams->gbb_data);
	VBDEBUG("    gbb_size         : %lu\n", cparams->gbb_size);
	VBDEBUG("    shared_data_blob : 0x%lx\n", cparams->shared_data_blob);
	VBDEBUG("    shared_data_size : %lu\n", cparams->shared_data_size);
	VBDEBUG("    caller_context   : 0x%lx\n", cparams->caller_context);

	VBDEBUG("VbSelectFirmwareParams:\n");
	VBDEBUG("    verification_block_A : 0x%lx\n",
			fparams->verification_block_A);
	VBDEBUG("    verification_block_B : 0x%lx\n",
			fparams->verification_block_B);
	VBDEBUG("    verification_size_A  : %lu\n",
			fparams->verification_size_A);
	VBDEBUG("    verification_size_B  : %lu\n",
			fparams->verification_size_B);
	VBDEBUG("Calling VbSelectFirmware()...\n");

	ret = VbSelectFirmware(cparams, fparams);
	VBDEBUG("Returned 0x%lu\n", ret);

	if (!ret) {
		VBDEBUG("VbSelectFirmwareParams:\n");
		VBDEBUG("    selected_firmware    : %lu\n",
				fparams->selected_firmware);
	}

	return ret;
}

static int set_fwid_value(vb_global_t *global,
			  firmware_storage_t *file,
			  struct fdt_twostop_fmap *fmap,
			  uint32_t selected_firmware)
{
	crossystem_data_t *cdata = &global->cdata_blob;
	char fwid[ID_LEN];
	uint32_t fwid_offset;

	switch (selected_firmware) {
	case VB_SELECT_FIRMWARE_A:
		fwid_offset = fmap->readwrite_a.firmware_id.offset;
		break;

	case VB_SELECT_FIRMWARE_B:
		fwid_offset = fmap->readwrite_b.firmware_id.offset;
		break;

	case VB_SELECT_FIRMWARE_RECOVERY:
		fwid_offset = fmap->readonly.firmware_id.offset;
		break;

	case VB_SELECT_FIRMWARE_READONLY:
		fwid_offset = fmap->readonly.firmware_id.offset;
		break;

	default:
		return 1;
	}

	if (file->read(file, fwid_offset, ID_LEN, fwid)) {
		VBDEBUG(PREFIX "Failed to read FWID!\n");
		return 1;
	}

	VBDEBUG(PREFIX "Set FWID as %s\n", fwid);
	crossystem_data_set_fwid(cdata, fwid);

	return 0;
}

void bootstub_entry(void)
{
	void *fdt_ptr = (void *)gd->blob;
	struct fdt_twostop_fmap fmap;
	vb_global_t *global;
	firmware_storage_t file;
	firmware_cache_t cache;
	VbCommonParams cparams;
	VbInitParams iparams;
	VbSelectFirmwareParams fparams;
	VbError_t ret;

	if (fdt_decode_twostop_fmap(fdt_ptr, &fmap))
		VbExError(PREFIX "Failed to load fmap config from fdt.\n");

	/* Open firmware storage device */
	if (firmware_storage_open_spi(&file))
		VbExError(PREFIX "Failed to open firmware device!\n");

	/* Init VBoot Global Data and load GBB from SPI */
	global = get_vboot_global();
	if (init_vboot_global(global, &file))
		VbExError(PREFIX "Failed to init vboot global data!\n");

	/* Call VbInit() */
	prepare_cparams(global, &cparams);
	prepare_iparams(global, &iparams);
	if ((ret = call_VbInit(&cparams, &iparams)))
		VbExError(PREFIX "VbInit() returns error: 0x%lx!\n", ret);

	/* Handle the VbInit() results */
	if (iparams.out_flags & VB_INIT_OUT_CLEAR_RAM)
		clear_ram_not_in_use();
	if (iparams.out_flags & VB_INIT_OUT_ENABLE_DISPLAY)
		if (load_bmpblk_in_gbb(global, &file))
			VbExError(PREFIX "Failed to load BMP Block!\n");
	if (iparams.out_flags & VB_INIT_OUT_ENABLE_RECOVERY)
		if (load_reckey_in_gbb(global, &file))
			VbExError(PREFIX "Failed to load recovery key!\n");

	/* Call VbSelectFirmware() */
	cparams.caller_context = &cache;
	prepare_fparams(&file, &cache, &fmap, &fparams);
	if ((ret = call_VbSelectFirmware(&cparams, &fparams)))
		VbExError(PREFIX "VbSelectFirmare() returned error: 0x%lx!\n",
				ret);
	release_fparams(&fparams);

	if (set_fwid_value(global, &file, &fmap, fparams.selected_firmware))
		VbExError(PREFIX "Failed to set FWID!\n");

	if (file.close(&file))
		VbExError(PREFIX "Failed to close firmware device!\n");

	/* Handle the VbSelectFirmware() results */
	switch (fparams.selected_firmware) {
	case VB_SELECT_FIRMWARE_A:
		memcpy((void *)CONFIG_SYS_TEXT_BASE, cache.infos[0].buffer,
				cache.infos[0].size);
		jump_to_firmware((firmware_entry_t)CONFIG_SYS_TEXT_BASE);
		break;

	case VB_SELECT_FIRMWARE_B:
		memcpy((void *)CONFIG_SYS_TEXT_BASE, cache.infos[1].buffer,
				cache.infos[1].size);
		jump_to_firmware((firmware_entry_t)CONFIG_SYS_TEXT_BASE);
		break;

	case VB_SELECT_FIRMWARE_RECOVERY:
		VBDEBUG(PREFIX "Boot to recovery mode...\n");
		main_entry();
		break;

	case VB_SELECT_FIRMWARE_READONLY:
		VBDEBUG(PREFIX "Boot to RO firmware...\n");
		main_entry();
		break;

	default:
		VbExError(PREFIX "Unexpected selected firmware value!\n");
	}

	VbExError(PREFIX "Should not reach here!\n");
}
