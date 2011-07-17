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

/*
 * The current design of twostop firmware, if we use x86 firmware design as a
 * metaphor, twostop firmware has:
 * - One bootstub that select one of the main firmware
 * - One read-only main firmware which can do recovery and normal/dev boot
 * - Two readwrite main firmware which are virtually identical to x86 readwrite
 *   firmware, that is, they only have code path to normal/dev boot
 *
 * The readwrite main firmware does not reinitialize itself (this differs to the
 * prior twostop design). As a consequence, a fixed protocol between bootstub
 * and readwrite main firmware must be defined, specifying which hardware need
 * or need not be initialized, what parameters are passed from bootstub to main
 * firmware, and etc.
 *
 * The parameters are:
 * - VbSharedData
 * - GBB
 * - Crossystem data
 * Note that the format of the parameters must be versioned so that newer
 * readwrite firmware can still work with old bootstub.
 */

/*
 * TODO The current readwrite firmware is a full-fledged U-Boot. As a
 * consequence, it will reinitialize most of the device that the bootstub
 * already initialized. We should eliminate such reinitialization not just
 * because it is slow, but also because it could be problematic.
 *
 * Given that, we must define a clear protocol specifying which device are
 * initialized by the bootstub, and which are by the readwrite firmware.
 */

/*
 * We use fixed memory address for the parameters --- this should be simpler
 * than atags or a register holding the address of the parameters. Besides,
 * Chrome OS kernel is loaded to a fixed location, we could simply use this
 * location as our anchor for the location of the parameters.
 */
/*
 * Layout: first, the kernel buffer, then the crossystem data (and the
 * VbSharedData), and finally, the GBB.
 */
#define CROSSYSTEM_DATA_ADDRESS \
	(CONFIG_CHROMEOS_KERNEL_LOADADDR + CONFIG_CHROMEOS_KERNEL_BUFSIZE)
#define CROSSYSTEM_DATA_MAXSIZE 0x8000
#define GBB_ADDRESS (CROSSYSTEM_DATA_ADDRESS + CROSSYSTEM_DATA_MAXSIZE)

DECLARE_GLOBAL_DATA_PTR;

/*
 * A sentinel value indicates an error occured when selecting main firmware or
 * kernel. This value must be unique to enum VbSelectFirmware_t.
 */
#define VB_SELECT_ERROR        0xff

/*
 * A dummy value indicates that VbSelectAndLoadKernel requires U-Boot to show up
 * a command line. This value must be unique to enum VbSelectFirmware_t.
 */
/* TODO Implement the "returning to command line" in vboot_reference. */
#define VB_SELECT_COMMAND_LINE 0xfe

#ifdef VBOOT_DEBUG
const char *str_selection(uint32_t selection)
{
	static const char const *str[] = {
		"VB_SELECT_FIRMWARE_RECOVERY",
		"VB_SELECT_FIRMWARE_A",
		"VB_SELECT_FIRMWARE_B",
		"VB_SELECT_FIRMWARE_READONLY"
	};

	if (selection == VB_SELECT_ERROR)
		return "VB_SELECT_ERROR";
	else if (selection == VB_SELECT_COMMAND_LINE)
		return "VB_SELECT_COMMAND_LINE";
	else
		return str[selection];
}
#endif /* VBOOT_DEBUG */

