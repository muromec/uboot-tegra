/*
 * coreboot Framebuffer driver.
 *
 * Copyright (C) 2011 The Chromium OS authors
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

#include <common.h>
#include <asm/ic/coreboot/tables.h>
#include <asm/ic/coreboot/sysinfo.h>
#include <video_fb.h>
#include "videomodes.h"

/*
 * The Graphic Device
 */
GraphicDevice ctfb;

static int parse_coreboot_table_fb(GraphicDevice *gd)
{
	struct cb_framebuffer *fb = lib_sysinfo.framebuffer;

	/* If there is no framebuffer structure, bail out and keep
	 * running on the serial console.
	 */
	if (!fb)
		return 0;

	gd->winSizeX = fb->x_resolution;
	gd->winSizeY = fb->y_resolution;

	gd->plnSizeX = fb->x_resolution;
	gd->plnSizeY = fb->y_resolution;

	gd->gdfBytesPP = fb->bits_per_pixel / 8;

	switch (fb->bits_per_pixel) {
	case 24:
		gd->gdfIndex = GDF_32BIT_X888RGB;
		break;
	case 16:
		gd->gdfIndex = GDF_16BIT_565RGB;
		break;
	default:
		gd->gdfIndex = GDF__8BIT_INDEX;
		break;
	}

	gd->isaBase = CONFIG_SYS_ISA_IO_BASE_ADDRESS;
	gd->pciBase = (unsigned int)fb->physical_address;

	gd->frameAdrs = (unsigned int)fb->physical_address;
	gd->memSize = fb->bytes_per_line * fb->y_resolution;

	gd->vprBase = (unsigned int)fb->physical_address;
	gd->cprBase = (unsigned int)fb->physical_address;

	return 1;
}

void *video_hw_init(void)
{
	GraphicDevice *gd = &ctfb;
	int bits_per_pixel;

	printf("Video: ");

	if(!parse_coreboot_table_fb(gd)) {
		printf("No video mode configured in coreboot!\n");
		return NULL;
	}

	bits_per_pixel = gd->gdfBytesPP * 8;

	/* fill in Graphic device struct */
	sprintf(gd->modeIdent, "%dx%dx%d", gd->winSizeX, gd->winSizeY,
		 bits_per_pixel);
	printf("%s\n", gd->modeIdent);

	memset((void *)gd->pciBase, 0,
		gd->winSizeX * gd->winSizeY * gd->gdfBytesPP);

	return (void *)gd;
}

