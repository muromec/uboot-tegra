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
#include <malloc.h>
#include <chromeos/common.h>
#include <chromeos/fdt_decode.h>
#include <chromeos/onestop.h>
#include <linux/string.h>

#include <load_firmware_fw.h>
#include <vboot_struct.h>

#define PREFIX "onestop_helper: "

typedef struct {
	firmware_storage_t *file;
	struct {
		uint32_t offset;
		uint32_t size;
		uint8_t *vblock;
		uint8_t *fwbody;
	} fwinfo[2];
} get_firmware_body_state_t;

int is_tpm_trust_ro_firmware(void)
{
	/* TODO implement later */
	return 0;
}

int GetFirmwareBody(LoadFirmwareParams *params, uint64_t index)
{
	get_firmware_body_state_t *s =
		(get_firmware_body_state_t *)params->caller_internal;
	int i = (int)index;

	if (i != 0 && i != 1) {
		VBDEBUG(PREFIX "incorrect firmware index: %d\n", i);
		return 1;
	}

	/* TODO load firmware from mmc */
	if (firmware_storage_read(s->file, s->fwinfo[i].offset,
				s->fwinfo[i].size, s->fwinfo[i].fwbody)) {
		VBDEBUG(PREFIX "fail to read firmware body: %d\n", i);
		return 1;
	}

	UpdateFirmwareBodyHash(params, s->fwinfo[i].fwbody, s->fwinfo[i].size);
	return 0;
}

uint32_t get_fwbody_size(uint32_t vblock_address)
{
	VbKeyBlockHeader         *keyblock;
	VbFirmwarePreambleHeader *preamble;

	keyblock = (VbKeyBlockHeader *)vblock_address;
	preamble = (VbFirmwarePreambleHeader *)
		(vblock_address + (uint32_t)keyblock->key_block_size);

	return preamble->body_signature.data_size;
}

uint32_t boot_rw_firmware(firmware_storage_t *file,
		struct fdt_onestop_fmap *fmap, int dev_mode,
		uint8_t *gbb_data, crossystem_data_t *cdata,
		VbNvContext *nvcxt)
{
	/* TODO Read r/w firmware from MMC */

	int status = LOAD_FIRMWARE_RECOVERY;
	LoadFirmwareParams params;
	get_firmware_body_state_t internal;
	uint8_t vbshared[VB_SHARED_DATA_REC_SIZE];
	int i, w, t;

	internal.fwinfo[0].vblock = malloc(fmap->onestop_layout.vblock.length);
	internal.fwinfo[1].vblock = malloc(fmap->onestop_layout.vblock.length);
	internal.fwinfo[0].fwbody = malloc(fmap->onestop_layout.fwbody.length);
	internal.fwinfo[1].fwbody = malloc(fmap->onestop_layout.fwbody.length);

	if (firmware_storage_read(file,
				fmap->readwrite_a.offset +
				fmap->onestop_layout.vblock.offset,
				fmap->onestop_layout.vblock.length,
				internal.fwinfo[0].vblock)) {
		VBDEBUG(PREFIX "fail to read vblock A\n");
		return VBNV_RECOVERY_RO_INVALID_RW;
	}
	if (firmware_storage_read(file,
				fmap->readwrite_a.offset +
				fmap->onestop_layout.vblock.offset,
				fmap->onestop_layout.vblock.length,
				internal.fwinfo[1].vblock)) {
		VBDEBUG(PREFIX "fail to read vblock B\n");
		return VBNV_RECOVERY_RO_INVALID_RW;
	}

	internal.file = file;
	internal.fwinfo[0].offset =
		fmap->readwrite_a.offset + fmap->onestop_layout.fwbody.offset;
	internal.fwinfo[1].offset =
		fmap->readwrite_b.offset + fmap->onestop_layout.fwbody.offset;
	internal.fwinfo[0].size = get_fwbody_size((uint32_t)
			internal.fwinfo[0].vblock);
	internal.fwinfo[1].size = get_fwbody_size((uint32_t)
			internal.fwinfo[1].vblock);

	params.gbb_data = gbb_data;
	params.gbb_size = fmap->readonly.gbb.length;
	params.verification_block_0 = internal.fwinfo[0].vblock;
	params.verification_block_1 = internal.fwinfo[1].vblock;
	params.verification_size_0 = params.verification_size_1 =
		fmap->onestop_layout.vblock.length;
	params.shared_data_blob = vbshared;
	params.shared_data_size = VB_SHARED_DATA_REC_SIZE;
	params.boot_flags = dev_mode ? BOOT_FLAG_DEVELOPER : 0;
	params.nv_context = nvcxt;
	params.caller_internal = &internal;

	VBDEBUG(PREFIX "LoadFirmware...\n");
	VBDEBUG(PREFIX "- gbb_data:   %p\n", params.gbb_data);
	VBDEBUG(PREFIX "- gbb_size:   %08llx\n", params.gbb_size);
	VBDEBUG(PREFIX "- vblock_0:   %p\n", params.verification_block_0);
	VBDEBUG(PREFIX "- vblock_1:   %p\n", params.verification_block_1);
	VBDEBUG(PREFIX "- vsize_0:    %08llx\n", params.verification_size_0);
	VBDEBUG(PREFIX "- vsize_1:    %08llx\n", params.verification_size_1);
	VBDEBUG(PREFIX "- vbshared:   %p\n", params.shared_data_blob);
	VBDEBUG(PREFIX "- size:       %08llx\n", params.shared_data_size);
	VBDEBUG(PREFIX "- boot_flags: %08llx\n", params.boot_flags);

	status = LoadFirmware(&params);
	VBDEBUG(PREFIX "status: %d\n", status);

	if (status == LOAD_FIRMWARE_SUCCESS) {
		i = (int)params.firmware_index;
		VBDEBUG(PREFIX "go to r/w firmware %d", i);

		/*
		 * Although second-stage firmware only uses the active main
		 * firmware information that first-stage firmware passes to it,
		 * we simply copy the whole crossystem data blob. If there is
		 * really a performance issue, we can optimize this part later.
		 */
		w = (i == 0) ? REWRITABLE_FIRMWARE_A : REWRITABLE_FIRMWARE_B;
		t = dev_mode ? DEVELOPER_TYPE : NORMAL_TYPE;
		if (crossystem_data_set_active_main_firmware(cdata, w, t))
			VBDEBUG(PREFIX "failed to set active main firmware\n");
		memmove((void *)CONFIG_CROSSYSTEM_DATA_ADDRESS, cdata,
				sizeof(*cdata));

		memmove((void *)CONFIG_SYS_TEXT_BASE, internal.fwinfo[i].fwbody,
				internal.fwinfo[i].size);

		/* FIXME temporary hack for go to second-stage firmware */
		cleanup_before_linux();
		((void(*)(void))CONFIG_SYS_TEXT_BASE)();
	}

	VBDEBUG(PREFIX "fail to boot to r/w firmware; image broken?\n");
	return VBNV_RECOVERY_RO_INVALID_RW;
}
