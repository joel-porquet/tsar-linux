/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/console.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>

#include <asm/io.h>


/*
 * Driver for the SoCLib VCI TTY (vci_tty)
 */

struct vci_tty_data {
	spinlock_t lock;
	struct device *dev;
	struct tty_port port;
	void __iomem *virt_base;
	unsigned int rx_irq;
	unsigned int id;
	struct console console;
};

static DEFINE_MUTEX(vci_tty_mutex);
static struct tty_driver *vci_tty_driver;
static unsigned int vci_tty_line_count = 1;
static struct vci_tty_data *vci_ttys;

/* vci_tty register map */
#define VCI_TTY_WRITE	0x0 // WO: character to print
#define VCI_TTY_STATUS	0x4 // RO: != 0 if pending character, 0 otherwise
#define VCI_TTY_READ	0x8 // RO: character to read (resets STATUS)

/*
 * Utils
 */

static char vci_tty_do_ischar(struct vci_tty_data *tty)
{
	return __raw_readl(tty->virt_base + VCI_TTY_STATUS) ? 1 : 0;
}

static char vci_tty_do_getchar(struct vci_tty_data *tty)
{
	return __raw_readb(tty->virt_base + VCI_TTY_READ);
}

static void vci_tty_do_putchar(struct vci_tty_data *tty, const char c)
{
	__raw_writeb(c, tty->virt_base + VCI_TTY_WRITE);
}

static void vci_tty_do_write(unsigned int line, const char *buf, unsigned count)
{
	unsigned long irq_flags;
	struct vci_tty_data *tty = &vci_ttys[line];

	spin_lock_irqsave(&tty->lock, irq_flags);

	while (count-- && *buf) {
#ifdef CONFIG_TSAR_FPGA
		if (*buf == '\n')
			vci_tty_do_putchar(tty, '\r');
#endif
		vci_tty_do_putchar(tty, *buf);
		buf++;
	}

	spin_unlock_irqrestore(&tty->lock, irq_flags);
}

/*
 * Console driver
 */

static void vci_tty_console_write(struct console *co, const char *buf, unsigned int count)
{
	vci_tty_do_write(co->index, buf, count);
}

static struct tty_driver *vci_tty_console_device(struct console *co, int *index)
{
	*index = co->index;
	return vci_tty_driver;
}

static int vci_tty_console_setup(struct console *co, char *options)
{
	if((unsigned)co->index >= vci_tty_line_count)
		return -ENODEV;
	if(vci_ttys[co->index].virt_base == 0)
		return -ENODEV;
	return 0;
}

/*
 * TTY driver
 */

/* tty ISR */
static irqreturn_t vci_tty_interrupt(int irq, void *dev_id)
{
	unsigned long irq_flags;
	struct vci_tty_data *tty = dev_id;
	unsigned char c;

	if (!vci_tty_do_ischar(tty))
		return IRQ_NONE;

	spin_lock_irqsave(&tty->lock, irq_flags);
	c = vci_tty_do_getchar(tty);
	tty_insert_flip_char(&tty->port, c, TTY_NORMAL);
	tty_flip_buffer_push(&tty->port);
	spin_unlock_irqrestore(&tty->lock, irq_flags);

	return IRQ_HANDLED;
}

/* tty operations */
static int vci_tty_install(struct tty_driver *driver, struct tty_struct *tty_st)
{
	struct vci_tty_data *tty = &vci_ttys[tty_st->index];

	tty_st->driver_data = tty;

	return tty_port_install(&tty->port, driver, tty_st);
}

static int vci_tty_open(struct tty_struct *tty_st, struct file *filp)
{
	struct vci_tty_data *tty = tty_st->driver_data;

	if (!tty->dev)
		return -ENODEV;

	return tty_port_open(&tty->port, tty_st, filp);
}

static void vci_tty_close(struct tty_struct *tty_st, struct file *filp)
{
	struct vci_tty_data *tty = tty_st->driver_data;

	if (tty->dev)
		tty_port_close(&tty->port, tty_st, filp);
}

static void vci_tty_hangup(struct tty_struct *tty_st)
{
	struct vci_tty_data *tty = tty_st->driver_data;
	tty_port_hangup(&tty->port);
}

