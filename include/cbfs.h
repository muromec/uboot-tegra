/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __CBFS_H
#define __CBFS_H

/*
 * Initialize the CBFS driver and load metadata into RAM.
 *
 * @param end_of_rom	Points to the end of the ROM the CBFS should be read
 *                      from.
 *
 * @return Zero on success, non-zero on failure
 */
int file_cbfs_init(uintptr_t end_of_rom);

/*
 * Read a file from CBFS into RAM
 *
 * @param filename	The name of the file to read.
 * @param buffer	Where to read it into memory.
 *
 * @return If positive or zero, the number of characters read. If negative, an
 *         error occurred.
 */
long file_cbfs_read(const char *filename, void *buffer, unsigned long maxsize);

/*
 * List the files names, types, and sizes in the current CBFS.
 *
 * @return Zero on success, non-zero on failure.
 */
int file_cbfs_ls(void);

/*
 * Print information from the CBFS header.
 *
 * @return Zero on success, non-zero on failure.
 */
int file_cbfs_fsinfo(void);

#endif /* __CBFS_H */
