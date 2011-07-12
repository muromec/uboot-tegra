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
	lbaint_t start_block;
	off_t offset_in_block;
};

static int within_entry(const struct fdt_fmap_entry *e, uint32_t offset)
{
	return e->offset <= offset && offset < e->offset + e->length;
}

static int get_section(struct context *cxt, off_t offset)
{
	const struct fdt_twostop_fmap *fmap = cxt->fmap;;

	if (within_entry(&fmap->readonly.readonly, offset))
		return SECTION_RO;
	else if (within_entry(&fmap->readwrite_a.readwrite_a, offset))
		return SECTION_RW_A;
	else if (within_entry(&fmap->readwrite_b.readwrite_b, offset))
		return SECTION_RW_B;
	else
		return -1;
}

static void set_block_lba(struct context *cxt, off_t offset)
{
	const struct fdt_twostop_fmap *fmap = cxt->fmap;;
	const off_t min_offset = MIN(fmap->readwrite_a.rw_a_onestop.offset,
			fmap->readwrite_b.rw_b_onestop.offset);
	const off_t max_offset = MAX(fmap->readwrite_a.rw_a_onestop.offset,
			fmap->readwrite_b.rw_b_onestop.offset);

	cxt->start_block = CHROMEOS_RW_FIRMWARE_START_LBA;
	if (offset >= max_offset)
		cxt->start_block +=
			(fmap->onestop_layout.onestop_layout.length >> 9);

	if (offset >= max_offset)
		cxt->offset_in_block = offset - max_offset;
	else
		cxt->offset_in_block = offset - min_offset;

	cxt->start_block += (cxt->offset_in_block >> 9);
	cxt->offset_in_block &= 0x1ff;
}

static off_t seek_onestop(void *context, off_t offset, enum whence_t whence)
{
	struct context *cxt = context;
	int section;

	VBDEBUG(PREFIX "seek(offset=%08x, whence=%d)\n", (int)offset, whence);

	/* TODO support other SEEK_* operation */
	if (whence != SEEK_SET) {
		VBDEBUG(PREFIX "only support SEEK_SET for now\n");
		return -1;
	}

	section = get_section(cxt, offset);
	if (section < 0) {
		VBDEBUG(PREFIX "offset is not in any section\n");
		return -1;
	}

	cxt->section = section;
	if (cxt->section == SECTION_RO)
		return cxt->spi_file.seek(cxt->spi_file.context, offset, whence);

	set_block_lba(cxt, offset);

	VBDEBUG(PREFIX "seek: section = %d, block = %08llx, offset = %08x\n",
			cxt->section,
			(uint64_t)cxt->start_block,
			(int)cxt->offset_in_block);

	return cxt->offset_in_block;
}

static ssize_t read_onestop(void *context, void *buf, size_t count)
{
	struct context *cxt = context;
	uint32_t n_block;
	size_t n_byte_read;
	uint8_t *residual;

	VBDEBUG(PREFIX "read(count=%08x, buffer=%p)\n", (int)count, buf);

	if (cxt->section == SECTION_RO)
		return cxt->spi_file.read(cxt->spi_file.context, buf, count);

	residual = memalign(CACHE_LINE_SIZE, 512);
	n_byte_read = 0;

	if (cxt->offset_in_block) {
		n_byte_read = MIN(512 - cxt->offset_in_block, count);
		cxt->mmc->block_dev.block_read(MMC_INTERNAL_DEVICE,
				cxt->start_block, 1, residual);
		memcpy(buf, residual + cxt->offset_in_block, n_byte_read);
	}

	cxt->offset_in_block += n_byte_read;
	if (cxt->offset_in_block == 512) {
		cxt->start_block++;
		cxt->offset_in_block = 0;
	}

	while (count > n_byte_read && (n_block = (count - n_byte_read) >> 9)) {
		n_block = cxt->mmc->block_dev.block_read(MMC_INTERNAL_DEVICE,
				cxt->start_block, n_block, buf + n_byte_read);
		cxt->start_block += n_block;
		n_byte_read += (n_block << 9);
	}

	if (count > n_byte_read) {
		cxt->mmc->block_dev.block_read(MMC_INTERNAL_DEVICE,
				cxt->start_block, 1, residual);
		memcpy(buf + n_byte_read, residual, count - n_byte_read);
		cxt->offset_in_block = count - n_byte_read;
	}

	free(residual);
	return count;
}

static ssize_t write_onestop(void *context, const void *buf, size_t count)
{
	struct context *cxt = context;
	uint32_t n_block;
	size_t n_byte_write;
	uint8_t *residual;

	VBDEBUG(PREFIX "write(count=%08x, buffer=%p)\n", (int)count, buf);

	if (cxt->section == SECTION_RO)
		return cxt->spi_file.write(cxt->spi_file.context, buf, count);

	residual = memalign(CACHE_LINE_SIZE, 512);
	n_byte_write = 0;

	if (cxt->offset_in_block) {
		n_byte_write = MIN(512 - cxt->offset_in_block, count);
		cxt->mmc->block_dev.block_read(MMC_INTERNAL_DEVICE,
				cxt->start_block, 1, residual);
		memcpy(residual + cxt->offset_in_block, buf, n_byte_write);
		cxt->mmc->block_dev.block_read(MMC_INTERNAL_DEVICE,
				cxt->start_block, 1, residual);
	}

	cxt->offset_in_block += n_byte_write;
	if (cxt->offset_in_block == 512) {
		cxt->start_block++;
		cxt->offset_in_block = 0;
	}

	while (count > n_byte_write && (n_block = (count - n_byte_write) >> 9)) {
		n_block = cxt->mmc->block_dev.block_write(MMC_INTERNAL_DEVICE,
				cxt->start_block, n_block, buf + n_byte_write);
		cxt->start_block += n_block;
		n_byte_write += (n_block << 9);
	}

	if (count > n_byte_write) {
		cxt->mmc->block_dev.block_read(MMC_INTERNAL_DEVICE,
				cxt->start_block, 1, residual);
		memcpy(residual, buf + n_byte_write, count - n_byte_write);
		cxt->mmc->block_dev.block_write(MMC_INTERNAL_DEVICE,
				cxt->start_block, 1, residual);
		cxt->offset_in_block = count - n_byte_write;
	}

	free(residual);
	return count;
}

static int close_onestop(void *context)
{
	struct context *cxt = context;
	int ret;

        ret = cxt->spi_file.close(cxt->spi_file.context);
	free(cxt);

	return ret;
}

static int lock_onestop(void *context)
{
	/* TODO Implement lock device */
	return 0;
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

	cxt->start_block = 0;
	cxt->offset_in_block = 0;

	file->seek = seek_onestop;
	file->read = read_onestop;
	file->write = write_onestop;
	file->close = close_onestop;
	file->lock_device = lock_onestop;
	file->context = (void*) cxt;

	return 0;
}
