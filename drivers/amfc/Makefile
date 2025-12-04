# SPDX-License-Identifier: GPL-2.0-only
# Amlogic amfc drivers

MODULE_NAME1 = amlogic-amfc
$(MODULE_NAME1)-$(CONFIG_AMLOGIC_AMFC) = amfc.o
$(MODULE_NAME1)-$(CONFIG_AMLOGIC_F2FS) = amfc_f2fs_hook.o
obj-$(CONFIG_AMLOGIC_AMFC) = $(MODULE_NAME1).o

obj-$(CONFIG_AMLOGIC_EROFS) += aml_erofs/
