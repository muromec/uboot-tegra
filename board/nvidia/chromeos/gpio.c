/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

/* Implementation of per-board GPIO accessor functions */

#include <common.h>
#include <asm/arch/gpio.h>
#include <asm/arch/tegra2.h>
#include <chromeos/common.h>
#include <chromeos/gpio.h>

static int read_gpio(enum polarity polarity, int gpio)
{
	int pol = (polarity == GPIO_ACTIVE_HIGH) ? 0 : 1;
	gpio_direction_input(gpio);
	return pol ^ gpio_get_value(gpio);
}

int is_firmware_write_protect_gpio_asserted(enum polarity polarity)
{
	return read_gpio(polarity, GPIO_PH3);
}

int is_recovery_mode_gpio_asserted(enum polarity polarity)
{
	return read_gpio(polarity, GPIO_PH0);
}

int is_developer_mode_gpio_asserted(enum polarity polarity)
{
	return read_gpio(polarity, GPIO_PV0);
}
