#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/blk-mq.h>
#include <linux/list.h>
// #include <linux/fs.h>

#define MY_BLKDEV_NAME  "zhernov_blk_6"
#define MY_BLK_SECT     (8 * 1024)

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

static int my_block_open(struct gendisk *disk, blk_mode_t mode)
{
    return 0;
}

static void my_block_release(struct gendisk *gd)
{
    return;
}

static blk_status_t my_block_request(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd)
{
    return BLK_STS_OK;
}

struct blk_mq_tag_set tag_set;
struct request_queue *queue;
struct gendisk *disk;
struct block_device_operations dops = {
    .owner = THIS_MODULE,
    .open = my_block_open,
    .release = my_block_release
};
static struct blk_mq_ops my_queue_ops = {
   .queue_rq = my_block_request
};


static int block_major; // Number will be allocated after calling register_blkdev

static int __init drv_block_init(void)
{
    block_major = register_blkdev(block_major, MY_BLKDEV_NAME);
    if (block_major < 0) {
        printk(KERN_ERR "Unable to register custom block device\n");
        return -EBUSY;
    }

    disk = blk_alloc_disk(1);
    if (IS_ERR(disk)) {
        printk(KERN_ERR "Alloc disk failed: %ld\n", PTR_ERR(disk));
        unregister_blkdev(block_major, MY_BLKDEV_NAME);
        return -ENOMEM;
    }

    disk->major = block_major;
    disk->first_minor = 0;
    disk->minors = 1;
    disk->flags = GENHD_FL_NO_PART;
    disk->fops = &dops;
    strscpy(disk->disk_name, "blkdrv", sizeof(disk->disk_name));
    set_capacity(disk, MY_BLK_SECT);

    memset(&tag_set, 0, sizeof(tag_set));
    tag_set.ops = &my_queue_ops;
    tag_set.nr_hw_queues = 1;
    tag_set.queue_depth = 128;
    tag_set.numa_node = NUMA_NO_NODE;
    tag_set.cmd_size = 0;
    tag_set.flags = BLK_MQ_F_SHOULD_MERGE;

    int err = blk_mq_alloc_tag_set(&tag_set);
    if (err) {
        printk(KERN_ERR "Failed to allocate tag set\n");
        unregister_blkdev(block_major, MY_BLKDEV_NAME);
        return -ENOMEM;
    }

    queue = blk_mq_init_queue(&tag_set);

    if (IS_ERR(queue)) {
        blk_mq_free_tag_set(&tag_set);
        put_disk(disk);
        unregister_blkdev(block_major, MY_BLKDEV_NAME);
        return PTR_ERR(queue);
    }

    blk_queue_logical_block_size(queue, 512);
    blk_queue_physical_block_size(queue, 512);

    disk->queue = queue;

    err = add_disk(disk);
    if (err) {
        printk(KERN_ERR "add_disk failed: %d\n", err);
        blk_mq_destroy_queue(queue);
        blk_mq_free_tag_set(&tag_set);
        put_disk(disk);
        unregister_blkdev(block_major, MY_BLKDEV_NAME);
        return err;
    }

    printk("Test module initialized: %d\n", block_major);
    return 0;
}

static void __exit drv_block_exit(void)
{
    if (disk) {
        del_gendisk(disk);

        if (queue) {
            blk_mq_destroy_queue(queue);
            queue = NULL;
        }

        blk_mq_free_tag_set(&tag_set);
        put_disk(disk);
        disk = NULL;
    }

    if (block_major > 0) {
        unregister_blkdev(block_major, MY_BLKDEV_NAME);
        block_major = 0;
    }

    printk("Test module exit called\n");
}

module_init(drv_block_init);

module_exit(drv_block_exit);