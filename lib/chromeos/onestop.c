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
#include <linux/string.h>

#include <load_firmware_fw.h>
#include <vboot_struct.h>

#define PREFIX "onestop_helper: "

/* TODO I am removing this; twostop uses a different layout */
#if CONFIG_LENGTH_VBLOCK_A != CONFIG_LENGTH_VBLOCK_B
#  error "vblock A and B have different sizes"
#endif
#define VBLOCK_SIZE CONFIG_LENGTH_VBLOCK_A

#if CONFIG_LENGTH_FW_MAIN_A != CONFIG_LENGTH_FW_MAIN_B
#  error "fw body A and B have different sizes"
#endif
#define FWBODY_SIZE CONFIG_LENGTH_FW_MAIN_A

typedef struct {
	firmware_storage_t *file;
	struct {
		uint32_t offset;
		uint32_t size;
		uint8_t vblock[VBLOCK_SIZE];
		uint8_t fwbody[FWBODY_SIZE];
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

uint32_t boot_rw_firmware(firmware_storage_t *file, int dev_mode,
			  uint8_t *gbb_data, crossystem_data_t *cdata,
			  VbNvContext *nvcxt)
{
	/*
	 * TODO
	 * 1. Read r/w firmware from MMC
	 * 2. Set main firmwre A/B in crossystem data
	 */

	int status = LOAD_FIRMWARE_RECOVERY;
	LoadFirmwareParams params;
	get_firmware_body_state_t internal;
	uint8_t vbshared[VB_SHARED_DATA_REC_SIZE];
	int i;

	if (firmware_storage_read(file, CONFIG_OFFSET_VBLOCK_A,
				VBLOCK_SIZE, internal.fwinfo[0].vblock)) {
		VBDEBUG(PREFIX "fail to read vblock A\n");
		return VBNV_RECOVERY_RO_INVALID_RW;
	}
	if (firmware_storage_read(file, CONFIG_OFFSET_VBLOCK_B,
				VBLOCK_SIZE, internal.fwinfo[1].vblock)) {
		VBDEBUG(PREFIX "fail to read vblock B\n");
		return VBNV_RECOVERY_RO_INVALID_RW;
	}

	internal.file = file;
	internal.fwinfo[0].offset = CONFIG_OFFSET_FW_MAIN_A;
	internal.fwinfo[1].offset = CONFIG_OFFSET_FW_MAIN_B;
	internal.fwinfo[0].size = get_fwbody_size((uint32_t)
			internal.fwinfo[0].vblock);
	internal.fwinfo[1].size = get_fwbody_size((uint32_t)
			internal.fwinfo[1].vblock);

	params.gbb_data = gbb_data;
	params.gbb_size = CONFIG_LENGTH_GBB;
	params.verification_block_0 = internal.fwinfo[0].vblock;
	params.verification_block_1 = internal.fwinfo[1].vblock;
	params.verification_size_0 = VBLOCK_SIZE;
	params.verification_size_1 = VBLOCK_SIZE;
	params.shared_data_blob = vbshared;
	params.shared_data_size = VB_SHARED_DATA_REC_SIZE;
	params.boot_flags = dev_mode ? BOOT_FLAG_DEVELOPER : 0;
	params.nv_context = nvcxt;
	params.caller_internal = &internal;

	status = LoadFirmware(&params);
	if (status == LOAD_FIRMWARE_SUCCESS) {
		i = (int)params.firmware_index;
		VBDEBUG(PREFIX "go to r/w firmware %d", i);

		memmove((void *)CONFIG_SYS_TEXT_BASE, internal.fwinfo[i].fwbody,
				internal.fwinfo[i].size);

		/* FIXME temporary hack for go to second-stage firmware */
		cleanup_before_linux();
		((void(*)(void))CONFIG_SYS_TEXT_BASE)();
	}

	VBDEBUG(PREFIX "fail to boot to r/w firmware; image broken?\n");
	return VBNV_RECOVERY_RO_INVALID_RW;
}
