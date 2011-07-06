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
#include <fdt_decode.h>
#include <gbb_header.h> /* for GoogleBinaryBlockHeader */
#include <libfdt.h>
#include <chromeos/common.h>
#include <chromeos/crossystem_data.h>
#include <chromeos/vboot_nvstorage_helper.h> /* for NVCONTEXT_LBA */
#include <linux/string.h>

/* This is used to keep u-boot and kernel in sync */
#define SHARED_MEM_VERSION 1

#define PREFIX "crossystem_data: "

enum {
	CHSW_RECOVERY_BUTTON_PRESSED	= 0x002,
	CHSW_DEVELOPER_MODE_ENABLED	= 0x020,
	CHSW_WRITE_PROTECT_DISABLED	= 0x200
};

int crossystem_data_init(crossystem_data_t *cdata, void *fdt, char *frid,
		uint32_t fmap_data, void *gbb_data, void *nvcxt_raw,
		int write_protect_sw, int recovery_sw, int developer_sw)
{
	GoogleBinaryBlockHeader *gbbh = (GoogleBinaryBlockHeader *)gbb_data;

	VBDEBUG(PREFIX "crossystem data at %p\n", cdata);

	memset(cdata, '\0', sizeof(*cdata));

	cdata->total_size = sizeof(*cdata);

	strcpy(cdata->signature, "CHROMEOS");
	cdata->version = SHARED_MEM_VERSION;

	if (recovery_sw)
		cdata->chsw |= CHSW_RECOVERY_BUTTON_PRESSED;
	if (developer_sw)
		cdata->chsw |= CHSW_DEVELOPER_MODE_ENABLED;
	if (!write_protect_sw)
		cdata->chsw |= CHSW_WRITE_PROTECT_DISABLED;

	strncpy(cdata->frid, frid, ID_LEN);
	strncpy(cdata->hwid, gbb_data + gbbh->hwid_offset, gbbh->hwid_size);

	/* boot reason; always 0 */
	cdata->binf[0] = 0;
	/* ec firmware type: 0=read-only, 1=rewritable; default to 0 */
	cdata->binf[2] = 0;
	/* recovery reason is default to VBNV_RECOVERY_NOT_REQUESTED */
	cdata->binf[4] = VBNV_RECOVERY_NOT_REQUESTED;

	cdata->write_protect_sw = write_protect_sw;
	cdata->recovery_sw = recovery_sw;
	cdata->developer_sw = developer_sw;

	cdata->gpio_port_write_protect_sw = fdt_decode_get_config_int(fdt,
			"gpio_port_write_protect_switch", -1);
	cdata->gpio_port_recovery_sw = fdt_decode_get_config_int(fdt,
			"gpio_port_recovery_switch", -1);
	cdata->gpio_port_developer_sw = fdt_decode_get_config_int(fdt,
			"gpio_port_developer_switch", -1);

	cdata->polarity_write_protect_sw = fdt_decode_get_config_int(fdt,
			"polarity_write_protect_switch", -1);
	cdata->polarity_recovery_sw = fdt_decode_get_config_int(fdt,
			"polarity_recovery_switch", -1);
	cdata->polarity_developer_sw = fdt_decode_get_config_int(fdt,
			"polarity_developer_switch", -1);

	cdata->vbnv[0] = 0;
	cdata->vbnv[1] = VBNV_BLOCK_SIZE;

	cdata->fmap_base = fmap_data;

	cdata->nvcxt_lba = NVCONTEXT_LBA;

	memcpy(cdata->nvcxt_cache, nvcxt_raw, VBNV_BLOCK_SIZE);

	return 0;
}

int crossystem_data_set_fwid(crossystem_data_t *cdata, char *fwid)
{
	strncpy(cdata->fwid, fwid, ID_LEN);
	return 0;
}

int crossystem_data_set_active_main_firmware(crossystem_data_t *cdata,
		int which, int type)
{
	cdata->binf[1] = which;
	cdata->binf[3] = type;
	return 0;
}

int crossystem_data_get_active_main_firmware(crossystem_data_t *cdata)
{
	return cdata->binf[1];
}

int crossystem_data_get_active_main_firmware_type(crossystem_data_t *cdata)
{
	return cdata->binf[3];
}

