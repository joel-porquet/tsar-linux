/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <asm/io.h>

#define DEVICE_NAME "vci_ioc"

/*
 * Driver for the SoCLib VCI IOC (vci_block_device*)
 */

struct vci_ioc_data {
	spinlock_t lock;
	struct device *dev;
	void __iomem *virt_base;
	unsigned int irq;
	unsigned int index;
	struct gendisk *gd;
	struct request_queue *queue;
	struct request *req;
};

struct vci_ioc_driver {
	int major;
	unsigned int num;
	struct vci_ioc_data **vci_iocs;
};

static struct vci_ioc_driver *vci_ioc_driver;

/* vci_ioc register map */
#define VCI_IOC_BUFFER		0	/* R/W: physical address of src/dst buffer */
#define VCI_IOC_LBA		4	/* R/W: first block of transfer */
#define VCI_IOC_COUNT		8	/* R/W: number of blocks to transfer */
#define VCI_IOC_OP		12	/* W: type of operation (NOOP/READ/WRITE) */
#define VCI_IOC_STATUS		16	/* R: Status of the transfer */
#define VCI_IOC_IRQ_ENABLE	20	/* R/W: Enable the hw IRQ line */
#define VCI_IOC_SIZE		24	/* R: Number of addressable blocks */
#define VCI_IOC_BLOCK_SIZE	28	/* R: Size of blocks */
#define VCI_IOC_BUFFER_EXT	32	/* R/W: 40-bit address extension */

/* vci_ioc OP values */
#define VCI_IOC_OP_NOOP		0
#define VCI_IOC_OP_READ		1
#define VCI_IOC_OP_WRITE	2

/* vci_ioc STATUS values */
#define VCI_IOC_STATUS_IDLE		0
#define VCI_IOC_STATUS_BUSY		1
#define VCI_IOC_STATUS_READ_SUCCESS	2
#define VCI_IOC_STATUS_WRITE_SUCCESS	3
#define VCI_IOC_STATUS_READ_ERROR	4
#define VCI_IOC_STATUS_WRITE_ERROR	5
#define VCI_IOC_STATUS_ERROR		6

/*
 * Utils
 */

static void vci_ioc_submit_request(struct request *req)
{
	struct vci_ioc_data *ioc = req->rq_disk->private_data;

	unsigned long buffer;
	unsigned long op;
	unsigned long start_sector, sectors;

	buffer = virt_to_phys(req->buffer);
	op = (rq_data_dir(req)) ? VCI_IOC_OP_WRITE : VCI_IOC_OP_READ;
	start_sector = blk_rq_pos(req);
	sectors = blk_rq_sectors(req);

	dev_dbg(ioc->dev, "vci_ioc_submit_request: \
			op=%ld, start_sector=%ld, sectors=%ld\n",
			op, start_sector, sectors);

	writel(buffer, ioc->virt_base + VCI_IOC_BUFFER);
	writel(start_sector, ioc->virt_base + VCI_IOC_LBA);
	writel(sectors, ioc->virt_base + VCI_IOC_COUNT);
	/* the following operation launches the transfer */
	writel(op, ioc->virt_base + VCI_IOC_OP);

	ioc->req = req;
}

static void vci_ioc_do_request(struct request_queue *q)
{
	struct request *req;

	while ((req = blk_fetch_request(q))) {
		if (req->cmd_type == REQ_TYPE_FS) {
			vci_ioc_submit_request(req);
		} else {
			/* not supported */
			blk_dump_rq_flags(req, DEVICE_NAME " bad request");
			__blk_end_request_all(req, -EIO);
		}
	}
}

static void vci_ioc_request(struct request_queue *q)
{
	struct vci_ioc_data *ioc = q->queuedata;

	if (ioc->req) {
		dev_dbg(ioc->dev, "vci_ioc_request busy\n");
		return;
	}

	vci_ioc_do_request(q);
}

