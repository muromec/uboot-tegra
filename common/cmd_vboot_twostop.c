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
#include <fdt_decode.h>
#include <lcd.h>
#include <malloc.h>
#include <chromeos/common.h>
#include <chromeos/crossystem_data.h>
#include <chromeos/cros_gpio.h>
#include <chromeos/fdt_decode.h>
#include <chromeos/firmware_storage.h>
#include <chromeos/power_management.h>
#include <vboot/boot_kernel.h>

#include <vboot_api.h>

#define PREFIX "vboot_twostop: "

DECLARE_GLOBAL_DATA_PTR;

/*
 * Although second-stage firmware does not trust anything first-stage firmware
 * tells to it because TPM says so. There are certain data that second-stage
 * firmware cannot acquire by itself, and has to rely on first-stage firmware.
 * Specifically, these data are whether crossystem data, such as active main
 * firmware (A or B).
 */
#define CROSSYSTEM_DATA_ADDRESS 0x00100000

typedef enum {
	I_AM_RO_FW = 0,
	I_AM_RW_A_FW,
	I_AM_RW_B_FW,
} whoami_t;

typedef enum {
	BOOT_SELF = 0,
	BOOT_RECOVERY,
	BOOT_RW_A,
	BOOT_RW_B,
} boot_target_t;

typedef struct {
	whoami_t whoami;
	struct fdt_twostop_fmap fmap;
	cros_gpio_t wpsw, recsw, devsw;
} twostop_t;

#ifdef VBOOT_DEBUG
static const char const *strwhoami[] = {
	"I_AM_RO_FW", "I_AM_RW_A_FW", "I_AM_RW_B_FW"
};

static const char const *strtarget[] = {
	"BOOT_SELF", "BOOT_RECOVERY", "BOOT_RW_A", "BOOT_RW_B"
};
#endif /* VBOOT_DEBUG */

int twostop_init(twostop_t *tdata,
		const void const *fdt)
{
	crossystem_data_t *first_stage_cdata = (crossystem_data_t *)
		CROSSYSTEM_DATA_ADDRESS;

	if (is_cold_boot())
		tdata->whoami = I_AM_RO_FW;
	else if (crossystem_data_get_active_main_firmware(first_stage_cdata) ==
			REWRITABLE_FIRMWARE_A)
		tdata->whoami = I_AM_RW_A_FW;
	else
		tdata->whoami = I_AM_RW_B_FW;

	VBDEBUG(PREFIX "whoami: %s\n", strwhoami[tdata->whoami]);

	if (cros_gpio_fetch(CROS_GPIO_WPSW, fdt, &tdata->wpsw) ||
			cros_gpio_fetch(CROS_GPIO_RECSW, fdt, &tdata->recsw) ||
			cros_gpio_fetch(CROS_GPIO_DEVSW, fdt, &tdata->devsw)) {
		VBDEBUG(PREFIX "failed to fetch gpio\n");
		return -1;
	}

	cros_gpio_dump(&tdata->wpsw);
	cros_gpio_dump(&tdata->recsw);
	cros_gpio_dump(&tdata->devsw);

	if (fdt_decode_twostop_fmap(fdt, &tdata->fmap)) {
		VBDEBUG(PREFIX "failed to decode fmap\n");
		return -1;
	}

	dump_fmap(&tdata->fmap);

	return 0;
}

int twostop_open_file(twostop_t *tdata, firmware_storage_t *file)
{
	/* We revert the decision of using firmware_storage_open_twostop() */
	return firmware_storage_open_spi(file);
}

int twostop_read_firmware_id(twostop_t *tdata,
		firmware_storage_t *file,
		char *firmware_id)
{
	const struct fdt_twostop_fmap const *fmap = &tdata->fmap;
	uint32_t firmware_id_offset = fmap->onestop_layout.fwid.offset;

	switch (tdata->whoami) {
	case I_AM_RO_FW:
		firmware_id_offset += fmap->readonly.ro_onestop.offset;
		break;
	case I_AM_RW_A_FW:
		firmware_id_offset += fmap->readwrite_a.rw_a_onestop.offset;
		break;
	case I_AM_RW_B_FW:
		firmware_id_offset += fmap->readwrite_b.rw_b_onestop.offset;
		break;
	default:
		VBDEBUG(PREFIX "unknown: whoami: %d\n", tdata->whoami);
		return -1;
	}

	return file->read(file, firmware_id_offset,
			fmap->onestop_layout.fwid.length,
			firmware_id);
}

