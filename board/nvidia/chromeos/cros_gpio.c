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
#include <fdt_decode.h>
#include <asm/arch/gpio.h>
#include <asm/arch/tegra2.h>
#include <chromeos/common.h>
#include <chromeos/cros_gpio.h>

#define PREFIX "cros_gpio: "

int cros_gpio_fetch(enum cros_gpio_index index, const void *fdt,
		cros_gpio_t *gpio)
{
	const char const *port[3] = {
		"gpio_port_write_protect_switch",
		"gpio_port_recovery_switch",
		"gpio_port_developer_switch"
	};
	const int const default_port[3] = {
		GPIO_PH3,
		GPIO_PH0,
		GPIO_PV0
	};
	const char const *polarity[3] = {
		"polarity_write_protect_switch",
		"polarity_recovery_switch",
		"polarity_developer_switch",
	};
	int p;

	if (index < 0 || index >= CROS_GPIO_MAX_GPIO) {
		VBDEBUG(PREFIX "index out of range: %d\n", index);
		return -1;
	}

	gpio->index = index;

	gpio->port = fdt_decode_get_config_int(fdt,
			port[index], default_port[index]);

	gpio_direction_input(gpio->port);

	gpio->polarity = fdt_decode_get_config_int(fdt,
			polarity[index], CROS_GPIO_ACTIVE_HIGH);

	p = (gpio->polarity == CROS_GPIO_ACTIVE_HIGH) ? 0 : 1;
	gpio->value = p ^ gpio_get_value(gpio->port);

	return 0;
}

int cros_gpio_dump(cros_gpio_t *gpio)
{
	const char const *name[3] = {
		"wpsw", "recsw", "devsw"
	};
	int index = gpio->index;

	if (index < 0 || index >= CROS_GPIO_MAX_GPIO) {
		VBDEBUG(PREFIX "index out of range: %d\n", index);
		return -1;
	}

	VBDEBUG(PREFIX "%-6s: port=%3d, polarity=%d, value=%d\n",
			name[gpio->index],
			gpio->port, gpio->polarity, gpio->value);
	return 0;
}
