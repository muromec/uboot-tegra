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

/*
 * CBFS commands
 */
#include <common.h>
#include <command.h>
#include <cbfs.h>

int do_cbfs_init(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	uintptr_t end_of_rom = 0xffffffff;
	char *ep;

	if (argc > 2) {
		printf("usage: cbfsls [end of rom]>\n");
		return 0;
	}
	if (argc == 2) {
		end_of_rom = (int)simple_strtoul(argv[1], &ep, 16);
		if (*ep) {
			puts("\n** Invalid end of ROM **\n");
			return 1;
		}
	}
	return file_cbfs_init(end_of_rom);
}

U_BOOT_CMD(
	cbfsinit,	2,	0,	do_cbfs_init,
	"initialize the cbfs driver",
	"[end of rom]\n"
	"    - Initialize the cbfs driver. The optional 'end of rom'\n"
	"      parameter specifies where the end of the ROM is that the\n"
	"      CBFS is in. It defaults to 0xFFFFFFFF\n"
);

int do_cbfs_fsload (cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	long size;
	unsigned long offset;
	unsigned long count;
	char buf[12];

	if (argc < 3) {
		printf("usage: cbfsload <addr> <filename> [bytes]\n");
		return 1;
	}

	/* parse offset and count */
	offset = simple_strtoul(argv[1], NULL, 16);
	if (argc == 4)
		count = simple_strtoul(argv[3], NULL, 16);
	else
		count = 0;

	size = file_cbfs_read(argv[2], (void *)offset, count);
	if (size < 0) {
		printf("\n** Unable to read \"%s\" from cbfs **\n", argv[2]);
		return 1;
	}

	printf("\n%ld bytes read\n", size);

	sprintf(buf, "%lX", size);
	setenv("filesize", buf);

	return 0;
}

U_BOOT_CMD(
	cbfsload,	4,	0,	do_cbfs_fsload,
	"load binary file from a cbfs filesystem",
	"<addr> <filename> [bytes]\n"
	"    - load binary file 'filename' from the cbfs to address 'addr'\n"
);

int do_cbfs_ls (cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	return file_cbfs_ls();
}

U_BOOT_CMD(
	cbfsls,	1,	1,	do_cbfs_ls,
	"list files",
	"    - list the files in the cbfs\n"
);

int do_cbfs_fsinfo (cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	return file_cbfs_fsinfo();
}

U_BOOT_CMD(
	cbfsinfo,	1,	1,	do_cbfs_fsinfo,
	"print information about filesystem",
	"    - print information about the cbfs filesystem\n"
);