static int vci_tty_write(struct tty_struct *tty_st, const unsigned char *buf, int count)
{
	vci_tty_do_write(tty_st->index, buf, count);
	return count;
}

static int vci_tty_write_room(struct tty_struct *tty_st)
{
	return 0x10000;
}

static int vci_tty_chars_in_buffer(struct tty_struct *tty_st)
{
	struct vci_tty_data *tty = tty_st->driver_data;
	return vci_tty_do_ischar(tty);
}

#ifdef CONFIG_CONSOLE_POLL
static int vci_tty_poll_init(struct tty_driver *driver, int line, char *options)
{
	/* nothing to do */
	return 0;
}

static int vci_tty_poll_get_char(struct tty_driver *driver, int line)
{
	struct tty_port *port = driver->ports[line];
	struct vci_tty_data *tty = container_of(port, struct vci_tty_data, port);

	if (!vci_tty_do_ischar(tty))
		return NO_POLL_CHAR;

	return vci_tty_do_getchar(tty);
}

static void vci_tty_poll_put_char(struct tty_driver *driver, int line, char ch)
{
	struct tty_port *port = driver->ports[line];
	struct vci_tty_data *tty = container_of(port, struct vci_tty_data, port);

	vci_tty_do_putchar(tty, ch);
}
#endif

static const struct tty_operations vci_tty_ops = {
	.install = vci_tty_install,
	.open = vci_tty_open,
	.close = vci_tty_close,
	.hangup = vci_tty_hangup,
	.write = vci_tty_write,
	.write_room = vci_tty_write_room,
	.chars_in_buffer = vci_tty_chars_in_buffer,
#ifdef CONFIG_CONSOLE_POLL
	.poll_init = vci_tty_poll_init,
	.poll_get_char = vci_tty_poll_get_char,
	.poll_put_char = vci_tty_poll_put_char,
#endif
};

/* tty port operations */
static int vci_tty_port_activate(struct tty_port *port, struct tty_struct *tty_st)
{
	struct vci_tty_data *tty = container_of(port, struct vci_tty_data, port);
	int ret;

	tty_st->driver_data = tty;

	ret = request_irq(tty->rx_irq, vci_tty_interrupt, 0, "vci_tty", tty);
	if (ret) {
		dev_err(tty->dev, "could not request rx irq %u (ret=%i)\n",
				tty->rx_irq, ret);
		return ret;
	}

	return 0;
}

static void vci_tty_port_shutdown(struct tty_port *port)
{
	struct vci_tty_data *tty = container_of(port, struct vci_tty_data, port);

	free_irq(tty->rx_irq, tty);
}

static const struct tty_port_operations vci_tty_port_ops = {
	.activate = vci_tty_port_activate,
	.shutdown = vci_tty_port_shutdown,
};

/*
 * Platform driver
 */

static int vci_tty_pf_probe(struct platform_device *pdev)
{
	struct vci_tty_data *tty;

	void __iomem *virt_base;
	int rx_irq;

	int id = pdev->id;

	struct resource *res;

	/* if id is -1, scan for a free id and use that one */
	if (id == -1) {
		for (id = 0; id < vci_tty_line_count; id++)
			if (vci_ttys[id].virt_base == 0)
				break;
	}

	if (id < 0 || id >= vci_tty_line_count)
		return -EINVAL;

	/* get virq of the rx_irq that links the vci_tty to the parent icu (ie
	 * vci_icu) */
	rx_irq = platform_get_irq(pdev, 0);
	if (rx_irq < 0)
		panic("%s: failed to get IRQ\n", pdev->name);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		panic("%s: failed to get memory range\n", pdev->name);

	if (!request_mem_region(res->start, resource_size(res), res->name))
		panic("%s: failed to request memory\n", pdev->name);

	virt_base = ioremap_nocache(res->start, resource_size(res));

	if (!virt_base)
		panic("%s: failed to remap memory\n", pdev->name);

	/* initialize our private data structure */
	mutex_lock(&vci_tty_mutex);
	tty = &vci_ttys[id];
	tty->id = id;
	spin_lock_init(&tty->lock);
	tty_port_init(&tty->port);
	tty->port.ops = &vci_tty_port_ops;
	tty->rx_irq = rx_irq;
	tty->virt_base = virt_base;

	tty->dev = tty_port_register_device(&tty->port, vci_tty_driver,
			id, &pdev->dev);

	if (IS_ERR(tty->dev))
		panic("Could not register vci_tty (ret=%ld)\n", PTR_ERR(tty->dev));

	platform_set_drvdata(pdev, tty);

	/* configure console */
	strcpy(tty->console.name, "ttyVTTY");
	tty->console.write = vci_tty_console_write;
	tty->console.device = vci_tty_console_device;
	tty->console.setup = vci_tty_console_setup;
	tty->console.flags = CON_PRINTBUFFER;
	tty->console.index = id;
	register_console(&tty->console);

	mutex_unlock(&vci_tty_mutex);
	return 0;

	/* TODO: should do a better error handling */
}

