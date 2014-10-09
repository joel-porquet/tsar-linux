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
#include <linux/of.h>
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
	unsigned int index;
	struct console console;
};

static struct tty_driver *vci_tty_driver;

/* vci_tty register map */
#define VCI_TTY_WRITE	0x0 // WO: character to print
#define VCI_TTY_STATUS	0x4 // RO: != 0 if pending character, 0 otherwise
#define VCI_TTY_READ	0x8 // RO: character to read (resets STATUS)

/*
 * Utils
 */

static char vci_tty_do_ischar(struct vci_tty_data *tty)
{
	/* with the VHDL model, there are other meaniful bits in
	 * VCI_TTY_STATUS. We should only test the LSB here.
	 * It should transparently work for systemc simulations as well. */
	return readl(tty->virt_base + VCI_TTY_STATUS) & 0x1 ? 1 : 0;
}

static char vci_tty_do_getchar(struct vci_tty_data *tty)
{
	return readb(tty->virt_base + VCI_TTY_READ);
}

static void vci_tty_do_putchar(struct vci_tty_data *tty, const char c)
{
	writeb(c, tty->virt_base + VCI_TTY_WRITE);
}

static void vci_tty_do_write(struct vci_tty_data *tty, unsigned int line, const
		char *buf, unsigned count)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&tty->lock, irq_flags);

	while (count-- && *buf) {
		if (*buf == '\n')
			vci_tty_do_putchar(tty, '\r');
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
	struct vci_tty_data *tty = co->data;

	vci_tty_do_write(tty, co->index, buf, count);
}

static struct tty_driver *vci_tty_console_device(struct console *co, int *index)
{
	*index = co->index;
	return vci_tty_driver;
}