static irqreturn_t vci_ioc_interrupt(int irq, void *data)
{
	struct vci_ioc_data *ioc = data;
	struct request *req = ioc->req;
	unsigned long status;
	int error = 0;

	/* reading the STATUS resets the IRQ */
	status = readl(ioc->virt_base + VCI_IOC_STATUS);
	dev_dbg(ioc->dev, "vci_ioc_interrupt: status=%ld\n", status);
	switch (status) {
		case VCI_IOC_STATUS_READ_SUCCESS:
		case VCI_IOC_STATUS_WRITE_SUCCESS:
			dev_dbg(ioc->dev, "vci_ioc_interrupt: success\n");
			break;
		case VCI_IOC_STATUS_READ_ERROR:
		case VCI_IOC_STATUS_WRITE_ERROR:
		case VCI_IOC_STATUS_ERROR:
			dev_dbg(ioc->dev, "vci_ioc_interrupt: failure\n");
			error = -EIO;
			break;
		default:
			dev_dbg(ioc->dev, "vci_ioc_interrupt: spurious INT\n");
			error = -EIO;
			break;
	}

	spin_lock(&ioc->lock);
	/* end the current req */
	__blk_end_request_all(req, error);
	ioc->req = NULL;
	/* look for a new one to launch */
	vci_ioc_do_request(ioc->queue);
	spin_unlock(&ioc->lock);

	return IRQ_HANDLED;
}

/*
 * Block device operations
 */

static const struct block_device_operations vci_ioc_fops = {
	.owner		= THIS_MODULE,
};

/*
 * Platform driver
 */

