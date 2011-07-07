/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#ifndef VBOOT_BOOT_KERNEL_H
#define VBOOT_BOOT_KERNEL_H

#include <chromeos/crossystem_data.h>
#include <vboot_api.h>

/**
 * This boots kernel specified in [kparmas].
 *
 * @param kparams       kparams returned from VbSelectAndLoadKernel()
 * @param cdata         crossystem data pointer
 * @return LOAD_KERNEL_INVALID if it fails to boot; otherwise it never returns
 *         to its caller
 */
int boot_kernel(VbSelectAndLoadKernelParams *kparams, crossystem_data_t *cdata);

#endif /* VBOOT_BOOT_KERNEL_H */
