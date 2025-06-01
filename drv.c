#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/list.h>
// #include <linux/fs.h>

#define MY_BLKDEV_NAME  "zhernov_blk_6"

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

static int block_major; // Number will be allocated after calling register_blkdev

static int __init drv_block_init(void)
{
    block_major = register_blkdev(block_major, MY_BLKDEV_NAME);
    if (block_major < 0) {
        printk(KERN_ERR "Unable to register custom block device\n");
        return -EBUSY;
    }

    printk("Test module initialized: %d\n", block_major);
    return 0;
}

static void __exit drv_block_exit(void)
{
    if (block_major > 0)
        unregister_blkdev(block_major, MY_BLKDEV_NAME);

    printk("Test module exit called\n");
}

module_init(drv_block_init);

module_exit(drv_block_exit);