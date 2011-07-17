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

int crossystem_data_init(crossystem_data_t *cdata, char *frid,
		uint32_t fmap_data, void *gbb_data, void *nvcxt_raw,
		cros_gpio_t *wpsw, cros_gpio_t *recsw, cros_gpio_t *devsw)
{
	GoogleBinaryBlockHeader *gbbh = (GoogleBinaryBlockHeader *)gbb_data;

	VBDEBUG(PREFIX "crossystem data at %p\n", cdata);

	memset(cdata, '\0', sizeof(*cdata));

	cdata->total_size = sizeof(*cdata);

	strcpy(cdata->signature, "CHROMEOS");
	cdata->version = SHARED_MEM_VERSION;

	if (recsw->value)
		cdata->chsw |= CHSW_RECOVERY_BUTTON_PRESSED;
	if (devsw->value)
		cdata->chsw |= CHSW_DEVELOPER_MODE_ENABLED;
	if (!wpsw->value)
		cdata->chsw |= CHSW_WRITE_PROTECT_DISABLED;

	strncpy(cdata->frid, frid, ID_LEN);
	strncpy(cdata->hwid, gbb_data + gbbh->hwid_offset, gbbh->hwid_size);

	/* boot reason; always 0 */
	cdata->binf[0] = 0;
	/* ec firmware type: 0=read-only, 1=rewritable; default to 0 */
	cdata->binf[2] = 0;
	/* recovery reason is default to VBNV_RECOVERY_NOT_REQUESTED */
	cdata->binf[4] = VBNV_RECOVERY_NOT_REQUESTED;

	cdata->gpio_port_write_protect_sw = wpsw->port;
	cdata->gpio_port_recovery_sw = recsw->port;
	cdata->gpio_port_developer_sw = devsw->port;

	cdata->polarity_write_protect_sw = wpsw->polarity;
	cdata->polarity_recovery_sw = recsw->polarity;
	cdata->polarity_developer_sw = devsw->polarity;

	cdata->write_protect_sw = wpsw->value;
	cdata->recovery_sw = recsw->value;
	cdata->developer_sw = devsw->value;

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
	char path[] = "/firmware/chromeos";
	int nodeoffset, err;
	int gpio_phandle;
	int gpio_prop[3];

	err = fdt_open_into(fdt, fdt,
			fdt_totalsize(fdt) + sizeof(*cdata) + 4096);
	if (err < 0) {
		VBDEBUG(PREFIX "fail to resize fdt: %s\n", fdt_strerror(err));
		return 1;
	}
	*size_ptr = fdt_totalsize(fdt);

	/* TODO: Upstream device tree is moving from tegra250 to
	 * tegra20. Keep the check for 250 around for now but it can be
	 * removed once the changes have trickled down.
	 */
	nodeoffset = fdt_node_offset_by_compatible(fdt, 0,
						   "nvidia,tegra20-gpio");
	if (nodeoffset <= 0)
		nodeoffset = fdt_node_offset_by_compatible(fdt, 0,
						   "nvidia,tegra250-gpio");

	gpio_phandle = fdt_get_phandle(fdt, nodeoffset);
	if (gpio_phandle <= 0) {
		gpio_phandle = fdt_alloc_phandle(fdt);
		fdt_setprop_cell(fdt, nodeoffset,
				 "linux,phandle", gpio_phandle);
	}
	gpio_prop[0] = cpu_to_fdt32(gpio_phandle);

	nodeoffset = fdt_add_subnodes_from_path(fdt, 0, path);
	if (nodeoffset < 0) {
		VBDEBUG(PREFIX "fail to create subnode %s: %s\n", path,
				fdt_strerror(nodeoffset));
		return 1;
	}

	fdt_setprop_string(fdt, nodeoffset, "compatible", "chromeos-firmware");

#define set_scalar_prop(name, f) \
	fdt_setprop_cell(fdt, nodeoffset, name, cdata->f)
#define set_array_prop(name, f) \
	fdt_setprop(fdt, nodeoffset, name, cdata->f, sizeof(cdata->f))
#define set_string_prop(name, f) \
	fdt_setprop_string(fdt, nodeoffset, name, cdata->f)
#define set_conststring_prop(name, str) \
	fdt_setprop_string(fdt, nodeoffset, name, str)
#define set_bool_prop(name, f) \
	((cdata->f) ? fdt_setprop(fdt, nodeoffset, name, NULL, 0) : 0)

	err = 0;
	err |= set_scalar_prop("total-size", total_size);
	err |= set_string_prop("signature", signature);
	err |= set_scalar_prop("version", version);
	err |= set_scalar_prop("nonvolatile-context-lba", nvcxt_lba);
	err |= set_scalar_prop("nonvolatile-context-offset", vbnv[0]);
	err |= set_scalar_prop("nonvolatile-context-size", vbnv[1]);
	err |= set_array_prop("boot-nonvolatile-cache", nvcxt_cache);

	err |= set_bool_prop("boot-write-protect-switch", write_protect_sw);
	err |= set_bool_prop("boot-recovery-switch", recovery_sw);
	err |= set_bool_prop("boot-developer-switch", developer_sw);

	gpio_prop[1] = cpu_to_fdt32(cdata->gpio_port_write_protect_sw);
	gpio_prop[2] = cpu_to_fdt32(cdata->polarity_write_protect_sw);
	err |= fdt_setprop(fdt, nodeoffset, "write-protect-switch",
			   gpio_prop, sizeof(gpio_prop));

	gpio_prop[1] = cpu_to_fdt32(cdata->gpio_port_recovery_sw);
	gpio_prop[2] = cpu_to_fdt32(cdata->polarity_recovery_sw);
	err |= fdt_setprop(fdt, nodeoffset, "recovery-switch",
			   gpio_prop, sizeof(gpio_prop));

	gpio_prop[1] = cpu_to_fdt32(cdata->gpio_port_developer_sw);
	gpio_prop[2] = cpu_to_fdt32(cdata->polarity_developer_sw);
	err |= fdt_setprop(fdt, nodeoffset, "developer-switch",
			   gpio_prop, sizeof(gpio_prop));

	err |= set_scalar_prop("boot-reason", binf[0]);

	switch (cdata->binf[2]) {
	case 0:
		err |= set_conststring_prop("active-ec-firmware", "RO");
		break;
	case 1:
		err |= set_conststring_prop("active-ec-firmware", "RW");
		break;
	}

	switch (cdata->binf[3]) {
	case RECOVERY_TYPE:
		err |= set_conststring_prop("firmware-type", "recovery");
		break;
	case NORMAL_TYPE:
		err |= set_conststring_prop("firmware-type", "normal");
		break;
	case DEVELOPER_TYPE:
		err |= set_conststring_prop("firmware-type", "developer");
		break;
	}
	err |= set_scalar_prop("recovery-reason", binf[4]);

	err |= set_string_prop("hardware-id", hwid);
	err |= set_string_prop("firmware-version", fwid);
	err |= set_string_prop("readonly-firmware-version", frid);
	err |= set_scalar_prop("fmap-offset", fmap_base);
	err |= set_array_prop("vboot-shared-data", vbshared_data);

#undef set_scalar_prop
#undef set_array_prop
#undef set_string_prop
#undef set_bool_prop

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
