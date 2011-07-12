/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

/* Implementation of firmware storage access interface for SPI */

#include <common.h>
#include <malloc.h>
#include <spi_flash.h>
#include <chromeos/common.h>
#include <chromeos/firmware_storage.h>

#define PREFIX "firmware_storage_spi: "

#ifndef CONFIG_SF_DEFAULT_SPEED
# define CONFIG_SF_DEFAULT_SPEED	1000000
#endif
#ifndef CONFIG_SF_DEFAULT_MODE
# define CONFIG_SF_DEFAULT_MODE		SPI_MODE_3
#endif

/*
 * Check the right-exclusive range [offset:offset+*count_ptr), and adjust
 * value pointed by <count_ptr> to form a valid range when needed.
 *
 * Return 0 if it is possible to form a valid range. and non-zero if not.
 */
static int border_check(struct spi_flash *flash, uint32_t offset,
		uint32_t count)
{
	if (offset >= flash->size) {
		VBDEBUG(PREFIX "at EOF\n");
		return -1;
	}

	if (offset + count > flash->size) {
		VBDEBUG(PREFIX "exceed range\n");
		return -1;
	}

	return 0;
}

static int read_spi(firmware_storage_t *file, uint32_t offset, uint32_t count,
		void *buf)
{
	struct spi_flash *flash = file->context;

	if (border_check(flash, offset, count))
		return -1;

	if (flash->read(flash, offset, count, buf)) {
		VBDEBUG(PREFIX "SPI read fail\n");
		return -1;
	}

	return 0;
}

/*
 * FIXME: It is a reasonable assumption that sector size = 4096 bytes.
 * Nevertheless, comparing to coding this magic number here, there should be a
 * better way (maybe rewrite driver interface?) to expose this parameter from
 * eeprom driver.
 */
#define SECTOR_SIZE 0x1000

/*
 * Align the right-exclusive range [*offset_ptr:*offset_ptr+*length_ptr) with
 * SECTOR_SIZE.
 * After alignment adjustment, both offset and length will be multiple of
 * SECTOR_SIZE, and will be larger than or equal to the original range.
 */
static void align_to_sector(uint32_t *offset_ptr, uint32_t *length_ptr)
{
	VBDEBUG(PREFIX "before adjustment\n");
	VBDEBUG(PREFIX "offset: 0x%x\n", *offset_ptr);
	VBDEBUG(PREFIX "length: 0x%x\n", *length_ptr);

	/* Adjust if offset is not multiple of SECTOR_SIZE */
	if (*offset_ptr & (SECTOR_SIZE - 1ul)) {
		*offset_ptr &= ~(SECTOR_SIZE - 1ul);
	}

	/* Adjust if length is not multiple of SECTOR_SIZE */
	if (*length_ptr & (SECTOR_SIZE - 1ul)) {
		*length_ptr &= ~(SECTOR_SIZE - 1ul);
		*length_ptr += SECTOR_SIZE;
	}

	VBDEBUG(PREFIX "after adjustment\n");
	VBDEBUG(PREFIX "offset: 0x%x\n", *offset_ptr);
	VBDEBUG(PREFIX "length: 0x%x\n", *length_ptr);
}

static int write_spi(firmware_storage_t *file, uint32_t offset, uint32_t count,
		void *buf)
{
	struct spi_flash *flash = file->context;
	uint8_t static_buf[SECTOR_SIZE];
	uint8_t *backup_buf;
	uint32_t k, n;
	int status, ret = -1;

	/* We will erase <n> bytes starting from <k> */
	k = offset;
	n = count;
	align_to_sector(&k, &n);

	VBDEBUG(PREFIX "offset:          0x%08x\n", offset);
	VBDEBUG(PREFIX "adjusted offset: 0x%08x\n", k);
	if (k > offset) {
		VBDEBUG(PREFIX "align incorrect: %08x > %08x\n", k, offset);
		return -1;
	}

	if (border_check(flash, k, n))
		return -1;

	backup_buf = n > sizeof(static_buf) ? malloc(n) : static_buf;

	if ((status = flash->read(flash, k, n, backup_buf))) {
		VBDEBUG(PREFIX "cannot backup data: %d\n", status);
		goto EXIT;
	}

	if ((status = flash->erase(flash, k, n))) {
		VBDEBUG(PREFIX "SPI erase fail: %d\n", status);
		goto EXIT;
	}

	/* combine data we want to write and backup data */
	memcpy(backup_buf + (offset - k), buf, count);

	if (flash->write(flash, k, n, backup_buf)) {
		VBDEBUG(PREFIX "SPI write fail\n");
		goto EXIT;
	}

	ret = 0;

EXIT:
	if (backup_buf != static_buf)
		free(backup_buf);

	return ret;
}

static int close_spi(firmware_storage_t *file)
{
	struct spi_flash *flash = file->context;
	spi_flash_free(flash);
	return 0;
}

int firmware_storage_open_spi(firmware_storage_t *file)
{
	const unsigned int bus = 0;
	const unsigned int cs = 0;
	const unsigned int max_hz = CONFIG_SF_DEFAULT_SPEED;
	const unsigned int spi_mode = CONFIG_SF_DEFAULT_MODE;
	struct spi_flash *flash;

	if (!(flash = spi_flash_probe(bus, cs, max_hz, spi_mode))) {
		VBDEBUG(PREFIX "fail to init SPI flash at %u:%u\n", bus, cs);
		return -1;
	}

	file->read = read_spi;
	file->write = write_spi;
	file->close = close_spi;
	file->context = (void *)flash;

	return 0;
}
