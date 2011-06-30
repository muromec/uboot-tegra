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

uint32_t VbExIsShutdownRequested(void)
{
	/*
	 * TODO(waihong@chromium.org) Detect the power button pressed or
	 * the lib closed.
	 */
	return 0;
}