static int vci_tty_pf_remove(struct platform_device *pdev)
{
	struct vci_tty_data *tty = platform_get_drvdata(pdev);

	mutex_lock(&vci_tty_mutex);

	unregister_console(&tty->console);
	tty_unregister_device(vci_tty_driver, tty->id);
	tty_port_destroy(&tty->port);
	iounmap(tty->virt_base);
	tty->virt_base = 0;
	/* XXX: irq_dispose_mapping? */
	free_irq(tty->rx_irq, pdev);

	mutex_unlock(&vci_tty_mutex);
	return 0;
}

static const struct of_device_id vci_tty_pf_of_ids[] = {
	{ .compatible = "soclib,vci_multi_tty" },
	{}
};
MODULE_DEVICE_TABLE(of, vci_tty_pf_of_ids);

static struct platform_driver vci_tty_pf_driver = {
	.driver = {
		.owner 		= THIS_MODULE,
		.name		= "vci_tty",
		.of_match_table = vci_tty_pf_of_ids,
	},
	.probe	= vci_tty_pf_probe,
	.remove = vci_tty_pf_remove,
};

/*
 * Module initialization and termination
 */

static int __init vci_tty_init(void)
{
	int ret;

	pr_debug("Registering SoCLib VCI TTY driver\n");

	/* create vci_tty private data structure */
	vci_ttys = kzalloc(sizeof(struct vci_tty_data) * vci_tty_line_count, GFP_KERNEL);
	if (!vci_ttys)
		return -ENOMEM;

	/* create tty_driver structure */
	vci_tty_driver = alloc_tty_driver(vci_tty_line_count);
	if (!vci_tty_driver) {
		ret = -ENOMEM;
		goto error;
	}

	/* initialize tty_driver structure */
	vci_tty_driver->driver_name = "vci_tty";
	vci_tty_driver->name = "ttyVTTY";
	vci_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	vci_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	vci_tty_driver->init_termios = tty_std_termios;
	vci_tty_driver->init_termios.c_oflag &= ~ONLCR;
	vci_tty_driver->flags = TTY_DRIVER_RESET_TERMIOS
		| TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	tty_set_operations(vci_tty_driver, &vci_tty_ops);

	/* register tty_driver structure */
	ret = tty_register_driver(vci_tty_driver);
	if (ret) {
		pr_err("vci_tty: could not register tty driver (ret=%i)\n", ret);
		goto error;
	}

	/* register platform driver */
	ret = platform_driver_register(&vci_tty_pf_driver);
	if (ret) {
		pr_err("vci_tty: could not register platform driver (ret=%i)\n", ret);
		goto error;
	}

	return 0;
error:
	if (vci_tty_driver) {
		tty_unregister_driver(vci_tty_driver);
		put_tty_driver(vci_tty_driver);
	}
	kfree(vci_ttys);
	return ret;
}

static void __exit vci_tty_exit(void)
{
	pr_debug("Unregistering SoCLib VCI TTY driver\n");
	platform_driver_unregister(&vci_tty_pf_driver);
	tty_unregister_driver(vci_tty_driver);
	put_tty_driver(vci_tty_driver);
	kfree(vci_ttys);
}

module_init(vci_tty_init);
module_exit(vci_tty_exit);

/* MODULE information */
MODULE_AUTHOR("Joël Porquet <joel.porquet@lip6.fr>");
MODULE_DESCRIPTION("SoCLib VCI TTY driver");
MODULE_LICENSE("GPL");
