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
#include <mmc.h>
#include <chromeos/common.h>
#include <chromeos/firmware_storage.h>
#include <chromeos/os_storage.h>

#define PREFIX "firmware_storage_onestop: "

enum {
	SECTION_RO = 0,
	SECTION_RW_A,
	SECTION_RW_B
};

struct context {
	struct fdt_twostop_fmap *fmap;
	int section;
	firmware_storage_t spi_file;
	struct mmc *mmc;
};

static int within_entry(const struct fdt_fmap_entry *e, uint32_t offset)
{
	return e->offset <= offset && offset < e->offset + e->length;
}

static int get_section(const struct fdt_twostop_fmap *fmap, off_t offset)
{
	if (within_entry(&fmap->readonly.readonly, offset))
		return SECTION_RO;
	else if (within_entry(&fmap->readwrite_a.readwrite_a, offset))
		return SECTION_RW_A;
	else if (within_entry(&fmap->readwrite_b.readwrite_b, offset))
		return SECTION_RW_B;
	else
		return -1;
}

static void set_start_block_and_offset(const struct fdt_twostop_fmap *fmap,
		int section, uint32_t offset,
		uint32_t *start_block_ptr, uint32_t *offset_in_block_ptr)
{
	*start_block_ptr = (section == SECTION_RW_A) ?
		fmap->readwrite_a.block_lba :
		fmap->readwrite_b.block_lba;

	*offset_in_block_ptr = offset - ((section == SECTION_RW_A) ?
			fmap->readwrite_a.readwrite_a.offset :
			fmap->readwrite_b.readwrite_b.offset);

	*start_block_ptr += (*offset_in_block_ptr >> 9);
	*offset_in_block_ptr &= 0x1ff;
}

static int read_mmc(struct mmc *mmc,
		uint32_t start_block, uint32_t offset_in_block,
		uint32_t offset, uint32_t count, void *buf);
static int write_mmc(struct mmc *mmc,
		uint32_t start_block, uint32_t offset_in_block,
		uint32_t offset, uint32_t count, void *buf);

static int read_onestop(struct firmware_storage_t *file,
		uint32_t offset, uint32_t count, void *buf)
{
	struct context *cxt = file->context;
	int section = get_section(cxt->fmap, offset);
	uint32_t start_block = 0, offset_in_block = 0;

	VBDEBUG(PREFIX "read(offset=%08x, count=%08x, buffer=%p)\n",
			offset, count, buf);

	if (section < 0)
		return -1;
	else if (section == SECTION_RO)
		return cxt->spi_file.read(&cxt->spi_file, offset, count, buf);
	else {
		set_start_block_and_offset(cxt->fmap, section, offset,
				&start_block, &offset_in_block);
		return read_mmc(cxt->mmc, start_block, offset_in_block,
				offset, count, buf);
	}
}

static int write_onestop(struct firmware_storage_t *file,
		uint32_t offset, uint32_t count, void *buf)
{
	struct context *cxt = file->context;
	int section = get_section(cxt->fmap, offset);
	uint32_t start_block = 0, offset_in_block = 0;

	VBDEBUG(PREFIX "write(offset=%08x, count=%08x, buffer=%p)\n",
			offset, count, buf);

	if (section < 0)
		return -1;
	else if (section == SECTION_RO)
		return cxt->spi_file.write(&cxt->spi_file, offset, count, buf);
	else {
		set_start_block_and_offset(cxt->fmap, section, offset,
				&start_block, &offset_in_block);
		return write_mmc(cxt->mmc, start_block, offset_in_block,
				offset, count, buf);
	}
}

static int read_mmc(struct mmc *mmc,
		uint32_t start_block, uint32_t offset_in_block,
		uint32_t offset, uint32_t count, void *buf)
{
	uint32_t n_block;
	size_t n_byte;
	uint8_t *residual;

	VBDEBUG(PREFIX "read_mmc(start_block=%08x, offset_in_block=%08x)\n",
			start_block, offset_in_block);

	residual = memalign(CACHE_LINE_SIZE, 512);
	n_byte = 0;

	if (offset_in_block) {
		n_byte = MIN(512 - offset_in_block, count);
		mmc->block_dev.block_read(MMC_INTERNAL_DEVICE,
				start_block, 1, residual);
		memcpy(buf, residual + offset_in_block, n_byte);
	}

	offset_in_block += n_byte;
	if (offset_in_block == 512) {
		start_block++;
		offset_in_block = 0;
	}

	while (count > n_byte && (n_block = (count - n_byte) >> 9)) {
		n_block = mmc->block_dev.block_read(MMC_INTERNAL_DEVICE,
				start_block, n_block, buf + n_byte);
		start_block += n_block;
		n_byte += (n_block << 9);
	}

	if (count > n_byte) {
		mmc->block_dev.block_read(MMC_INTERNAL_DEVICE,
				start_block, 1, residual);
		memcpy(buf + n_byte, residual, count - n_byte);
		offset_in_block = count - n_byte;
	}

	free(residual);
	return 0;
}

static int write_mmc(struct mmc *mmc,
		uint32_t start_block, uint32_t offset_in_block,
		uint32_t offset, uint32_t count, void *buf)
{
	uint32_t n_block;
	size_t n_byte;
	uint8_t *residual;

	VBDEBUG(PREFIX "write_mmc(start_block=%08x, offset_in_block=%08x)\n",
			start_block, offset_in_block);

	residual = memalign(CACHE_LINE_SIZE, 512);
	n_byte = 0;

	if (offset_in_block) {
		n_byte = MIN(512 - offset_in_block, count);
		mmc->block_dev.block_read(MMC_INTERNAL_DEVICE,
				start_block, 1, residual);
		memcpy(residual + offset_in_block, buf, n_byte);
		mmc->block_dev.block_write(MMC_INTERNAL_DEVICE,
				start_block, 1, residual);
	}

	offset_in_block += n_byte;
	if (offset_in_block == 512) {
		start_block++;
		offset_in_block = 0;
	}

	while (count > n_byte && (n_block = (count - n_byte) >> 9)) {
		n_block = mmc->block_dev.block_write(MMC_INTERNAL_DEVICE,
				start_block, n_block, buf + n_byte);
		start_block += n_block;
		n_byte += (n_block << 9);
	}

	if (count > n_byte) {
		mmc->block_dev.block_read(MMC_INTERNAL_DEVICE,
				start_block, 1, residual);
		memcpy(residual, buf + n_byte, count - n_byte);
		mmc->block_dev.block_write(MMC_INTERNAL_DEVICE,
				start_block, 1, residual);
		offset_in_block = count - n_byte;
	}

	free(residual);
	return 0;
}

static int close_onestop(firmware_storage_t *file)
{
	struct context *cxt = file->context;
	int ret;

        ret = cxt->spi_file.close(cxt->spi_file.context);
	free(cxt);

	return ret;
}

int firmware_storage_open_onestop(firmware_storage_t *file,
		struct fdt_twostop_fmap *fmap)
{
	struct context *cxt;

	cxt = malloc(sizeof(*cxt));
	if (!cxt)
		return -1;

	cxt->fmap = fmap;
	cxt->section = SECTION_RO;

	if (firmware_storage_open_spi(&cxt->spi_file)) {
		free(cxt);
		return -1;
	}

	cxt->mmc = find_mmc_device(MMC_INTERNAL_DEVICE);
	if (!cxt->mmc) {
		free(cxt);
		return -1;
	}
	mmc_init(cxt->mmc);

	file->read = read_onestop;
	file->write = write_onestop;
	file->close = close_onestop;
	file->context = (void *)cxt;

	return 0;
}