int twostop_init_cparams(struct fdt_twostop_fmap *fmap,
		void *gbb,
		void *vb_shared_data,
		VbCommonParams *cparams)
{
	cparams->gbb_data = gbb;
	cparams->gbb_size = fmap->readonly.gbb.length;
	cparams->shared_data_blob = vb_shared_data;
	cparams->shared_data_size = VB_SHARED_DATA_REC_SIZE;

#define P(format, field) \
	VBDEBUG(PREFIX "- %-20s: " format "\n", #field, cparams->field)

	VBDEBUG(PREFIX "cparams:\n");
	P("%p",   gbb_data);
	P("%08x", gbb_size);
	P("%p",   shared_data_blob);
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

/* This can only be called after key block has been verified */
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
	hasher_state_t *s = cparams->caller_context;
	const int i = (firmware_index == VB_SELECT_FIRMWARE_A ? 0 : 1);
	firmware_storage_t *file = s->file;

	if (firmware_index != VB_SELECT_FIRMWARE_A &&
			firmware_index != VB_SELECT_FIRMWARE_B) {
		VBDEBUG(PREFIX "incorrect firmware index: %d\n",
				firmware_index);
		return 1;
	}

	/*
	 * The key block has been verified. It is safe now to infer the actual
	 * firmware body size from the key block.
	 */
	/*
	 * TODO: This is not 64-bit safe. On machine that has 64-bit address,
	 * casting address to 32-bit loses data. But it is okay on ARM.
	 */
	s->fw[i].size = firmware_body_size((uint32_t)s->fw[i].vblock);

	if (file->read(file, s->fw[i].offset, s->fw[i].size, s->fw[i].cache)) {
		VBDEBUG(PREFIX "fail to read firmware: %d\n", firmware_index);
		return 1;
	}

	VbUpdateFirmwareBodyHash(cparams, s->fw[i].cache, s->fw[i].size);
	return 0;
}

VbError_t twostop_init_vboot_library(crossystem_data_t *cdata,
		VbCommonParams *cparams)
{
	VbError_t err;
	VbInitParams iparams;

	iparams.flags = VB_INIT_FLAG_RO_NORMAL_SUPPORT;
	if (cdata->write_protect_sw)
		iparams.flags |= VB_INIT_FLAG_WP_ENABLED;
	if (cdata->recovery_sw)
		iparams.flags |= VB_INIT_FLAG_REC_BUTTON_PRESSED;
	if (cdata->developer_sw)
		iparams.flags |= VB_INIT_FLAG_DEV_SWITCH_ON;
	VBDEBUG(PREFIX "iparams.flags: %08x\n", iparams.flags);

	if ((err = VbInit(cparams, &iparams))) {
		VBDEBUG(PREFIX "VbInit: %u\n", err);
		return err;
	}

	/* TODO(waihong) implement clear unused RAM */
	/* if (iparams.out_flags & VB_INIT_OUT_CLEAR_RAM)
		; */

	/* No need to check iparams.out_flags & VB_INIT_OUT_ENABLE_RECOVERY */

	return VBERROR_SUCCESS;
}

uint32_t twostop_make_selection(struct fdt_twostop_fmap *fmap,
		firmware_storage_t *file,
		VbCommonParams *cparams,
		void **fw_blob_ptr,
		uint32_t *fw_size_ptr)
{
	uint32_t selection = VB_SELECT_ERROR;
	VbError_t err;
	uint32_t vlength;
	VbSelectFirmwareParams fparams;
	hasher_state_t s;

	memset(&fparams, '\0', sizeof(fparams));

	vlength = fmap->readwrite_a.vblock.length;
	assert(vlength == fmap->readwrite_b.vblock->vblock.length);

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
	selection = fparams.selected_firmware;

out:

	free(fparams.verification_block_A);
	free(fparams.verification_block_B);

	if (selection == VB_SELECT_FIRMWARE_A) {
		*fw_blob_ptr = s.fw[0].cache;
		*fw_size_ptr = s.fw[0].size;
		free(s.fw[1].cache);
	} else if (selection == VB_SELECT_FIRMWARE_B) {
		*fw_blob_ptr = s.fw[1].cache;
		*fw_size_ptr = s.fw[1].size;
		free(s.fw[0].cache);
	}

	return selection;
}

uint32_t twostop_select_and_set_main_firmware(struct fdt_twostop_fmap *fmap,
		firmware_storage_t *file,
		void *gbb,
		crossystem_data_t *cdata,
		void *vb_shared_data,
		void **fw_blob_ptr, uint32_t *fw_size_ptr)
{
	uint32_t selection;
	uint32_t id_offset = 0, id_length = 0;
	int w, t;
	char fwid[ID_LEN];
	VbCommonParams cparams;

	if (twostop_init_cparams(fmap, gbb, vb_shared_data, &cparams)) {
		VBDEBUG(PREFIX "failed to init cparams\n");
		return VB_SELECT_ERROR;
	}

	if (twostop_init_vboot_library(cdata, &cparams) != VBERROR_SUCCESS) {
		VBDEBUG(PREFIX "failed to init vboot library\n");
		return VB_SELECT_ERROR;
	}

	selection = twostop_make_selection(fmap, file, &cparams,
			fw_blob_ptr, fw_size_ptr);

	VBDEBUG(PREFIX "selection: %s\n", str_selection(selection));

	if (selection == VB_SELECT_ERROR)
		return VB_SELECT_ERROR;

	w = RECOVERY_FIRMWARE;
	t = RECOVERY_TYPE;
	switch(selection) {
	case VB_SELECT_FIRMWARE_RECOVERY:
		w = RECOVERY_FIRMWARE;
		t = RECOVERY_TYPE;
		id_offset = fmap->readonly.firmware_id.offset;
		id_length = fmap->readonly.firmware_id.length;
		break;
	case VB_SELECT_FIRMWARE_A:
		w = REWRITABLE_FIRMWARE_A;
		t = cdata->developer_sw ? DEVELOPER_TYPE : NORMAL_TYPE;
		id_offset = fmap->readwrite_a.firmware_id.offset;
		id_length = fmap->readwrite_a.firmware_id.length;
		break;
	case VB_SELECT_FIRMWARE_B:
		w = REWRITABLE_FIRMWARE_B;
		t = cdata->developer_sw ? DEVELOPER_TYPE : NORMAL_TYPE;
		id_offset = fmap->readwrite_b.firmware_id.offset;
		id_length = fmap->readwrite_b.firmware_id.length;
		break;
	case VB_SELECT_FIRMWARE_READONLY:
		w = READONLY_FIRMWARE;
		t = cdata->developer_sw ? DEVELOPER_TYPE : NORMAL_TYPE;
		id_offset = fmap->readonly.firmware_id.offset;
		id_length = fmap->readonly.firmware_id.length;
		break;
	default:
		VBDEBUG(PREFIX "impossible selection value: %d\n", selection);
		assert(0);
	}

	if (id_length > sizeof(fwid)) {
		VBDEBUG(PREFIX "firmware id is too long: %u > %u\n",
				id_length, sizeof(fwid));
		return VB_SELECT_ERROR;
	}

	if (id_length && file->read(file, id_offset, id_length, fwid)) {
		VBDEBUG(PREFIX "failed to read active firmware id\n");
		fwid[0] = '\0';
	}

	VBDEBUG(PREFIX "active main firmware      : %d\n", w);
	VBDEBUG(PREFIX "active main firmware type : %d\n", t);
	VBDEBUG(PREFIX "active main firmware id   : \"%s\"\n", fwid);

	if (crossystem_data_set_active_main_firmware(cdata, w, t)) {
		VBDEBUG(PREFIX "failed to set active main firmware\n");
		return VB_SELECT_ERROR;
	}

	if (crossystem_data_set_fwid(cdata, fwid)) {
		VBDEBUG(PREFIX "failed to set active main firmware id\n");
		return VB_SELECT_ERROR;
	}

	return selection;
}

uint32_t twostop_jump(crossystem_data_t *cdata, void *fw_blob, uint32_t fw_size)
{
	VBDEBUG(PREFIX "jump to readwrite main firmware at %#x, size %#x\n",
			CONFIG_SYS_TEXT_BASE, fw_size);

	/*
	 * TODO: This version of U-Boot must be loaded at a fixed location. It
	 * could be problematic if newer version U-Boot changed this address.
	 */
	memmove((void *)CONFIG_SYS_TEXT_BASE, fw_blob, fw_size);

	/*
	 * TODO We need to reach the Point of Unification here, but I am not
	 * sure whether the following function call flushes L2 cache or not. If
	 * it does, we should avoid that.
	 */
	cleanup_before_linux();

	((void(*)(void))CONFIG_SYS_TEXT_BASE)();

	/* It is an error if readwrite firmware returns */
	return VB_SELECT_ERROR;
}

int twostop_init(const void const *fdt,
		struct fdt_twostop_fmap *fmap,
		firmware_storage_t *file,
		void *gbb,
		crossystem_data_t *cdata,
		void *vb_shared_data)
{
	cros_gpio_t wpsw, recsw, devsw;
	char frid[ID_LEN];
	uint8_t nvcxt_raw[VBNV_BLOCK_SIZE];
	int ret = -1;

	if (cros_gpio_fetch(CROS_GPIO_WPSW, fdt, &wpsw) ||
			cros_gpio_fetch(CROS_GPIO_RECSW, fdt, &recsw) ||
			cros_gpio_fetch(CROS_GPIO_DEVSW, fdt, &devsw)) {
		VBDEBUG(PREFIX "failed to fetch gpio\n");
		return -1;
	}
	cros_gpio_dump(&wpsw);
	cros_gpio_dump(&recsw);
	cros_gpio_dump(&devsw);

	if (fdt_decode_twostop_fmap(fdt, fmap)) {
		VBDEBUG(PREFIX "failed to decode fmap\n");
		return -1;
	}
	dump_fmap(fmap);

	/* Read NvStorage */
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

	/* We revert the decision of using firmware_storage_open_twostop() */
	if (firmware_storage_open_spi(file)) {
		VBDEBUG(PREFIX "failed to open firmware storage\n");
		return -1;
	}

	/* Read read-only firmware ID */
	if (file->read(file, fmap->readonly.firmware_id.offset,
				fmap->readonly.firmware_id.length, frid)) {
		VBDEBUG(PREFIX "failed to read firmware ID\n");
		goto out;
	}
	VBDEBUG(PREFIX "read-only firmware id: \"%s\"\n", frid);

	/* Load gbb blob */
	if (file->read(file, fmap->readonly.gbb.offset,
				fmap->readonly.gbb.length, gbb)) {
		VBDEBUG(PREFIX "failed to read gbb\n");
		goto out;
	}

	/* Initialize crossystem data */
	if (crossystem_data_init(cdata,
				frid,
				fmap->readonly.fmap.offset,
				gbb,
				nvcxt_raw,
				&wpsw,
				&recsw,
				&devsw)) {
		VBDEBUG(PREFIX "failed to init crossystem data\n");
		goto out;
	}

	ret = 0;

out:
	if (ret)
		file->close(file);

	return ret;
}

uint32_t twostop_main_firmware(struct fdt_twostop_fmap *fmap,
		void *gbb,
		crossystem_data_t *cdata,
		void *vb_shared_data)
{
	VbError_t err;
	VbSelectAndLoadKernelParams kparams;
	VbCommonParams cparams;

	if (twostop_init_cparams(fmap, gbb, vb_shared_data, &cparams)) {
		VBDEBUG(PREFIX "failed to init cparams\n");
		return VB_SELECT_ERROR;
	}

	kparams.kernel_buffer = (void *)CONFIG_CHROMEOS_KERNEL_LOADADDR;
	kparams.kernel_buffer_size = CONFIG_CHROMEOS_KERNEL_BUFSIZE;

	VBDEBUG(PREFIX "kparams:\n");
	VBDEBUG(PREFIX "- kernel_buffer:      : %p\n", kparams.kernel_buffer);
	VBDEBUG(PREFIX "- kernel_buffer_size: : %08x\n",
			kparams.kernel_buffer_size);

        if ((err = VbSelectAndLoadKernel(&cparams, &kparams))) {
		VBDEBUG(PREFIX "VbSelectAndLoadKernel: %d\n", err);
		return VB_SELECT_ERROR;
	}

	/* TODO: Check kparams.out_flags and return VB_SELECT_COMMAND_LINE. */

	VBDEBUG(PREFIX "kparams:\n");
	VBDEBUG(PREFIX "- disk_handle:        : %p\n", kparams.disk_handle);
	VBDEBUG(PREFIX "- partition_number:   : %08x\n",
			kparams.partition_number);
	VBDEBUG(PREFIX "- bootloader_address: : %08llx\n",
			kparams.bootloader_address);
	VBDEBUG(PREFIX "- bootloader_size:    : %08x\n",
			kparams.bootloader_size);
	VBDEBUG(PREFIX "- partition_guid:     :");
#ifdef VBOOT_DEBUG
	int i;
	for (i = 0; i < 16; i++)
		VBDEBUG(" %02x", kparams.partition_guid[i]);
	VBDEBUG("\n");
#endif /* VBOOT_DEBUG */

	crossystem_data_dump(cdata);
	boot_kernel(&kparams, cdata);

	/* It is an error if boot_kenel returns */
	return VB_SELECT_ERROR;
}

uint32_t twostop_boot(const void const *fdt)
{
	struct fdt_twostop_fmap fmap;
	firmware_storage_t file;
	crossystem_data_t *cdata = (crossystem_data_t *)CROSSYSTEM_DATA_ADDRESS;
	void *gbb = (void *)GBB_ADDRESS;
	void *vb_shared_data = cdata->vbshared_data;
	void *fw_blob = NULL;
	uint32_t fw_size = 0;
	uint32_t selection;

	if (twostop_init(fdt, &fmap, &file, gbb, cdata, vb_shared_data)) {
		VBDEBUG(PREFIX "failed to init twostop boot\n");
		return VB_SELECT_ERROR;
	}

	selection = twostop_select_and_set_main_firmware(&fmap, &file,
			gbb, cdata, vb_shared_data,
			&fw_blob, &fw_size);
	VBDEBUG(PREFIX "selection of bootstub: %s\n", str_selection(selection));

	file.close(&file); /* We don't care even if it fails */

	/* Don't we bother to free(fw_blob) if there was an error? */
	if (selection == VB_SELECT_ERROR)
		return VB_SELECT_ERROR;

	if (selection == VB_SELECT_FIRMWARE_A ||
			selection == VB_SELECT_FIRMWARE_B)
		return twostop_jump(cdata, fw_blob, fw_size);

	assert(selection == VB_SELECT_FIRMWARE_READONLY ||
			selection == VB_SELECT_FIRMWARE_RECOVERY);

	/*
	 * TODO: Now, load drivers for rec/normal/dev main firmware.
	 * We should be able to use out_flags from VbInit to know which boot
	 * mode and how many drivers we need.
	 */

	selection = twostop_main_firmware(&fmap, gbb, cdata, vb_shared_data);
	VBDEBUG(PREFIX "selection of read-only main firmware: %s\n",
			str_selection(selection));

	if (selection == VB_SELECT_ERROR)
		return VB_SELECT_ERROR;

	assert(selection == VB_SELECT_COMMAND_LINE);

	/*
	 * TODO: Now, load all other drivers, such as networking, as we are
	 * returning back to the command line.
	 */

	return VB_SELECT_COMMAND_LINE;
}

int crossystem_data_check_integrity(crossystem_data_t *cdata)
{
	/* TODO implement crossystem data integrity check */
	return 0;
}

int gbb_check_integrity(uint8_t *gbb)
{
	if (gbb[0] == '$' && gbb[1] == 'G' && gbb[2] == 'B' && gbb[3] == 'B')
		return 0;
	else
		return 1;
}

uint32_t twostop_readwrite_main_firmware(const void const *fdt)
{
	struct fdt_twostop_fmap fmap;
	crossystem_data_t *cdata = (crossystem_data_t *)CROSSYSTEM_DATA_ADDRESS;
	void *gbb = (void *)GBB_ADDRESS;
	void *vb_shared_data = cdata->vbshared_data;

	/* Newer readwrite firmware should check version of the data blobs */

	if (crossystem_data_check_integrity(cdata)) {
		VBDEBUG(PREFIX "invalid crossystem data\n");
		return VB_SELECT_ERROR;
	}

	if (gbb_check_integrity(gbb)) {
		VBDEBUG(PREFIX "invalid gbb\n");
		return VB_SELECT_ERROR;
	}

	if (fdt_decode_twostop_fmap(fdt, &fmap)) {
		VBDEBUG(PREFIX "failed to decode fmap\n");
		return VB_SELECT_ERROR;
	}
	dump_fmap(&fmap);

	/* TODO Now, initialize device that bootstub did not initialize */

	return twostop_main_firmware(&fmap, gbb, cdata, vb_shared_data);
}

int do_vboot_twostop(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	const void const *fdt = gd->blob;
	uint32_t selection;

	/*
	 * TODO: We should clear screen later if we load graphics optionally.
	 * In normal mode, we don't need to load graphics driver and clear
	 * screen.
	 */
	lcd_clear();

	/* If it is a cold boot, we are in read-only firmware */
	if (is_cold_boot())
		selection = twostop_boot(fdt);
	else
		selection = twostop_readwrite_main_firmware(fdt);

	VBDEBUG(PREFIX "selection of main firmware: %s\n",
			str_selection(selection));

	if (selection == VB_SELECT_COMMAND_LINE)
		return 0;

	assert(selection == VB_SELECT_ERROR);

	cold_reboot();
	return 0;
}

U_BOOT_CMD(vboot_twostop, 1, 1, do_vboot_twostop,
		"verified boot twostop firmware", NULL);
