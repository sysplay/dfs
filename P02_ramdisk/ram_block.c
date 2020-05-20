/* Disk on RAM Driver */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/genhd.h> // For basic block driver framework
#include <linux/blkdev.h> // For at least, struct block_device_operations
#include <linux/hdreg.h> // For struct hd_geometry
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0))
#include <linux/blk-mq.h>
#endif
#include <linux/errno.h>

#include "ram_device.h"

#define RB_FIRST_MINOR 0
#define RB_MINOR_CNT 16

static u_int rb_major = 0;

/*
 * The internal structure representation of our Device
 */
static struct rb_device
{
	/* Size is the size of the device (in sectors) */
	sector_t size;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0))
	/* For exclusive access to our request queue */
	spinlock_t lock;
#else
	/* Utility structure to store various parameters like queue depth, cmd size, ... */
	struct blk_mq_tag_set tag_set;
#endif
	/* Our request queue */
	struct request_queue *queue;
	/* This is kernel's representation of an individual disk device */
	struct gendisk *disk;
} rb_dev;

static int rb_open(struct block_device *bdev, fmode_t mode)
{
	unsigned unit = iminor(bdev->bd_inode);

	printk(KERN_INFO "rb: Device is opened\n");
	printk(KERN_INFO "rb: Inode number is %d\n", unit);

	if (unit > RB_MINOR_CNT)
		return -ENODEV;
	return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
static int rb_close(struct gendisk *disk, fmode_t mode)
{
	printk(KERN_INFO "rb: Device is closed\n");
	return 0;
}
#else
static void rb_close(struct gendisk *disk, fmode_t mode)
{
	printk(KERN_INFO "rb: Device is closed\n");
}
#endif

static int rb_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	geo->heads = 1;
	geo->cylinders = 32;
	geo->sectors = 32;
	geo->start = 0;
	return 0;
}

/*
 * Actual Data transfer
 */
static int rb_transfer(struct request *req)
{
	//struct rb_device *dev = (struct rb_device *)(req->rq_disk->private_data);

	int dir = rq_data_dir(req);
	sector_t start_sector = blk_rq_pos(req);
	unsigned int sector_cnt = blk_rq_sectors(req);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0))
#define BV_PAGE(bv) ((bv)->bv_page)
#define BV_OFFSET(bv) ((bv)->bv_offset)
#define BV_LEN(bv) ((bv)->bv_len)
	struct bio_vec *bv;
#else
#define BV_PAGE(bv) ((bv).bv_page)
#define BV_OFFSET(bv) ((bv).bv_offset)
#define BV_LEN(bv) ((bv).bv_len)
	struct bio_vec bv;
#endif
	struct req_iterator iter;

	sector_t sector_offset;
	unsigned int sectors;
	u8 *buffer;

	int ret = 0;

	//printk(KERN_DEBUG "rb: Dir:%d; Sec:%lld; Cnt:%d\n", dir, (unsigned long long)(start_sector), sector_cnt);

	sector_offset = 0;
	rq_for_each_segment(bv, req, iter)
	{
		buffer = page_address(BV_PAGE(bv)) + BV_OFFSET(bv);
		if (BV_LEN(bv) % RB_SECTOR_SIZE != 0)
		{
			printk(KERN_ERR "rb: Should never happen: "
				"bio size (%d) is not a multiple of RB_SECTOR_SIZE (%d).\n"
				"This may lead to data truncation.\n",
				BV_LEN(bv), RB_SECTOR_SIZE);
			ret = -EIO;
		}
		sectors = BV_LEN(bv) / RB_SECTOR_SIZE;
		printk(KERN_DEBUG "rb: Start Sector: %llu, Sector Offset: %llu; Buffer: %p; Length: %u sectors\n",
			(unsigned long long)(start_sector), (unsigned long long)(sector_offset), buffer, sectors);
		if (dir == WRITE) /* TODO 4: Write to the device */
		{
			//ramdevice_write(/* from sector */, buffer, /* number of sectors */);
		}
		else /* TODO 5: Read from the device */
		{
			//ramdevice_read(/* from sector */, buffer, /* number of sectors */);
		}
		sector_offset += sectors;
	}
	if (sector_offset != sector_cnt)
	{
		printk(KERN_ERR "rb: bio info doesn't match with the request info");
		ret = -EIO;
	}

	return ret;
}