static int vci_tty_console_setup(struct console *co, char *options)
{
	struct vci_tty_data *tty = co->data;

	if((unsigned)co->index >= vci_tty_driver->num)
		return -ENODEV;
	if(tty->virt_base == 0)
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

/* tty port operations */
static int vci_tty_port_activate(struct tty_port *port, struct tty_struct *tty_st)
{
	struct vci_tty_data *tty = tty_st->driver_data;
	int ret;

	ret = devm_request_irq(tty->dev, tty->rx_irq, vci_tty_interrupt, 0,
			"vci_tty", tty);
	if (ret) {
		dev_err(tty->dev, "could not request rx irq %u (ret=%i)\n",
				tty->rx_irq, ret);
		return ret;
	}

	return 0;
}

static void vci_tty_port_shutdown(struct tty_port *port)
{
	struct vci_tty_data *tty = port->tty->driver_data;

	devm_free_irq(tty->dev, tty->rx_irq, tty);
}

static const struct tty_port_operations vci_tty_port_ops = {
	.activate = vci_tty_port_activate,
	.shutdown = vci_tty_port_shutdown,
};

/* tty operations */
static int vci_tty_install(struct tty_driver *driver, struct tty_struct *tty_st)
{
	struct vci_tty_data *tty = ((struct vci_tty_data**)
			driver->driver_state)[tty_st->index];

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
	struct vci_tty_data *tty = tty_st->driver_data;

	vci_tty_do_write(tty, tty_st->index, buf, count);
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
	struct vci_tty_data *tty = (struct vci_tty_data**)
		driver->driver_state[line]

	if (!vci_tty_do_ischar(tty))
		return NO_POLL_CHAR;
	return vci_tty_do_getchar(tty);
}

static void vci_tty_poll_put_char(struct tty_driver *driver, int line, char ch)
{
	struct vci_tty_data *tty = (struct vci_tty_data**)
		driver->driver_state[line]

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

/*
 * Platform driver
 */

static int vci_tty_pf_probe(struct platform_device *pdev)
{
	struct vci_tty_data **vci_ttys;
	struct vci_tty_data *tty;
	unsigned int index;
	struct resource *res;
	int ret = -EINVAL;

	/* get our private structure */
	vci_ttys = vci_tty_driver->driver_state;

	/* look for a free index */
	for (index = 0; index < vci_tty_driver->num; index++)
		if (vci_ttys[index] == NULL)
			break;
	if (index >= vci_tty_driver->num)
		return -EINVAL;

	/* allocate a new vci_tty_data instance */
	tty = devm_kzalloc(&pdev->dev, sizeof(struct vci_tty_data),
			GFP_KERNEL);
	if (!tty) {
		dev_err(&pdev->dev, "failed to allocate memory for %s node\n",
				pdev->name);
		return -ENOMEM;
	}
	vci_ttys[index] = tty;
	tty->index = index;

	/* get the virq of the rx_irq that links the vci_tty to the parent icu
	 * (eg vci_xicu).
	 * Note that the virq has already been computed beforehand, with
	 * respect to the irq_domain it belongs to. */
	tty->rx_irq = platform_get_irq(pdev, 0);
	if (tty->rx_irq < 0) {
		dev_err(&pdev->dev, "failed to get virq for %s node\n",
				pdev->name);
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	tty->virt_base = devm_request_and_ioremap(&pdev->dev, res);
	if (!tty->virt_base) {
		dev_err(&pdev->dev, "failed to ioremap_resource for %s node\n",
				pdev->name);
		return -EADDRNOTAVAIL;
	}

	/* complete the initialization of our private data structure */
	spin_lock_init(&tty->lock);
	tty_port_init(&tty->port);
	tty->port.ops = &vci_tty_port_ops;

	tty->dev = tty_port_register_device(&tty->port, vci_tty_driver,
			index, &pdev->dev);

	if (IS_ERR(tty->dev)) {
		dev_err(&pdev->dev, "could not register vci_tty (ret=%d)\n",
				ret);
		return PTR_ERR(tty->dev);
	}

	platform_set_drvdata(pdev, tty);

	/* configure console */
	strcpy(tty->console.name, "ttyVTTY");
	tty->console.write = vci_tty_console_write;
	tty->console.device = vci_tty_console_device;
	tty->console.setup = vci_tty_console_setup;
	tty->console.flags = CON_PRINTBUFFER;
	tty->console.index = index;
	tty->console.data = tty;
	register_console(&tty->console);

	return 0;
}

static int vci_tty_pf_remove(struct platform_device *pdev)
{
	struct vci_tty_data **vci_ttys = vci_tty_driver->driver_state;
	struct vci_tty_data *tty = platform_get_drvdata(pdev);

	unregister_console(&tty->console);
	tty_unregister_device(vci_tty_driver, tty->index);
	tty_port_destroy(&tty->port);

	vci_ttys[tty->index] = NULL;

	return 0;
}

#define VCI_TTY_OF_COMPATIBLE "soclib,vci_multi_tty"

static const struct of_device_id vci_tty_pf_of_ids[] = {
	{ .compatible = VCI_TTY_OF_COMPATIBLE },
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
	struct device_node *np;
	unsigned int count = 0;
	int ret;
	struct vci_tty_data **vci_ttys;

	pr_debug("Registering SoCLib VCI TTY driver\n");

	/* count the number of tty channels in the system */
	for_each_compatible_node(np, NULL, VCI_TTY_OF_COMPATIBLE)
		count++;

	if (!count)
		return -ENODEV;

	/* create tty_driver structure */
	vci_tty_driver = alloc_tty_driver(count);
	if (!vci_tty_driver) {
		pr_err("vci_tty: could not allocate tty driver\n");
		return -ENOMEM;
	}

	/* create vci_tty private data structure */
	vci_ttys = kzalloc(sizeof(struct vci_tty_data *) * count, GFP_KERNEL);
	if (!vci_ttys) {
		pr_err("vci_tty: could not allocate private data structure\n");
		ret = -ENOMEM;
		goto error_tty_driver;
	}
	vci_tty_driver->driver_state = vci_ttys;

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
		goto error_alloc_tty;
	}

	/* register platform driver
	 * note: device probing will already happen in this call */
	ret = platform_driver_register(&vci_tty_pf_driver);
	if (ret) {
		pr_err("vci_tty: could not register platform driver (ret=%i)\n", ret);
		goto error_tty_register;
	}

	return 0;

error_tty_register:
	tty_unregister_driver(vci_tty_driver);
error_alloc_tty:
	kfree(vci_tty_driver->driver_state);
error_tty_driver:
	put_tty_driver(vci_tty_driver);
	return ret;
}

static void __exit vci_tty_exit(void)
{
	pr_debug("Unregistering SoCLib VCI TTY driver\n");

	platform_driver_unregister(&vci_tty_pf_driver);
	tty_unregister_driver(vci_tty_driver);
	kfree(vci_tty_driver->driver_state);
	put_tty_driver(vci_tty_driver);
}

module_init(vci_tty_init);
module_exit(vci_tty_exit);

/* MODULE information */
MODULE_AUTHOR("Joël Porquet <joel.porquet@lip6.fr>");
MODULE_DESCRIPTION("SoCLib VCI TTY driver");
MODULE_LICENSE("GPL");
