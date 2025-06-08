#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/genhd.h>

#define MY_BLKDEV_NAME  "zhernov_blk_6"
#define NR_SECTORS      512

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Zhernov Kirill");
MODULE_VERSION("6");
MODULE_DESCRIPTION("Kernel module that will earn me za4et");

/*
open, release, ioctl

IOCTL_CREATE_THREAD
IOCTL_RUN_THREAD
IOCTL_START_THREAD_BY_ID
IOCTL_START_THREADS
IOCTL_TERMINATE_THREAD_BY_ID
IOCTL_TERMINATE_GROUP_THREADS
IOCTL_TERMINATE_ALL_THREADS
IOCTL_CNT_THREADS
IOCTL_CNT_RUNNING_THREADS
IOCTL_CNT_CREATED_THREADS
IOCTL_CNT_TERMINATED_THREADS
IOCTL_CNT_THREADS_IN_GROUP
IOCTL_CNT_RUNNING_THREADS_IN_GROUP
IOCTL_CNT_CREATED_THREADS_IN_GROUP
IOCTL_CNT_TERMINATED_THREADS_IN_GROUP
IOCTL_GET_SHARED_VARIABLE_VALUE
IOCTL_THREADS_INFO
IOCTL_DBG_MSG_THREADS_INFO

Group types:
*   Without synch
*   Mutex
*   Bin semaphore
*   Spin-lock

struct task_struck + list_head + atomic + mutex

*/

// struct blk_mq_tag_set tag_set;
// struct request_queue *queue;
// struct gendisk *disk;
// struct block_device_operations dops = {
    // .owner = THIS_MODULE,
    // .open = my_block_open,
    // .release = my_block_release
// };

static int block_major; // Number will be allocated after calling register_blkdev

static int my_block_open(struct block_device *bdev, fmode_t mode)
{
    printk(MY_BLKDEV_NAME ": Device opened\n");
    return 0;
}

static void my_block_release(struct gendisk *gd, fmode_t mode)
{
    printk(MY_BLKDEV_NAME ": Device released\n");
}

/*
*   Main structure
*/
static struct my_device {
    // spinlock_t lock;     I guess we don't need that
    struct blk_mq_tag_set tag_set;
    struct request_queue *queue;
    struct gendisk *gd;
} dev;

static blk_status_t my_queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd)
{
    blk_mq_end_request(bd->rq, BLK_STS_OK);
    return BLK_STS_OK;
}

static const struct blk_mq_ops my_queue_ops = {
    .queue_rq = my_queue_rq,
};

// Block device ops
struct block_device_operations my_block_ops = {
    .owner = THIS_MODULE,
    .open = my_block_open,
    .release = my_block_release
};

/*
*   Init my_device
*/
static int create_block_device(struct my_device *dev)
{
    dev->tag_set.ops = &my_queue_ops;
    dev->tag_set.nr_hw_queues = 1;
    dev->tag_set.queue_depth = 128;
    dev->tag_set.numa_node = NUMA_NO_NODE;
    dev->tag_set.cmd_size = 0;
    dev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;

    int err = blk_mq_alloc_tag_set(&dev->tag_set);
    if (err) {
        printk(KERN_NOTICE "Failed to allocate tag set\n");
        return -ENOMEM;
    }

    dev->queue = blk_mq_init_queue(&dev->tag_set);
    if (IS_ERR(dev->queue)) {
        blk_mq_free_tag_set(&dev->tag_set);
        return -ENOMEM;
    }

    // dev->queue = blk_init_
    dev->gd = alloc_disk(1);
    if (!dev->gd) {
        // blk_cleanup_queue(my_device.queue);
        printk(KERN_NOTICE "Alloc disk failed\n");
        unregister_blkdev(block_major, MY_BLKDEV_NAME);
        return -ENOMEM;
    }

    dev->gd->major          = block_major;
    dev->gd->first_minor    = 0;
    dev->gd->fops           = &my_block_ops;
    dev->gd->queue          = dev->queue;
    // dev->gd->flags          = GENHD_FL_NO_PART_SCAN;
    dev->gd->private_data   = dev;
    strcpy(dev->gd->disk_name, MY_BLKDEV_NAME);
    set_capacity(dev->gd, NR_SECTORS);

    add_disk(dev->gd);

    return 0;
}

// Register device
static int __init drv_block_init(void)
{
    block_major = register_blkdev(block_major, MY_BLKDEV_NAME);
    if (block_major < 0) {
        printk(KERN_ERR "Unable to register custom block device\n");
        return -EBUSY;
    }

    int status = create_block_device(&dev);
    if (status < 0)
        return status;

    printk("Test module initialized: %d\n", block_major);
    return 0;
}

// Delete my_device
static void delete_block_device(struct my_device *dev)
{
    if (dev->gd) {
        del_gendisk(dev->gd);
        put_disk(dev->gd);
    }
    blk_cleanup_queue(dev->queue);
    blk_mq_free_tag_set(&dev->tag_set);
}

// Unregister block device
static void __exit drv_block_exit(void)
{
    printk(MY_BLKDEV_NAME ": Block exit begin\n");

    delete_block_device(&dev);

    unregister_blkdev(block_major, MY_BLKDEV_NAME);

    printk(MY_BLKDEV_NAME ": Block exit end\n");
}

module_init(drv_block_init);

module_exit(drv_block_exit);
