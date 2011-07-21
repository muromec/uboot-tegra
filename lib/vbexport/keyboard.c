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

/* Import the header files from vboot_reference. */
#include <vboot_api.h>

/* Control Sequence Introducer for arrow keys */
#define CSI_0		0x1B	/* Escape */
#define CSI_1		0x5B	/* '[' */

uint32_t VbExKeyboardRead(void)
{
	int c = 0;

	/* No input available. */
	if (!tstc())
		goto out;

	/* Read a non-Escape character or a standalone Escape character. */
	if ((c = getc()) != CSI_0 || !tstc())
		goto out;

	/* Filter out non- Escape-[ sequence. */
	if (getc() != CSI_1) {
		c = 0;
		goto out;
	}

	/* Special values for arrow up/down/left/right. */
	switch (getc()) {
	case 'A':
		c = VB_KEY_UP;
		break;
	case 'B':
		c = VB_KEY_DOWN;
		break;
	case 'C':
		c = VB_KEY_RIGHT;
		break;
	case 'D':
		c = VB_KEY_LEFT;
		break;
	default:
		/* Filter out speical keys that we do not recognize. */
		c = 0;
		break;
	}

out:
	return c;
}
