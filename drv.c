#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/blkdev.h>
// #include <linux/fs.h>

#define MY_BLOCK_MAJOR  240
#define MY_BLKDEV_NAME  "Zhernov_blk_6"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Zhernov Kirill");
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Kernel module that will earn me za4et");

static int __init drv_block_init(void)
{
    int status;

    status = register_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
    if (status < 0) {
             printk(KERN_ERR "unable to register custom block device\n");
             return -EBUSY;
    }

    printk("Test module initialized\n");
    return 0;
}

static void __exit drv_block_exit(void)
{
    unregister_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
    printk("Test module exit called\n");
}

module_init(drv_block_init);

module_exit(drv_block_exit);