#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive for
# more details.
#

KBUILD_DEFCONFIG := tsar_monocpu_soclib_defconfig
# are we cross-compiling?
ifneq ($(SUBARCH), $(ARCH))
	ifeq ($(CROSS_COMPILE),)
		CROSS_COMPILE := $(call cc-cross-prefix, mipsel-unknown-elf- mipsel-linux- mipsel-linux-gnu- mipsel-unknown-linux-gnu-)
	endif
endif

# TODO: we should be able to compile with Werror
#cflags-y += -Werror
cflags-y += -mno-check-zero-division
cflags-y += -mabi=32
cflags-y += -G0 -mno-abicalls -fno-pic -pipe
cflags-y += -ffreestanding
cflags-y += -EL -D__MIPSEL__
cflags-y += -march=mips32
# assembler options
cflags-y += -Wa,-mips32 -Wa,--trap
# we want soft-float because otherwise gcc uses fpu registers as temporary
# holders for gp regs (!?)
cflags-y += -msoft-float
# temporary flag (for debug)
#cflags-y += -DDEBUG

KBUILD_AFLAGS += $(cflags-y)
KBUILD_CFLAGS += $(cflags-y)
KBUILD_AFLAGS_MODULE += -mlong-calls
KBUILD_CFLAGS_MODULE += -mlong-calls

LDFLAGS_vmlinux += -G0 -static -n -nostdlib

ifdef CONFIG_TSAR
CHECKFLAGS += $(shell $(CC) $(KBUILD_CFLAGS) -dM -E -x c /dev/null | \
	      egrep -vw '__GNUC_(|MINOR_|PATCHLEVEL_)_' | \
	      sed -e "s/^\#define /-D'/" -e "s/ /'='/" -e "s/$$/'/")
endif
OBJCOPYFLAGS += --remove-section=.reginfo

head-y := arch/tsar/kernel/head.o
libs-y += arch/tsar/lib/
core-y += arch/tsar/kernel/ arch/tsar/mm/

core-y += arch/tsar/boot/dts/

#drivers-$(CONFIG_MATH_EMULATION) += arch/tsar/math-emu/

boot := arch/tsar/boot

vmlinux.bin: vmlinux
	$(Q)$(MAKE) $(build)=$(boot) $(boot)/$@

archclean:
	$(Q)$(MAKE) $(clean)=$(boot)

define archhelp
	echo '  vmlinux.bin          - Raw binary boot image'
	echo
	echo '  These will be default as appropriate for a configured platform.'
endef
