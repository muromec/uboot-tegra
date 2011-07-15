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

#define CROSSYSTEM_DATA_ADDRESS 0x01000000
#define GBB_ADDRESS             0x01010000

typedef enum {
	FIRMWARE_TYPE_READONLY       = 0,
	FIRMWARE_TYPE_READWRITE      = 1
} firmware_type_t;

typedef enum {
	SELECTION_NONE               = 0,
	SELECTION_REBOOT             = 1,
	SELECTION_FIRMWARE_READWRITE = 2,
	SELECTION_KERNEL             = 3,
	SELECTION_COMMAND_LINE       = 4
} selection_t;

typedef struct {
	firmware_type_t type;
	struct fdt_twostop_fmap fmap;
	cros_gpio_t wpsw, recsw, devsw;
} twostop_t;

#ifdef VBOOT_DEBUG
static const char const *str_type[] = {
	"FIRMWARE_TYPE_READONLY",
	"FIRMWARE_TYPE_READWRITE"
};

static const char const *str_selection[] = {
	"SELECTION_NONE",
	"SELECTION_REBOOT",
	"SELECTION_FIRMWARE_READWRITE",
	"SELECTION_KERNEL",
	"SELECTION_COMMAND_LINE"
};

static const char const *str_vbselect[] = {
	"VB_SELECT_FIRMWARE_RECOVERY",
	"VB_SELECT_FIRMWARE_A",
	"VB_SELECT_FIRMWARE_B",
	"VB_SELECT_FIRMWARE_READONLY"
};
#endif /* VBOOT_DEBUG */

int twostop_init(twostop_t *tdata, const void const *fdt)
{
	if (is_cold_boot())
		tdata->type = FIRMWARE_TYPE_READONLY;
	else
		tdata->type = FIRMWARE_TYPE_READWRITE;

	VBDEBUG(PREFIX "type: %s\n", str_type[tdata->type]);

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
		enum VbSelectFirmware_t vbselect,
		firmware_storage_t *file,
		char *firmware_id)
{
	const struct fdt_twostop_fmap const *fmap = &tdata->fmap;
	const struct fdt_fmap_entry *entry;

	if (tdata->type == FIRMWARE_TYPE_READONLY ||
			vbselect == VB_SELECT_FIRMWARE_READONLY ||
			vbselect == VB_SELECT_FIRMWARE_RECOVERY)
		entry = &fmap->readonly.firmware_id;
	else if (vbselect == VB_SELECT_FIRMWARE_A)
		entry = &fmap->readwrite_a.firmware_id;
	else
		entry = &fmap->readwrite_b.firmware_id;

	return file->read(file, entry->offset, entry->length,
			firmware_id);
}

int twostop_read_gbb(twostop_t *tdata, firmware_storage_t *file, void *gbb)
{
	const struct fdt_twostop_fmap const *fmap = &tdata->fmap;
	return file->read(file,
			fmap->readonly.gbb.offset, fmap->readonly.gbb.length,
			gbb);
}

