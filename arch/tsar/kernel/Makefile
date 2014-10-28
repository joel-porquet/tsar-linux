extra-y := head.o vmlinux.lds

obj-y += entry.o irq.o process.o ptrace.o reboot.o setup.o signal.o sys_call_table.o switch.o time.o tls.o topology.o traps.o

obj-$(CONFIG_EARLY_PRINTK) += early_printk.o

obj-$(CONFIG_KGDB) += kgdb.o

obj-$(CONFIG_SMP) += smp.o
obj-$(CONFIG_NUMA) += numa.o