/*
 * Represents a block I/O request for us to execute
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0))
static void rb_request(struct request_queue *q)
{
	struct request *req;
	int ret;

	/* Gets the current request from the dispatch queue */
	while ((req = blk_fetch_request(q)) != NULL)
	{
		ret = rb_transfer(req);
		/* End the request */
		__blk_end_request_all(req, ret); // Equivalent to the following:
		//__blk_end_request(req, ret, blk_rq_bytes(req));
	}
}
#else
static blk_status_t rb_request(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd)
{
	struct request *req;
	int status;

	/* Gets the current request */
	req = bd->rq;

	/* Start new request procedure w/ a timer to time the processing */
	blk_mq_start_request(req);

	status = ((rb_transfer(req) == 0) ? BLK_STS_OK : BLK_STS_IOERR);

	/* End request procedure */
	blk_mq_end_request(req, status); // Equivalent to the following:
	//if (blk_update_request(req, status, blk_rq_bytes(req))) // GPL-only symbol
	//	BUG();
	//__blk_mq_end_request(req, status);

	return BLK_STS_OK;
}
#endif

/*
 * These are the file operations that performed on the ram block device
 */
static struct block_device_operations rb_fops =
{
	.owner = THIS_MODULE,
	.open = rb_open,
	.release = rb_close,
	.getgeo = rb_getgeo,
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0))
/*
 * This is the call back (request function) setup for the upper layers to process the requests in request queue(s)
 */
static struct blk_mq_ops rb_mq_ops =
{
	.queue_rq = rb_request
};
#endif

/*
 * This is the registration and initialization section of the ram block device
 * driver
 */
static int __init rb_init(void)
{
	sector_t size;

	/* Set up our Disk On RAM (DOR) */
	if ((size = ramdevice_init()) < 0)
	{
		return (int)(size);
	}
	/* TODO 1: Initialize with DOR memory size */
	rb_dev.size = 0;

	/* Get Registered */
	rb_major = register_blkdev(rb_major, "rb");
	if (rb_major <= 0)
	{
		printk(KERN_ERR "rb: Unable to get Major Number\n");
		ramdevice_cleanup();
		return -EBUSY;
	}

	/* Get a request queue (here queue is created) */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0))
	spin_lock_init(&rb_dev.lock);
	rb_dev.queue = blk_init_queue(rb_request, &rb_dev.lock);
#else
	rb_dev.queue = blk_mq_init_sq_queue(&rb_dev.tag_set, &rb_mq_ops, 128, BLK_MQ_F_SHOULD_MERGE);
#endif
	if (IS_ERR(rb_dev.queue))
	{
		printk(KERN_ERR "rb: request queue allocation & initialization failure\n");
		unregister_blkdev(rb_major, "rb");
		ramdevice_cleanup();
		return PTR_ERR(rb_dev.queue);
	}

	/*
	 * Add the gendisk structure
	 * By using this memory allocation is involved,
	 * the minor number we need to pass bcz the device
	 * will support this much partitions
	 */
	rb_dev.disk = alloc_disk(RB_MINOR_CNT);
	if (!rb_dev.disk)
	{
		printk(KERN_ERR "rb: alloc_disk failure\n");
		blk_cleanup_queue(rb_dev.queue);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0))
		blk_mq_free_tag_set(&rb_dev.tag_set);
#endif
		unregister_blkdev(rb_major, "rb");
		ramdevice_cleanup();
		return -ENOMEM;
	}

	/* TODO 2: Setting the major number */
	rb_dev.disk->major = 0;
	/* Setting the first mior number */
	rb_dev.disk->first_minor = RB_FIRST_MINOR;
	/* Initializing the device operations */
	rb_dev.disk->fops = &rb_fops;
	/* Driver-specific own internal data */
	rb_dev.disk->private_data = &rb_dev;
	/* TODO 3: Setting up the request queue */
	rb_dev.disk->queue = NULL;
	/*
	 * You do not want partition information to show up in
	 * cat /proc/partitions set this flags
	 */
	//rb_dev.disk->flags = GENHD_FL_SUPPRESS_PARTITION_INFO;
	sprintf(rb_dev.disk->disk_name, "rb");
	/* Setting the capacity of the device in its gendisk structure */
	set_capacity(rb_dev.disk, rb_dev.size);

	/* Adding the disk to the system */
	add_disk(rb_dev.disk);
	/* Now the disk is "live" */
	printk(KERN_INFO "rb: Ram Block driver initialised (%llu sectors; %llu bytes)\n",
		(unsigned long long)(rb_dev.size), (unsigned long long)(rb_dev.size * RB_SECTOR_SIZE));

	return 0;
}
/*
 * This is the unregistration and uninitialization section of the ram block
 * device driver
 */
static void __exit rb_cleanup(void)
{
	del_gendisk(rb_dev.disk);
	put_disk(rb_dev.disk);
	blk_cleanup_queue(rb_dev.queue);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0))
	blk_mq_free_tag_set(&rb_dev.tag_set);
#endif
	unregister_blkdev(rb_major, "rb");
	ramdevice_cleanup();
}

module_init(rb_init);
module_exit(rb_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia <email@sarika-pugs.com>");
MODULE_DESCRIPTION("Ram Block Driver");
MODULE_ALIAS_BLOCKDEV_MAJOR(rb_major);
