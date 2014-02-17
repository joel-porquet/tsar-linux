#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive for
# more details.
#

# from arch/mips/boot/makefile: drop some uninteresting sections in the kernel image
remove-section-names := .reginfo .mdebug .comment .note .pdr .options .MIPS.options
remove-section-flags := $(addprefix --remove-section=,$(remove-section-names))

targets := vmlinux.bin

quiet_cmd_bin = OBJCOPY $@
      cmd_bin = $(OBJCOPY) -O binary $(remove-section-flags) vmlinux $@

$(obj)/vmlinux.bin: vmlinux FORCE
	$(call if_changed,objcopy)
	@echo 'Kernel: $@ is read' ' (#'`cat .version`')'

