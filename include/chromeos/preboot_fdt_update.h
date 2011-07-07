/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

/*
 * This library does the last chance FDT update before booting to kernel.
 * Currently we modify the FDT by embedding crossystem data. So before
 * calling bootm(), set_crossystem_data() should be called.
 */

#ifndef CHROMEOS_PREBOOT_FDT_UPDATE_H_
#define CHROMEOS_PREBOOT_FDT_UPDATE_H_

#include <chromeos/crossystem_data.h>
#include <linux/types.h>

/* Set the crossystem data pointer. It must be done before booting to kernel */
void set_crossystem_data(crossystem_data_t *cdata);

/* Update FDT. This function is called just before booting to kernel. */
int fit_update_fdt_before_boot(char *fdt, ulong *new_size);

#endif /* CHROMEOS_PREBOOT_FDT_UPDATE_H_ */
