#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Alternatively, this software may be distributed under the terms of the
# GNU General Public License ("GPL") version 2 as published by the Free
# Software Foundation.
#

include $(TOPDIR)/config.mk

LIB	= $(obj)libchromeos.a

COBJS-$(CONFIG_CHROMEOS) += cmdline_updater.o
COBJS-$(CONFIG_CHROMEOS) += crossystem_data.o
COBJS-$(CONFIG_CHROMEOS) += fdt_decode.o
COBJS-$(CONFIG_CHROMEOS) += firmware_storage_spi.o
COBJS-$(CONFIG_CHROMEOS) += preboot_fdt_update.o
COBJS-$(CONFIG_CHROMEOS) += gbb_bmpblk.o

# TODO(sjg): This MMC code is not needed as yet, and needs slight changes
# to build now
#COBJS-$(CONFIG_CHROMEOS_TWOSTOP) += firmware_storage_twostop.o

COBJS	:= $(COBJS-y)
OBJS	:= $(addprefix $(obj),$(COBJS))

$(LIB):	$(obj).depend $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

#########################################################################

# defines $(obj).depend target
include $(SRCTREE)/rules.mk

sinclude $(obj).depend

#########################################################################
