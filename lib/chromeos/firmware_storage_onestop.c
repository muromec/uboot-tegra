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
#include <chromeos/firmware_storage.h>

#define PREFIX "firmware_storage_onestop: "

/* TODO support read/write mmc */

struct context {
	firmware_storage_t spi_file;
	struct fdt_onestop_fmap *fmap;
};

static off_t seek_onestop(void *context, off_t offset, enum whence_t whence)
{
	struct context *cxt = context;

	/* TODO support other SEEK_* operation */
	if (whence != SEEK_SET) {
		VBDEBUG(PREFIX "only support SEEK_SET for now\n");
		return -1;
	}

	/* TODO remap these offsets to mmc lba */
	if (cxt->fmap->readwrite_a.offset <= offset &&
			offset < cxt->fmap->readwrite_b.offset)
		offset = offset - cxt->fmap->readwrite_a.offset + 0x00100000;
	else if (cxt->fmap->readwrite_b.offset <= offset)
		offset = offset - cxt->fmap->readwrite_b.offset + 0x00180000;

	return cxt->spi_file.seek(cxt->spi_file.context, offset, whence);
}

static ssize_t read_onestop(void *context, void *buf, size_t count)
{
	struct context *cxt = context;
	return cxt->spi_file.read(cxt->spi_file.context, buf, count);
}

static ssize_t write_onestop(void *context, const void *buf, size_t count)
{
	struct context *cxt = context;
	return cxt->spi_file.write(cxt->spi_file.context, buf, count);
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
		struct fdt_onestop_fmap *fmap)
{
	struct context *cxt;

	cxt = malloc(sizeof(*cxt));
	cxt->fmap = fmap;
	if (firmware_storage_open_spi(&cxt->spi_file)) {
		free(cxt);
		return -1;
	}

	file->seek = seek_onestop;
	file->read = read_onestop;
	file->write = write_onestop;
	file->close = close_onestop;
	file->lock_device = lock_onestop;
	file->context = (void*) cxt;

	return 0;
}
