extra-y := head.o vmlinux.lds

obj-y += entry.o irq.o process.o ptrace.o reboot.o setup.o signal.o sys_call_table.o switch.o time.o tls.o topology.o traps.o

obj-$(CONFIG_EARLY_PRINTK) += early_printk.o

obj-$(CONFIG_KGDB) += kgdb.o

obj-$(CONFIG_SMP) += smp.o
obj-$(CONFIG_NUMA) += numa.o

obj-$(CONFIG_SOCLIB_VCI_ICU) += vci_icu.o
obj-$(CONFIG_SOCLIB_VCI_XICU) += vci_xicu.o vci_xicu_timer.o
obj-$(CONFIG_SOCLIB_VCI_TIMER) += vci_timer.o
obj-$(CONFIG_SOCLIB_VCI_IOC) += vci_ioc.o
obj-$(CONFIG_SOCLIB_VCI_MTTY) += vci_mtty.o