int crossystem_data_set_recovery_reason(crossystem_data_t *cdata,
		uint32_t reason)
{
	cdata->binf[4] = reason;
	return 0;
}

int crossystem_data_embed_into_fdt(crossystem_data_t *cdata, void *fdt,
		uint32_t *size_ptr)
{
	char path[] = "/crossystem";
	int nodeoffset, err;

	err = fdt_open_into(fdt, fdt,
			fdt_totalsize(fdt) + sizeof(*cdata) + 4096);
	if (err < 0) {
		VBDEBUG(PREFIX "fail to resize fdt: %s\n", fdt_strerror(err));
		return 1;
	}
	*size_ptr = fdt_totalsize(fdt);

	nodeoffset = fdt_add_subnodes_from_path(fdt, 0, path);
	if (nodeoffset < 0) {
		VBDEBUG(PREFIX "fail to create subnode %s: %s\n", path,
				fdt_strerror(nodeoffset));
		return 1;
	}

#define set_scalar_prop(f) \
	err |= fdt_setprop_cell(fdt, nodeoffset, #f, cdata->f)
#define set_array_prop(f) \
	err |= fdt_setprop(fdt, nodeoffset, #f, cdata->f, sizeof(cdata->f))
#define set_string_prop(f) \
	err |= fdt_setprop_string(fdt, nodeoffset, #f, cdata->f)

	err = 0;
	set_scalar_prop(total_size);
	set_string_prop(signature);
	set_scalar_prop(version);
	set_scalar_prop(nvcxt_lba);
	set_array_prop(vbnv);
	set_array_prop(nvcxt_cache);
	set_scalar_prop(write_protect_sw);
	set_scalar_prop(recovery_sw);
	set_scalar_prop(developer_sw);
	set_scalar_prop(gpio_port_write_protect_sw);
	set_scalar_prop(gpio_port_recovery_sw);
	set_scalar_prop(gpio_port_developer_sw);
	set_scalar_prop(polarity_write_protect_sw);
	set_scalar_prop(polarity_recovery_sw);
	set_scalar_prop(polarity_developer_sw);
	set_array_prop(binf);
	set_scalar_prop(chsw);
	set_string_prop(hwid);
	set_string_prop(fwid);
	set_string_prop(frid);
	set_scalar_prop(fmap_base);
	set_array_prop(vbshared_data);

#undef set_scalar_prop
#undef set_array_prop
#undef set_string_prop

	if (err)
		VBDEBUG(PREFIX "fail to store all properties into fdt\n");
	return err;
}

void crossystem_data_dump(crossystem_data_t *cdata)
{
#ifdef VBOOT_DEBUG /* decleare inside ifdef so that compiler doesn't complain */
	int i;

#define _p(format, field) \
	VBDEBUG("crossystem_data_dump: %-20s: " format "\n", #field, cdata->field)
	_p("%08x",	total_size);
	_p("\"%s\"",	signature);
	_p("%d",	version);
	_p("%08llx",	nvcxt_lba);
	_p("%08x",	vbnv[0]);
	_p("%08x",	vbnv[1]);
	VBDEBUG("crossystem_data_dump: nvcxt_cache: ");
	for (i = 0; i < VBNV_BLOCK_SIZE; i++)
		VBDEBUG("%02x", cdata->nvcxt_cache[i]);
	VBDEBUG("\n");
	_p("%d",	write_protect_sw);
	_p("%d",	recovery_sw);
	_p("%d",	developer_sw);
	_p("%d",	gpio_port_write_protect_sw);
	_p("%d",	gpio_port_recovery_sw);
	_p("%d",	gpio_port_developer_sw);
	_p("%d",	polarity_write_protect_sw);
	_p("%d",	polarity_recovery_sw);
	_p("%d",	polarity_developer_sw);
	_p("%08x",	binf[0]);
	_p("%08x",	binf[1]);
	_p("%08x",	binf[2]);
	_p("%08x",	binf[3]);
	_p("%08x",	binf[4]);
	_p("%08x",	chsw);
	_p("\"%s\"",	hwid);
	_p("\"%s\"",	fwid);
	_p("\"%s\"",	frid);
	_p("%08x",	fmap_base);
#undef _p
#endif
}