void *twostop_read_gbb(twostop_t *tdata, firmware_storage_t *file)
{
	const struct fdt_twostop_fmap const *fmap = &tdata->fmap;
	void *gbb;

	if (!(gbb = malloc(fmap->readonly.gbb.length))) {
		VBDEBUG(PREFIX "failed to allocate gbb\n");
		return NULL;
	}

	if (file->read(file, fmap->readonly.gbb.offset,
				fmap->readonly.gbb.length, gbb)) {
		VBDEBUG(PREFIX "failed to read gbb\n");
		free(gbb);
		return NULL;
	}

	return gbb;
}

int twostop_init_crossystem_data(twostop_t *tdata,
		firmware_storage_t *file,
		void *gbb,
		crossystem_data_t *cdata)
{
	const struct fdt_twostop_fmap const *fmap = &tdata->fmap;
	char firmware_id[ID_LEN];
	uint8_t nvcxt_raw[VBNV_BLOCK_SIZE];

	if (twostop_read_firmware_id(tdata, file, firmware_id)) {
		VBDEBUG(PREFIX "failed to read firmware ID\n");
		return -1;
	}

	VBDEBUG(PREFIX "firmware id: \"%s\"\n", firmware_id);

	if (VbExNvStorageRead(nvcxt_raw)) {
		VBDEBUG(PREFIX "failed to read NvStorage\n");
		return -1;
	}

#ifdef VBOOT_DEBUG
	int i;
	VBDEBUG(PREFIX "nvcxt_raw: ");
	for (i = 0; i < VBNV_BLOCK_SIZE; i++)
		VBDEBUG("%02x", nvcxt_raw[i]);
	VBDEBUG("\n");
#endif /* VBOOT_DEBUG */

	if (crossystem_data_init(cdata,
				firmware_id,
				fmap->readonly.fmap.offset,
				gbb,
				nvcxt_raw,
				&tdata->wpsw,
				&tdata->recsw,
				&tdata->devsw)) {
		VBDEBUG(PREFIX "failed to init crossystem data\n");
		return -1;
	}

	return 0;
}

