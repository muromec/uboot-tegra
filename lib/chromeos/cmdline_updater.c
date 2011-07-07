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
#include <chromeos/common.h>
#include <chromeos/cmdline_updater.h>

#define PREFIX "cmdline_updater: "

/* assert(0 <= val && val < 99); sprintf(dst, "%u", val); */
static char *itoa(char *dst, int val)
{
	if (val > 9)
		*dst++ = '0' + val / 10;
	*dst++ = '0' + val % 10;
	return dst;
}

/* copied from x86 bootstub code; sprintf(dst, "%02x", val) */
static void one_byte(char *dst, uint8_t val)
{
	dst[0] = "0123456789abcdef"[(val >> 4) & 0x0F];
	dst[1] = "0123456789abcdef"[val & 0x0F];
}

/* copied from x86 bootstub code; display a GUID in canonical form */
static char *emit_guid(char *dst, uint8_t *guid)
{
	one_byte(dst, guid[3]); dst += 2;
	one_byte(dst, guid[2]); dst += 2;
	one_byte(dst, guid[1]); dst += 2;
	one_byte(dst, guid[0]); dst += 2;
	*dst++ = '-';
	one_byte(dst, guid[5]); dst += 2;
	one_byte(dst, guid[4]); dst += 2;
	*dst++ = '-';
	one_byte(dst, guid[7]); dst += 2;
	one_byte(dst, guid[6]); dst += 2;
	*dst++ = '-';
	one_byte(dst, guid[8]); dst += 2;
	one_byte(dst, guid[9]); dst += 2;
	*dst++ = '-';
	one_byte(dst, guid[10]); dst += 2;
	one_byte(dst, guid[11]); dst += 2;
	one_byte(dst, guid[12]); dst += 2;
	one_byte(dst, guid[13]); dst += 2;
	one_byte(dst, guid[14]); dst += 2;
	one_byte(dst, guid[15]); dst += 2;
	return dst;
}

/* This replaces %D, %P, and %U in kernel command line */
void update_cmdline(char *src, int devnum, int partnum, uint8_t *guid,
		char *dst)
{
	int c;

	// sanity check on inputs
	if (devnum < 0 || devnum > 25 || partnum < 1 || partnum > 99) {
		VBDEBUG(PREFIX "insane input: %d, %d\n", devnum, partnum);
		devnum = 0;
		partnum = 3;
	}

	while ((c = *src++)) {
		if (c != '%') {
			*dst++ = c;
			continue;
		}

		switch ((c = *src++)) {
		case '\0':
			/* input ends in '%'; is it not well-formed? */
			src--;
			break;
		case 'D':
			/*
			 * TODO: Do we have any better way to know whether %D
			 * is replaced by a letter or digits? So far, this is
			 * done by a rule of thumb that if %D is followed by a
			 * 'p' character, then it is replaced by digits.
			 */
			if (*src == 'p')
				dst = itoa(dst, devnum);
			else
				*dst++ = 'a' + devnum;
			break;
		case 'P':
			dst = itoa(dst, devnum);
			break;
		case 'U':
			dst = emit_guid(dst, guid);
			break;
		default:
			*dst++ = '%';
			*dst++ = c;
			break;
		}
	}

	*dst = '\0';
}