static int vci_ioc_pf_probe(struct platform_device *pdev)
{
	struct vci_ioc_data **vci_iocs;
	struct vci_ioc_data *ioc;
	unsigned int index;
	struct resource *res;
	int ret = -EINVAL;

	unsigned long sector_size, nb_sectors;

	/* get our private structure */
	vci_iocs = vci_ioc_driver->vci_iocs;

	/* look for a free index */
	for (index = 0; index < vci_ioc_driver->num; index++)
		if (vci_iocs[index] == NULL)
			break;
	if (index >= vci_ioc_driver->num)
		return -EINVAL;

	/* allocate a new vci_ioc_data instance */
	ioc = devm_kzalloc(&pdev->dev, sizeof(struct vci_ioc_data),
			GFP_KERNEL);
	if (!ioc) {
		dev_err(&pdev->dev, "failed to allocate memory for %s node\n",
				pdev->name);
		return -ENOMEM;
	}
	vci_iocs[index] = ioc;
	ioc->index = index;

	/* get the virq of the irq that links the vci_ioc to the parent icu (eg
	 * vci_xicu).
	 * Note that the virq has already been computed beforehand, with
	 * respect to the irq_domain it belongs to. */
	ioc->irq = platform_get_irq(pdev, 0);
	if (ioc->irq < 0) {
		dev_err(&pdev->dev, "failed to get virq for %s node\n",
				pdev->name);
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ioc->virt_base = devm_request_and_ioremap(&pdev->dev, res);
	if (!ioc->virt_base) {
		dev_err(&pdev->dev, "failed to ioremap_resource for %s node\n",
				pdev->name);
		return -EADDRNOTAVAIL;
	}

	/* let's only support 512-byte sectors */
	sector_size = readl(ioc->virt_base + VCI_IOC_BLOCK_SIZE);
	if (sector_size != 512) {
		dev_err(&pdev->dev, "please use 512-byte sectors for %s node\n",
				pdev->name);
		return -EINVAL;
	}
	/* number of accessible blocks */
	nb_sectors = readl(ioc->virt_base + VCI_IOC_SIZE);

	/* complete the initialization of our private data structure */
	ioc->dev = &pdev->dev;
	spin_lock_init(&ioc->lock);
	ioc->queue = blk_init_queue(vci_ioc_request, &ioc->lock);
	ioc->queue->queuedata = ioc;

	/* support up to 16 partitions (minors) */
	ioc->gd = alloc_disk(16);

	ioc->gd->major = vci_ioc_driver->major;
	ioc->gd->first_minor = index * 16;
	ioc->gd->fops = &vci_ioc_fops;
	ioc->gd->queue = ioc->queue;
	ioc->gd->private_data = ioc;
	sprintf(ioc->gd->disk_name, DEVICE_NAME "%u", index);
	/* configure disk capacity */
	set_capacity(ioc->gd, nb_sectors);

	/* setup irq */
	ret = devm_request_irq(&pdev->dev, ioc->irq, vci_ioc_interrupt, 0,
			DEVICE_NAME, ioc);
	if (ret) {
		dev_err(&pdev->dev, "could not request irq %u for %s node\n",
				ioc->irq, pdev->name);
		goto error_request_irq;
	}

	/* enable irq */
	writel(1, ioc->virt_base + VCI_IOC_IRQ_ENABLE);

	add_disk(ioc->gd);

	platform_set_drvdata(pdev, ioc);

	return 0;

error_request_irq:
	del_gendisk(ioc->gd);
	put_disk(ioc->gd);
	blk_cleanup_queue(ioc->queue);
	return ret;
}

static int vci_ioc_pf_remove(struct platform_device *pdev)
{
	struct vci_ioc_data **vci_iocs = vci_ioc_driver->vci_iocs;
	struct vci_ioc_data *ioc = platform_get_drvdata(pdev);

	del_gendisk(ioc->gd);
	put_disk(ioc->gd);
	blk_cleanup_queue(ioc->queue);

	vci_iocs[ioc->index] = NULL;

	return 0;
}

#define VCI_IOC_OF_COMPATIBLE "tsar,vci_block_device"

static const struct of_device_id vci_ioc_pf_of_ids[] = {
	{ .compatible = VCI_IOC_OF_COMPATIBLE },
	{}
};
MODULE_DEVICE_TABLE(of, vci_ioc_pf_of_ids);

static struct platform_driver vci_ioc_pf_driver = {
	.driver = {
		.owner		= THIS_MODULE,
		.name		= DEVICE_NAME,
		.of_match_table = vci_ioc_pf_of_ids,
	},
	.probe	= vci_ioc_pf_probe,
	.remove	= vci_ioc_pf_remove,
};

/*
 * Module initialization and termination
 */

static int __init vci_ioc_init(void)
{
	struct device_node *np;
	unsigned int count = 0;
	int ret;

	pr_debug("Registering SoCLib VCI IOC driver\n");

	/* count the number of iocs in the system */
	for_each_compatible_node(np, NULL, VCI_IOC_OF_COMPATIBLE)
		count++;

	if (!count)
		return -ENODEV;

	/* create vci_ioc_driver structure */
	vci_ioc_driver = kzalloc(sizeof(*vci_ioc_driver), GFP_KERNEL);
	if (!vci_ioc_driver) {
		pr_err("vci_ioc: could not allocate vci_ioc_driver\n");
		return -ENOMEM;
	}

	vci_ioc_driver->num = count;

	vci_ioc_driver->vci_iocs = kzalloc(sizeof(struct vci_ioc_data *) *
			count, GFP_KERNEL);
	if (!vci_ioc_driver->vci_iocs) {
		pr_err("vci_ioc: could not allocate private data structure\n");
		ret = -ENOMEM;
		goto error_ioc_driver;
	}

	vci_ioc_driver->major = register_blkdev(0, DEVICE_NAME);
	if (vci_ioc_driver->major <= 0) {
		pr_err("vci_ioc: unable to get major number\n");
		ret = vci_ioc_driver->major;
		goto error_alloc_ioc;
	}

	pr_info("SoCLib VCI IOC driver, major=%i\n", vci_ioc_driver->major);

	ret = platform_driver_register(&vci_ioc_pf_driver);
	if (ret) {
		pr_err("vci_ioc: could not register platform driver\n");
		goto error_register_blk;
	}

	return 0;

error_register_blk:
	unregister_blkdev(vci_ioc_driver->major, DEVICE_NAME);
error_alloc_ioc:
	kfree(vci_ioc_driver->vci_iocs);
error_ioc_driver:
	kfree(vci_ioc_driver);
	return ret;
}

static void __exit vci_ioc_exit(void)
{
	pr_debug("Unregistering SoCLib VCI IOC driver\n");

	platform_driver_unregister(&vci_ioc_pf_driver);
	unregister_blkdev(vci_ioc_driver->major, DEVICE_NAME);
	kfree(vci_ioc_driver->vci_iocs);
	kfree(vci_ioc_driver);
}

module_init(vci_ioc_init);
module_exit(vci_ioc_exit);

/* MODULE information */
MODULE_AUTHOR("Joël Porquet <joel.porquet@lip6.fr>");
MODULE_DESCRIPTION("SoCLib VCI IOC driver");
MODULE_LICENSE("GPL");