int twostop_init_crossystem_data(twostop_t *tdata,
		firmware_storage_t *file,
		void *gbb,
		crossystem_data_t *cdata)
{
	const struct fdt_twostop_fmap const *fmap = &tdata->fmap;
	char frid[ID_LEN];
	uint8_t nvcxt_raw[VBNV_BLOCK_SIZE];

	if (twostop_read_firmware_id(tdata, VB_SELECT_FIRMWARE_READONLY, file,
				frid)) {
		VBDEBUG(PREFIX "failed to read firmware ID\n");
		return -1;
	}

	VBDEBUG(PREFIX "read-only firmware id: \"%s\"\n", frid);

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
				frid,
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

int twostop_init_cparams(twostop_t *tdata,
		void *gbb,
		void *shared_data_blob,
		VbCommonParams *cparams)
{
	const struct fdt_twostop_fmap const *fmap = &tdata->fmap;

	cparams->gbb_data = gbb;
	cparams->gbb_size = fmap->readonly.gbb.length;
	cparams->shared_data_blob = shared_data_blob;
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
		void *vblock;
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
	hasher_state_t *s = (hasher_state_t *)cparams->caller_context;
	const int i = (firmware_index == VB_SELECT_FIRMWARE_A ? 0 : 1);
	firmware_storage_t *file = s->file;


	if (firmware_index != VB_SELECT_FIRMWARE_A &&
			firmware_index != VB_SELECT_FIRMWARE_B) {
		VBDEBUG(PREFIX "incorrect firmware index: %d\n",
				firmware_index);
		return 1;
	}

	/*
	 * The key block is verified now. It is safe to infer the actual
	 * firmware body size from the key block.
	 */
	s->fw[i].size = firmware_body_size((uint32_t)s->fw[i].vblock);

	if (file->read(file, s->fw[i].offset, s->fw[i].size, s->fw[i].cache)) {
		VBDEBUG(PREFIX "fail to read firmware: %d\n", firmware_index);
		return 1;
	}

	VbUpdateFirmwareBodyHash(cparams, s->fw[i].cache, s->fw[i].size);
	return 0;
}

int twostop_check_recovery(twostop_t *tdata, VbCommonParams *cparams)
{
	VbError_t err;
	VbInitParams iparams;

	iparams.flags = VB_INIT_FLAG_RO_NORMAL_SUPPORT;
	if (tdata->wpsw.value)
		iparams.flags |= VB_INIT_FLAG_WP_ENABLED;
	if (tdata->recsw.value)
		iparams.flags |= VB_INIT_FLAG_REC_BUTTON_PRESSED;
	if (tdata->devsw.value)
		iparams.flags |= VB_INIT_FLAG_DEV_SWITCH_ON;
	VBDEBUG(PREFIX "iparams.flags: %08x\n", iparams.flags);

	if ((err = VbInit(cparams, &iparams))) {
		VBDEBUG(PREFIX "VbInit: %d\n", err);
		return SELECTION_REBOOT;
	}

	/* TODO(waihong) implement clear unused RAM */
	/* if (iparams.out_flags & VB_INIT_OUT_CLEAR_RAM)
		; */

	/* Do not check iparams.out_flags & VB_INIT_OUT_ENABLE_RECOVERY */
	return SELECTION_NONE;
}

enum VbSelectFirmware_t twostop_make_selection(twostop_t *tdata,
		firmware_storage_t *file,
		VbCommonParams *cparams,
		void **fw_blob_ptr,
		uint32_t *fw_size_ptr)
{
	const struct fdt_twostop_fmap const *fmap = &tdata->fmap;
	enum VbSelectFirmware_t vbselect = VB_SELECT_FIRMWARE_RECOVERY;
	VbError_t err;
	uint32_t vlength;
	VbSelectFirmwareParams fparams;
	hasher_state_t s;

	memset(&fparams, '\0', sizeof(fparams));

	vlength = fmap->readwrite_a.vblock.length;
	assert(fmap->readwrite_a.vblock.length ==
			fmap->readwrite_b.vblock->vblock.length);

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

	if (file->read(file, fmap->readwrite_a.vblock.offset, vlength,
				fparams.verification_block_A)) {
		VBDEBUG(PREFIX "fail to read vblock A\n");
		goto out;
	}
	if (file->read(file, fmap->readwrite_b.vblock.offset, vlength,
				fparams.verification_block_B)) {
		VBDEBUG(PREFIX "fail to read vblock B\n");
		goto out;
	}

	s.fw[0].vblock = fparams.verification_block_A;
	s.fw[1].vblock = fparams.verification_block_B;

	s.fw[0].offset = fmap->readwrite_a.boot.offset;
	s.fw[1].offset = fmap->readwrite_b.boot.offset;

	s.fw[0].size = fmap->readwrite_a.boot.length;
	s.fw[1].size = fmap->readwrite_b.boot.length;

	s.fw[0].cache = memalign(CACHE_LINE_SIZE, s.fw[0].size);
	if (!s.fw[0].cache) {
		VBDEBUG(PREFIX "failed to allocate cache A\n");
		goto out;
	}
	s.fw[1].cache = memalign(CACHE_LINE_SIZE, s.fw[0].size);
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
	vbselect = fparams.selected_firmware;

out:

	free(fparams.verification_block_A);
	free(fparams.verification_block_B);

	if (vbselect == VB_SELECT_FIRMWARE_A) {
		*fw_blob_ptr = s.fw[0].cache;
		*fw_size_ptr = s.fw[0].size;
		free(s.fw[1].cache);
	} else if (vbselect == VB_SELECT_FIRMWARE_B) {
		*fw_blob_ptr = s.fw[1].cache;
		*fw_size_ptr = s.fw[1].size;
		free(s.fw[0].cache);
	}

	return vbselect;
}

selection_t twostop_select_main_firmware(twostop_t *tdata,
		void *gbb, crossystem_data_t *cdata, void *shared_data_blob,
		void **fw_blob_ptr, uint32_t *fw_size_ptr)
{
	selection_t selection = SELECTION_REBOOT, s;
	enum VbSelectFirmware_t vbselect;
	int w, t;
	firmware_storage_t file;
	VbCommonParams cparams;
	char fwid[ID_LEN];

	if (twostop_open_file(tdata, &file)) {
		VBDEBUG(PREFIX "failed to open firmware storage\n");
		return SELECTION_REBOOT;
	}

	if (twostop_read_gbb(tdata, &file, gbb)) {
		VBDEBUG(PREFIX "failed to read gbb\n");
		goto done_selection;
	}

	if (twostop_init_crossystem_data(tdata, &file, gbb, cdata)) {
		VBDEBUG(PREFIX "failed to init crossystem data\n");
		goto done_selection;
	}

	if (twostop_init_cparams(tdata, gbb, shared_data_blob, &cparams)) {
		VBDEBUG(PREFIX "failed to init cparams\n");
		goto done_selection;
	}

	s = twostop_check_recovery(tdata, &cparams);
	if (s == SELECTION_KERNEL) {
		VBDEBUG(PREFIX "boot to recovery kernel\n");
		if (crossystem_data_set_active_main_firmware(cdata,
					RECOVERY_FIRMWARE, RECOVERY_TYPE))
			VBDEBUG(PREFIX "failed to set active main firmware\n");
	}
	if (s != SELECTION_NONE) {
		selection = s;
		goto done_selection;
	}

	vbselect = twostop_make_selection(tdata, &file, &cparams,
			fw_blob_ptr, fw_size_ptr);

	VBDEBUG(PREFIX "vbselect: %s\n", str_vbselect[vbselect]);

	if (vbselect == VB_SELECT_FIRMWARE_READONLY ||
			vbselect == VB_SELECT_FIRMWARE_RECOVERY)
		selection = SELECTION_KERNEL;
	else
		selection = SELECTION_FIRMWARE_READWRITE;

	/* set extra data of readwrite firmware to crossystem data */
	if (selection == SELECTION_FIRMWARE_READWRITE) {
		w = (vbselect == VB_SELECT_FIRMWARE_A) ?
			REWRITABLE_FIRMWARE_A : REWRITABLE_FIRMWARE_B;
		t = tdata->devsw.value ? DEVELOPER_TYPE : NORMAL_TYPE;

		if (crossystem_data_set_active_main_firmware(cdata, w, t))
			VBDEBUG(PREFIX "failed to set active main firmware\n");

		if (twostop_read_firmware_id(tdata, vbselect, &file, fwid) ||
				crossystem_data_set_fwid(cdata, fwid))
			VBDEBUG(PREFIX "failed to set fwid\n");
		else
			VBDEBUG(PREFIX "readwrite firmware id: \"%s\"\n", fwid);
	}

done_selection:

	file.close(&file);
	return selection;
}

void twostop_jump(twostop_t *tdata,
		crossystem_data_t *cdata, void *fw_blob, uint32_t fw_size)
{
	VBDEBUG(PREFIX "Jumping to second-stage firmware at %#x, size %#x\n",
		CONFIG_SYS_TEXT_BASE, fw_size);

	memmove((void *)CONFIG_SYS_TEXT_BASE, fw_blob, fw_size);

	/* TODO go to readwrite firmware without extra cleanup */
	cleanup_before_linux();

	((void(*)(void))CONFIG_SYS_TEXT_BASE)();
}

void twostop_boot_kernel(twostop_t *tdata,
		crossystem_data_t *cdata, VbCommonParams *cparams)
{
	VbError_t err;
	VbSelectAndLoadKernelParams kparams;

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

	crossystem_data_dump(cdata);
	boot_kernel(&kparams, cdata);
}

int gbb_check_integrity(void *gbb)
{
	return memcmp(gbb, "$GBB", 5);
}

selection_t twostop_boot(const void const *fdt)
{
	selection_t selection = SELECTION_REBOOT;
	twostop_t tdata;
	crossystem_data_t *cdata = (crossystem_data_t *)CROSSYSTEM_DATA_ADDRESS;
	void *gbb = (void *)GBB_ADDRESS;
	void *shared_data_blob = cdata->vbshared_data;
	VbCommonParams cparams;
	void *fw_blob = NULL;
	uint32_t fw_size = 0;

	if (twostop_init(&tdata, fdt)) {
		VBDEBUG(PREFIX "failed to init twostop data\n");
		goto done_selection;
	}

	if (tdata.type == FIRMWARE_TYPE_READONLY) {
		selection = twostop_select_main_firmware(&tdata,
				gbb, cdata, shared_data_blob,
				&fw_blob, &fw_size);
		/*
		 * TODO: if selection is SELECTION_KERNEL, we have to load
		 * normal boot path from SPI ...
		 */
	} else {
		/* Okay, I am readwrite firmware ... */
		if (crossystem_data_check_integrity(cdata))
			VBDEBUG(PREFIX "invalid crossystem_data\n");
		else if (gbb_check_integrity(gbb))
			VBDEBUG(PREFIX "invalid gbb\n");
		else
			selection = SELECTION_KERNEL;
	}

done_selection:

	VBDEBUG(PREFIX "selection: %s\n", str_selection[selection]);

	if (selection == SELECTION_FIRMWARE_READWRITE) {
		twostop_jump(&tdata, cdata, fw_blob, fw_size);
		selection = SELECTION_REBOOT; /* in case jump failed */
	}

	if (selection == SELECTION_KERNEL) {
		if (twostop_init_cparams(&tdata, gbb, shared_data_blob,
					&cparams))
			VBDEBUG(PREFIX "failed to init cparams\n");
		else
			twostop_boot_kernel(&tdata, cdata, &cparams);
		selection = SELECTION_REBOOT; /* in case boot failed */
	}

	return selection;
}

int do_vboot_twostop(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	const void const *fdt = gd->blob;
	selection_t selection;

	lcd_clear();

	selection = twostop_boot(fdt);

	if (selection == SELECTION_COMMAND_LINE)
		return 0;

	cold_reboot();
	return 0;
}

U_BOOT_CMD(vboot_twostop, 1, 1, do_vboot_twostop,
		"verified boot twostop firmware", NULL);
