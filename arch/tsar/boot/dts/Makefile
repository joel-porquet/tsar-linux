dtb-$(CONFIG_ARCH_TSARV4_MONO_SOCLIB)	+= tsarv4_mono_soclib.dtb.o

dtb-$(CONFIG_ARCH_TSARV5_MONO_SOCLIB)	+= tsarv5_mono_soclib.dtb.o
dtb-$(CONFIG_ARCH_TSARV5_MULTI_SOCLIB)	+= tsarv5_multi_soclib.dtb.o

dtb-$(CONFIG_ARCH_TSARV5_MONO_FPGA)	+= tsarv5_mono_fpga.dtb.o
dtb-$(CONFIG_ARCH_TSARV5_MULTI_FPGA)	+= tsarv5_multi_fpga.dtb.o

dtb-$(CONFIG_ARCH_TSARV5_LETI_SOCLIB)	+= tsarv5_leti.dtb.o
dtb-$(CONFIG_ARCH_TSARV5_LETI_FPGA)	+= tsarv5_leti.dtb.o

## if required, include the Device Tree Blob into Linux image
## else the DTB should be given by the bootloader
obj-$(CONFIG_TSAR_BUILTIN_DTB) += $(dtb-y)
