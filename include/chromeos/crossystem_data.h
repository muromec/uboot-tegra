/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#ifndef __CHROMEOS_KERNEL_SHARED_DATA_H__
#define __CHROMEOS_KERNEL_SHARED_DATA_H__

#include <vboot_nvstorage.h>
#include <vboot_struct.h>

#define ID_LEN		256

#define RECOVERY_FIRMWARE	0
#define REWRITABLE_FIRMWARE_A	1
#define REWRITABLE_FIRMWARE_B	2

#define RECOVERY_TYPE		0
#define NORMAL_TYPE		1
#define DEVELOPER_TYPE		2

/* the data blob format */
typedef struct {
	uint32_t	total_size;
	char		signature[10];
	uint16_t	version;
	uint64_t	nvcxt_lba;
	uint16_t	vbnv[2];
	uint8_t		nvcxt_cache[VBNV_BLOCK_SIZE];
	uint8_t		write_protect_sw;
	uint8_t		recovery_sw;
	uint8_t		developer_sw;
	uint8_t		polarity_write_protect_sw;
	uint8_t		polarity_recovery_sw;
	uint8_t		polarity_developer_sw;
	uint8_t		binf[5];
	uint32_t	chsw;
	char 		hwid[ID_LEN];
	char		fwid[ID_LEN];
	char		frid[ID_LEN];
	uint32_t	fmap_base;
	uint8_t		vbshared_data[VB_SHARED_DATA_REC_SIZE];
} crossystem_data_t;

/**
 * This initializes the data blob that we will pass to kernel, and later be
 * used by crossystem. Note that:
 * - It does not initialize information of the main firmware, e.g., fwid. This
 *   information must be initialized in subsequent calls to the setters below.
 * - The recovery reason is default to VBNV_RECOVERY_NOT_REQUESTED.
 *
 * @param cdata is the data blob shared with crossystem
 * @param frid r/o firmware id; a zero-terminated string shorter than ID_LEN
 * @param fmap_data is the address of fmap in firmware
 * @param gbb_data points to gbb blob
 * @param nvcxt_raw points to the VbNvContext raw data
 * @param write_protect_sw stores the value of write protect gpio
 * @param recovery_sw stores the value of recovery mode gpio
 * @param developer_sw stores the value of developer mode gpio
 * @param polarity_write_protect_sw is the polarity of the gpio pin
 * @param polarity_recovery_sw is the polarity of the gpio pin
 * @param polarity_developer_sw is the polarity of the gpio pin
 * @return 0 if it succeeds; non-zero if it fails
 */
int crossystem_data_init(crossystem_data_t *cdata, char *frid,
		uint32_t fmap_data, void *gbb_data, void *nvcxt_raw,
		int write_protect_sw, int recovery_sw, int developer_sw,
		int polarity_write_protect_sw, int polarity_recovery_sw,
		int polarity_developer_sw);

/**
 * This sets rewritable firmware id. It should only be called in non-recovery
 * boots.
 *
 * @param cdata is the data blob shared with crossystem
 * @param fwid r/w firmware id; a zero-terminated string shorter than ID_LEN
 * @return 0 if it succeeds; non-zero if it fails
 */
int crossystem_data_set_fwid(crossystem_data_t *cdata, char *fwid);

/**
 * This sets active main firmware and its type.
 *
 * @param cdata is the data blob shared with crossystem
 * @param which - recovery: 0; rewritable A: 1; rewritable B: 2
 * @param type - recovery: 0; normal: 1; developer: 2
 * @return 0 if it succeeds; non-zero if it fails
 */
int crossystem_data_set_active_main_firmware(crossystem_data_t *cdata,
		int which, int type);

/**
 * This sets recovery reason.
 *
 * @param cdata is the data blob shared with crossystem
 * @param reason - the recovery reason
 * @return 0 if it succeeds; non-zero if it fails
 */
int crossystem_data_set_recovery_reason(crossystem_data_t *cdata,
		uint32_t reason);

/**
 * This embeds kernel shared data into fdt.
 *
 * @param cdata is the data blob shared with crossystem
 * @param fdt points to a device tree
 * @param size_ptr stores the new size of fdt
 * @return 0 if it succeeds, non-zero if it fails
 */
int crossystem_data_embed_into_fdt(crossystem_data_t *cdata, void *fdt,
		uint32_t *size_ptr);

/**
 * This prints out the data blob content to debug output.
 */
void crossystem_data_dump(crossystem_data_t *cdata);

#endif /* __CHROMEOS_KERNEL_SHARED_DATA_H__ */
