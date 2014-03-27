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
	unsigned int id;
	struct gendisk *gd;
	struct request_queue *queue;
	struct request *req;
};

static int vci_ioc_major;

/* vci_ioc register map */
#define VCI_IOC_BUFFER 0
#define VCI_IOC_LBA 4
#define VCI_IOC_COUNT 8
#define VCI_IOC_OP 12
#define VCI_IOC_STATUS 16
#define VCI_IOC_IRQ_ENABLE 20
#define VCI_IOC_SIZE 24
#define VCI_IOC_BLOCK_SIZE 28

/* vci_ioc OP values */
#define VCI_IOC_OP_NOOP 0
#define VCI_IOC_OP_READ 1
#define VCI_IOC_OP_WRITE 2

/* vci_ioc STATUS values */
#define VCI_IOC_STATUS_IDLE 0
#define VCI_IOC_STATUS_BUSY 1
#define VCI_IOC_STATUS_READ_SUCCESS 2
#define VCI_IOC_STATUS_WRITE_SUCCESS 3
#define VCI_IOC_STATUS_READ_ERROR 4
#define VCI_IOC_STATUS_WRITE_ERROR 5
#define VCI_IOC_STATUS_ERROR 6

/*
 * Utils
 */

static void vci_ioc_submit_request(struct request *req)
{
	struct vci_ioc_data *ioc = req->rq_disk->private_data;

	char *buffer;
	unsigned long op;
	unsigned long start_sector, sectors;

	buffer = req->buffer;
	op = (rq_data_dir(req)) ? VCI_IOC_OP_WRITE : VCI_IOC_OP_READ;
	start_sector = blk_rq_pos(req);
	sectors = blk_rq_sectors(req);

	dev_dbg(ioc->dev, "vci_ioc_submit_request: \
			op=%ld, start_sector=%ld, sectors=%ld\n",
			op, start_sector, sectors);

	__raw_writel((u32)buffer, ioc->virt_base + VCI_IOC_BUFFER);
	__raw_writel(start_sector, ioc->virt_base + VCI_IOC_LBA);
	__raw_writel(sectors, ioc->virt_base + VCI_IOC_COUNT);
	/* that launches the transfer */
	__raw_writel(op, ioc->virt_base + VCI_IOC_OP);

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
	status = __raw_readl(ioc->virt_base + VCI_IOC_STATUS);
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
	struct vci_ioc_data *ioc;

	void __iomem *virt_base;
	int irq;

	int id = pdev->id;

	struct resource *res;

	unsigned long size;

	if (id == -1)
		id = 0;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
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
	ioc = kzalloc(sizeof(struct vci_ioc_data), GFP_KERNEL);
	if (!ioc)
		panic("%s: failed to allocate memory\n", pdev->name);

	ioc->dev = &pdev->dev;
	ioc->id = id;
	ioc->irq = irq;
	ioc->virt_base = virt_base;

	spin_lock_init(&ioc->lock);
	ioc->queue = blk_init_queue(vci_ioc_request, &ioc->lock);
	ioc->queue->queuedata = ioc;

	/* check block size */
	if (__raw_readl(ioc->virt_base + VCI_IOC_BLOCK_SIZE) != 512)
		panic("%s: the block size should be 512\n", pdev->name);
	blk_queue_logical_block_size(ioc->queue, 512);

	/* setup total size in blocks */
	size = __raw_readl(ioc->virt_base + VCI_IOC_SIZE);
	blk_queue_max_hw_sectors(ioc->queue, size);

	ioc->gd = alloc_disk(1);

	ioc->gd->major = vci_ioc_major;
	ioc->gd->first_minor = 0;
	ioc->gd->fops = &vci_ioc_fops;
	ioc->gd->queue = ioc->queue;
	ioc->gd->private_data = ioc;
	sprintf(ioc->gd->disk_name, DEVICE_NAME);

	/* setup irq */
	if (request_irq( irq, vci_ioc_interrupt, 0, DEVICE_NAME, ioc))
		panic("%s: failed to request irq\n", pdev->name);

	/* enable irq */
	__raw_writel(1, ioc->virt_base + VCI_IOC_IRQ_ENABLE);

	add_disk(ioc->gd);

	platform_set_drvdata(pdev, ioc);

	return 0;
}

static int vci_ioc_pf_remove(struct platform_device *pdev)
{
	struct vci_ioc_data *ioc = platform_get_drvdata(pdev);

	del_gendisk(ioc->gd);
	put_disk(ioc->gd);
	blk_cleanup_queue(ioc->queue);
	free_irq(ioc->irq, pdev);
	iounmap(ioc->virt_base);
	platform_set_drvdata(pdev, NULL);
	kfree(ioc);
	return 0;
}

static const struct of_device_id vci_ioc_pf_of_ids[] = {
	{ .compatible = "soclib,vci_block_device_tsar_v4" },
	{ .compatible = "soclib,vci_block_device_tsar_v5" },
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
	int ret;

	pr_debug("Registering SoCLib VCI IOC driver\n");

	vci_ioc_major = register_blkdev(vci_ioc_major, DEVICE_NAME);
	if (vci_ioc_major <= 0)
	{
		ret = -ENOMEM;
		goto err_blk;
	}

	ret = platform_driver_register(&vci_ioc_pf_driver);
	if (ret)
		goto err_blk;

	pr_info("SoCLib VCI IOC driver, major=%i\n", vci_ioc_major);
	return 0;

err_blk:
	pr_err("vci_ioc: registration failed (ret=%i)\n", ret);
	return ret;
}

static void __exit vci_ioc_exit(void)
{
	pr_debug("Unregistering SoCLib VCI IOC driver\n");
	platform_driver_unregister(&vci_ioc_pf_driver);
	unregister_blkdev(vci_ioc_major, DEVICE_NAME);
}

module_init(vci_ioc_init);
module_exit(vci_ioc_exit);

/* MODULE information */
MODULE_AUTHOR("Joël Porquet <joel.porquet@lip6.fr>");
MODULE_DESCRIPTION("SoCLib VCI IOC driver");
MODULE_LICENSE("GPL");
