# SPDX-License-Identifier: GPL-2.0-or-later
#
#Makefile for the dvb dummy frontend drivers
#

MODULE_NAME_DUMMY_FE = amlogic-dummy-fe
obj-$(CONFIG_AMLOGIC_DVB_DUMMY_FRONTEND) = $(MODULE_NAME_DUMMY_FE).o

ccflags-y += -I$(srctree)/include
ccflags-y += -I$(srctree)/include/media
ccflags-y += -I$(srctree)/include/uapi
ccflags-y += -I$(srctree)/$(COMMON_DRIVERS_DIR)/include/linux/amlogic
ccflags-y += -I$(srctree)/$(COMMON_DRIVERS_DIR)/include/uapi
ccflags-y += -I$(srctree)/$(COMMON_DRIVERS_DIR)/drivers/dvb

$(MODULE_NAME_DUMMY_FE)-y += dvb_dummy_fe.o