int twostop_init_cparams( twostop_t *tdata,
		void *gbb,
		void *vbshared_data,
		VbCommonParams *cparams)
{
	const struct fdt_twostop_fmap const *fmap = &tdata->fmap;

	cparams->gbb_data = gbb;
	cparams->gbb_size = fmap->readonly.gbb.length;
	cparams->shared_data_blob = vbshared_data;
	cparams->shared_data_size = VB_SHARED_DATA_REC_SIZE;

#define P(format, field) \
	VBDEBUG(PREFIX "- %-20s: " format "\n", #field, cparams->field)

	VBDEBUG(PREFIX "cparams:\n");
	P("%p", gbb_data);
	P("%08x", gbb_size);
	P("%p", shared_data_blob);
	P("%08x", shared_data_size);

#undef P

	return 0;
}

typedef struct {
	firmware_storage_t *file;
	struct {
		uint32_t offset;
		uint32_t size;
		void *cache;
	} fw[2];
} hasher_state_t;

uint32_t firmware_body_size(const uint32_t vblock_address)
{
	const VbKeyBlockHeader         const *keyblock;
	const VbFirmwarePreambleHeader const *preamble;

	keyblock = (VbKeyBlockHeader *)vblock_address;
	preamble = (VbFirmwarePreambleHeader *)
		(vblock_address + (uint32_t)keyblock->key_block_size);

	return preamble->body_signature.data_size;
}

VbError_t VbExHashFirmwareBody(VbCommonParams* cparams, uint32_t firmware_index)
{
	hasher_state_t const *s = (hasher_state_t *)cparams->caller_context;
	const int i = (firmware_index == VB_SELECT_FIRMWARE_A ? 0 : 1);
	firmware_storage_t *file = s->file;


	if (firmware_index != VB_SELECT_FIRMWARE_A &&
			firmware_index != VB_SELECT_FIRMWARE_B) {
		VBDEBUG(PREFIX "incorrect firmware index: %d\n",
				firmware_index);
		return 1;
	}

	if (file->read(file, s->fw[i].offset, s->fw[i].size, s->fw[i].cache)) {
		VBDEBUG(PREFIX "fail to read firmware: %d\n", firmware_index);
		return 1;
	}

	VbUpdateFirmwareBodyHash(cparams, s->fw[i].cache, s->fw[i].size);
	return 0;
}

boot_target_t twostop_select_boot_target(twostop_t *tdata,
		firmware_storage_t *file,
		void *gbb,
		crossystem_data_t *cdata,
		VbCommonParams *cparams,
		void **fw_blob_ptr,
		uint32_t *fw_size_ptr)
{
	const struct fdt_twostop_fmap const *fmap = &tdata->fmap;
	boot_target_t target = BOOT_RECOVERY;
	VbError_t err;
	uint32_t base[2], voffset, vlength;
	VbInitParams iparams;
	VbSelectFirmwareParams fparams;
	hasher_state_t s;

	memset(&fparams, '\0', sizeof(fparams));

	iparams.flags = 0;
	if (tdata->wpsw.value)
		iparams.flags |= VB_INIT_FLAG_WP_ENABLED;
	if (tdata->recsw.value)
		iparams.flags |= VB_INIT_FLAG_REC_BUTTON_PRESSED;
	if (tdata->devsw.value)
		iparams.flags |= VB_INIT_FLAG_DEV_SWITCH_ON;

	VBDEBUG(PREFIX "iparams.flags: %08x\n", iparams.flags);

	if ((err = VbInit(cparams, &iparams))) {
		VBDEBUG(PREFIX "VbInit: %d\n", err);
		goto out;
	}

	if (iparams.out_flags & VB_INIT_OUT_CLEAR_RAM)
		/* TODO(waihong) implement clear unused RAM */;

	if (iparams.out_flags & VB_INIT_OUT_ENABLE_RECOVERY)
		goto out;

	voffset = fmap->onestop_layout.vblock.offset;
	vlength = fmap->onestop_layout.vblock.length;

	if (tdata->whoami == I_AM_RW_A_FW) {
		base[0] = base[1] = fmap->readwrite_a.rw_a_onestop.offset;
	} else if (tdata->whoami == I_AM_RW_B_FW) {
		base[0] = base[1] = fmap->readwrite_b.rw_b_onestop.offset;
	} else {
		base[0] = fmap->readwrite_a.rw_a_onestop.offset;
		base[1] = fmap->readwrite_b.rw_b_onestop.offset;
	}

	fparams.verification_size_A = fparams.verification_size_B = vlength;

	fparams.verification_block_A = memalign(CACHE_LINE_SIZE, vlength);
	if (!fparams.verification_block_A) {
		VBDEBUG(PREFIX "failed to allocate vblock A\n");
		goto out;
	}
	fparams.verification_block_B = memalign(CACHE_LINE_SIZE, vlength);
	if (!fparams.verification_block_B) {
		VBDEBUG(PREFIX "failed to allocate vblock B\n");
		goto out;
	}

	if (file->read(file, base[0] + voffset, vlength,
				fparams.verification_block_A)) {
		VBDEBUG(PREFIX "fail to read vblock A\n");
		goto out;
	}
	if (file->read(file, base[1] + voffset, vlength,
				fparams.verification_block_B)) {
		VBDEBUG(PREFIX "fail to read vblock B\n");
		goto out;
	}

	s.fw[0].offset = base[0] + fmap->onestop_layout.fwbody.offset;
	s.fw[1].offset = base[1] + fmap->onestop_layout.fwbody.offset;

	s.fw[0].size = firmware_body_size((uint32_t)
			fparams.verification_block_A);
	s.fw[1].size = firmware_body_size((uint32_t)
			fparams.verification_block_B);

	s.fw[0].cache = memalign(CACHE_LINE_SIZE, s.fw[0].size);
	if (!s.fw[0].cache) {
		VBDEBUG(PREFIX "failed to allocate cache A\n");
		goto out;
	}
	s.fw[1].cache = memalign(CACHE_LINE_SIZE, s.fw[1].size);
	if (!s.fw[1].cache) {
		VBDEBUG(PREFIX "failed to allocate cache B\n");
		goto out;
	}

	s.file = file;
	cparams->caller_context = &s;

	if ((err = VbSelectFirmware(cparams, &fparams))) {
		VBDEBUG(PREFIX "VbSelectFirmware: %d\n", err);
		goto out;
	}

	VBDEBUG(PREFIX "selected_firmware: %d\n", fparams.selected_firmware);
	switch (fparams.selected_firmware) {
	/*
	 * TODO: Here will check if VbSelectFirmware decides to select
	 * R/O onestop.
	 */
	case VB_SELECT_FIRMWARE_A:
		target = (tdata->whoami == I_AM_RW_A_FW) ?
			BOOT_SELF : BOOT_RW_A;
		break;
	case VB_SELECT_FIRMWARE_B:
		target = (tdata->whoami == I_AM_RW_B_FW) ?
			BOOT_SELF : BOOT_RW_B;
		break;
	case VB_SELECT_FIRMWARE_RECOVERY:
		target = BOOT_RECOVERY;
		break;
	default:
		VBDEBUG(PREFIX "unkonwn selected firmware: %d\n",
				fparams.selected_firmware);
		break;
	}

out:
	VBDEBUG(PREFIX "target: %s\n", strtarget[target]);

	free(fparams.verification_block_A);
	free(fparams.verification_block_B);

	if (target == BOOT_RW_A) {
		*fw_blob_ptr = s.fw[0].cache;
		*fw_size_ptr = s.fw[0].size;
		free(s.fw[1].cache);
	} else if (target == BOOT_RW_B) {
		*fw_blob_ptr = s.fw[1].cache;
		*fw_size_ptr = s.fw[1].size;
		free(s.fw[0].cache);
	}

	return target;
}

void twostop_goto_rwfw(twostop_t *tdata, crossystem_data_t *cdata,
		boot_target_t target, void *fw_blob, uint32_t fw_size)
{
	int w, t;

	if (target != BOOT_RW_A && target != BOOT_RW_B) {
		VBDEBUG(PREFIX "invalid target: %s\n", strtarget[target]);
		return;
	}

	w = (target == BOOT_RW_A) ?
		REWRITABLE_FIRMWARE_A : REWRITABLE_FIRMWARE_B;
	t = tdata->devsw.value ? DEVELOPER_TYPE : NORMAL_TYPE;

	/* Set these data to R/W firmware */
	if (crossystem_data_set_active_main_firmware(cdata, w, t))
		VBDEBUG(PREFIX "failed to set active main firmware\n");

	memmove((void *)CROSSYSTEM_DATA_ADDRESS, cdata, sizeof(*cdata));

	memmove((void *)CONFIG_SYS_TEXT_BASE, fw_blob, fw_size);

	/* TODO go to second-stage firmware without extra cleanup */
	cleanup_before_linux();
	((void(*)(void))CONFIG_SYS_TEXT_BASE)();
}

void twostop_continue_boot(twostop_t *tdata,
		firmware_storage_t *file,
		crossystem_data_t *cdata,
		boot_target_t target,
		VbCommonParams *cparams)
{
	/* TODO We don't have a main firmware ID for R/O onestop */
	const int which[] = {
		RECOVERY_FIRMWARE, REWRITABLE_FIRMWARE_A, REWRITABLE_FIRMWARE_B
	};
	VbError_t err;
	int w, t;
	VbSelectAndLoadKernelParams kparams;
	char fwid[ID_LEN];

	if (target != BOOT_SELF && target != BOOT_RECOVERY) {
		VBDEBUG(PREFIX "invalid target: %s\n", strtarget[target]);
		return;
	}

	kparams.kernel_buffer = (void *)CONFIG_CHROMEOS_KERNEL_LOADADDR;
	kparams.kernel_buffer_size = CONFIG_CHROMEOS_KERNEL_BUFSIZE;

	VBDEBUG(PREFIX "kparams:\n");
	VBDEBUG(PREFIX "- kernel_buffer:      : %p\n", kparams.kernel_buffer);
	VBDEBUG(PREFIX "- kernel_buffer_size: : %08x\n",
			kparams.kernel_buffer_size);

        if ((err = VbSelectAndLoadKernel(cparams, &kparams))) {
		VBDEBUG(PREFIX "VbSelectAndLoadKernel: %d\n", err);
		return;
	}

#ifdef VBOOT_DEBUG
	VBDEBUG(PREFIX "kparams:\n");
	VBDEBUG(PREFIX "- disk_handle:        : %p\n", kparams.disk_handle);
	VBDEBUG(PREFIX "- partition_number:   : %08x\n",
			kparams.partition_number);
	VBDEBUG(PREFIX "- bootloader_address: : %08llx\n",
			kparams.bootloader_address);
	VBDEBUG(PREFIX "- bootloader_size:    : %08x\n",
			kparams.bootloader_size);
	VBDEBUG(PREFIX "- partition_guid:     :");
	int i;
	for (i = 0; i < 16; i++)
		VBDEBUG(" %02x", kparams.partition_guid[i]);
	VBDEBUG("\n");
#endif /* VBOOT_DEBUG */

	w = (target == BOOT_RECOVERY) ? RECOVERY_FIRMWARE :
		which[tdata->whoami];
	t = (target == BOOT_RECOVERY) ? RECOVERY_TYPE :
		(tdata->devsw.value ? DEVELOPER_TYPE : NORMAL_TYPE);
	if (crossystem_data_set_active_main_firmware(cdata, w, t))
		VBDEBUG(PREFIX "failed to set active main firmware\n");

	if (target == BOOT_SELF) {
		if (twostop_read_firmware_id(tdata, file, fwid) ||
				crossystem_data_set_fwid(cdata, fwid))
			VBDEBUG(PREFIX "failed to set fwid\n");
	}

	crossystem_data_dump(cdata);
	boot_kernel(&kparams, cdata);
}

void twostop_boot(twostop_t *tdata,
		const void const *fdt,
		firmware_storage_t *file,
		crossystem_data_t *cdata)
{
	boot_target_t target;
	void *gbb, *fw_blob;
	uint32_t fw_size;
	VbCommonParams cparams;

	if (twostop_init(tdata, fdt)) {
		VBDEBUG(PREFIX "failed to init twostop data\n");
		return;
	}

	if (twostop_open_file(tdata, file)) {
		VBDEBUG(PREFIX "failed to open firmware storage\n");
		return;
	}

	if (!(gbb = twostop_read_gbb(tdata, file))) {
		VBDEBUG(PREFIX "failed to allocate gbb\n");
		return;
	}

	if (twostop_init_crossystem_data(tdata, file, gbb, cdata)) {
		VBDEBUG(PREFIX "failed to init crossystem data\n");
		return;
	}

	if (twostop_init_cparams(tdata, gbb, cdata->vbshared_data, &cparams)) {
		VBDEBUG(PREFIX "failed to init cparams\n");
		return;
	}

	target = twostop_select_boot_target(tdata, file, gbb, cdata, &cparams,
			&fw_blob, &fw_size);

	switch (target) {
	case BOOT_RECOVERY:
		if (tdata->whoami != I_AM_RO_FW)
			break;
	case BOOT_SELF:
		twostop_continue_boot(tdata, file, cdata, target, &cparams);
		break;
	case BOOT_RW_A:
	case BOOT_RW_B:
		twostop_goto_rwfw(tdata, cdata, target, fw_blob, fw_size);
		break;
	default:
		VBDEBUG(PREFIX "unknown target: %d\n", target);
		break;
	}
}

int do_vboot_twostop(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	const void const *fdt = gd->blob;
	twostop_t tdata;
	firmware_storage_t file;
	crossystem_data_t cdata;

	lcd_clear();

	twostop_boot(&tdata, fdt, &file, &cdata);

	cold_reboot();
	return 0;
}

U_BOOT_CMD(vboot_twostop, 1, 1, do_vboot_twostop,
		"verified boot twostop firmware", NULL);
