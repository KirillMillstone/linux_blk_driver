#include "drv.h"

#define NR_SECTORS      512

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Zhernov Kirill");
MODULE_VERSION("6");
MODULE_DESCRIPTION("Kernel module that will earn me za4et");
module_param_array(init_shared_vars, int, &init_shared_cnt, 0);
MODULE_PARM_DESC(init_shared_vars, "Array of integer shared variables for each group. Usage: init_shared_vars=1,2,3,4");

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

// int thread_func(void* data)
// {

// }
// int thread_func(void* data)
// {

// }

static int my_block_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    th_create_params_t th_create_params;
    th_info_t thread_info;

    switch (cmd)
    {
    case IOCTL_CREATE_THREAD:
        printk(MY_BLKDEV_NAME ": IOCTL recieved\n");
        copy_from_user(&th_create_params, (th_create_params_t __user *)arg, sizeof(th_create_params));
        printk(MY_BLKDEV_NAME ": Create params: {start = %d, group_id = %d}\n", th_create_params.start, th_create_params.group_id);
        blk_thread_t* th = kmalloc(sizeof(blk_thread_t), GFP_KERNEL);
        th->state = CREATED;
        list_add_tail(&th->thread_node, &groups[th_create_params.group_id].thread_head);

        int i;
        for (i = 0; i < N_GROUPS; i++) {
            if (!list_empty(&groups[i].thread_head)) {
                struct list_head* curr;
                list_for_each(curr, &groups[i].thread_head) {
                    blk_thread_t* tp = list_entry(curr, blk_thread_t, thread_node);
                    printk(MY_BLKDEV_NAME ": %d\n", tp->state);
                }
            }
            else {
                printk(MY_BLKDEV_NAME ": List %d is empty\n", i);
            }
        }

        break;
    
    default:
        printk(KERN_ERR MY_BLKDEV_NAME ": Unknown IOCTL\n");
        break;
    }
    switch (cmd)
    {
    case IOCTL_CREATE_THREAD:
        printk(MY_BLKDEV_NAME ": IOCTL recieved\n");
        copy_from_user(&th_create_params, (th_create_params_t __user *)arg, sizeof(th_create_params));
        printk(MY_BLKDEV_NAME ": Create params: {start = %d, group_id = %d}\n", th_create_params.start, th_create_params.group_id);
        blk_thread_t* th = kmalloc(sizeof(blk_thread_t), GFP_KERNEL);
        th->state = CREATED;
        list_add_tail(&th->thread_node, &groups[th_create_params.group_id].thread_head);

        int i;
        for (i = 0; i < N_GROUPS; i++) {
            if (!list_empty(&groups[i].thread_head)) {
                struct list_head* curr;
                list_for_each(curr, &groups[i].thread_head) {
                    blk_thread_t* tp = list_entry(curr, blk_thread_t, thread_node);
                    printk(MY_BLKDEV_NAME ": %d\n", tp->state);
                }
            }
            else {
                printk(MY_BLKDEV_NAME ": List %d is empty\n", i);
            }
        }

        break;
    
    default:
        printk(KERN_ERR MY_BLKDEV_NAME ": Unknown IOCTL\n");
        break;
    }
    return ret;
}

/*
*   Main structure
*/
static struct my_device {
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
    .release = my_block_release,
    .ioctl = my_block_ioctl,
    .compat_ioctl = my_block_ioctl,
};

static void groups_init(void)
{
    int i;
    for (i = 0; i < N_GROUPS; i++) {
        INIT_LIST_HEAD(&groups[i].thread_head);

        if (list_empty(&groups[i].thread_head)) {
            printk(MY_BLKDEV_NAME ": Group list %d initialized and empty\n", i);
        }

        // Init counters & shared vars
        groups[i].created_count = 0;
        groups[i].running_count = 0;
        groups[i].terminated_count = 0;
        groups[i].shared_var = init_shared_vars[i];

        printk(MY_BLKDEV_NAME ": Group list %d initial shared var: %d\n", i, groups[i].shared_var);

        // Thread list protection mutex
        mutex_init(&groups[i].list_mutex);
    }

    // Synch objects for groups
    mutex_init(&group_mutex);
    sema_init(&group_sem, 1);
    spin_lock_init(&group_spinlock);
}

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

    groups_init();

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

static void cleanup_threads(void)
{
    int i;
    for (i = 0; i < N_GROUPS; i++) {

        if (list_empty(&groups[i].thread_head)) {
            printk(KERN_INFO "Group %d already empty\n", i);
            continue;
        }

        blk_thread_t *curr, *tmp;
        list_for_each_entry_safe(curr, tmp, &groups[i].thread_head, thread_node) {
            list_del_init(&curr->thread_node);

        if (list_empty(&groups[i].thread_head)) {
            printk(KERN_INFO "Group %d already empty\n", i);
            continue;
        }

        blk_thread_t *curr, *tmp;
        list_for_each_entry_safe(curr, tmp, &groups[i].thread_head, thread_node) {
            list_del_init(&curr->thread_node);
        }
    }
}

// Delete my_device
static void delete_block_device(struct my_device *dev)
{
    cleanup_threads();

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
