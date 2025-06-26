#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/genhd.h>
#include <linux/moduleparam.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include "shared_interface.h"

#define NR_SECTORS      512

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Zhernov Kirill");
MODULE_VERSION("6");
MODULE_DESCRIPTION("Kernel module that will earn me za4et");
int init_shared_cnt = 0;
int init_shared_vars[N_GROUPS] = {};
module_param_array(init_shared_vars, int, &init_shared_cnt, 0);
MODULE_PARM_DESC(init_shared_vars, "Array of integer shared variables for each group. Usage: init_shared_vars=1,2,3,4");

static int block_major; // Number will be allocated after calling register_blkdev

static struct mutex group_mutex;
static struct semaphore group_sem;
static spinlock_t group_spinlock;
static unsigned int thread_cnt;

typedef enum {
    CREATED,
    RUNNING,
    TERMINATED,
} th_state_t;

typedef struct {
    struct list_head thread_node;
    struct task_struct* task;
    group_type_t group_type;
    unsigned int global_id;
    th_state_t state;
    void* synch_obj;
    int* pshared_var;
} blk_thread_t;

typedef struct {
    struct list_head thread_head;
    struct mutex list_mutex;
    int shared_var;
    void* group_synch_obj;
    unsigned int created_count;
    unsigned int running_count;
    unsigned int terminated_count;
} thread_group_t;

static thread_group_t groups[N_GROUPS];

/*
*   Main structure
*/
static struct my_device {
    struct blk_mq_tag_set tag_set;
    struct request_queue *queue;
    struct gendisk *gd;
} dev;

static int my_block_open(struct block_device *bdev, fmode_t mode);
static void my_block_release(struct gendisk *gd, fmode_t mode);
int thread_func(void* data);
static void cleanup_threads(void);
static int blk_create_thread(th_create_params_t params);
static int my_block_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg);
static blk_status_t my_queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd);

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

static void groups_init(void);
static int create_block_device(struct my_device *dev);
static int __init drv_block_init(void);
static void delete_block_device(struct my_device *dev);
static void __exit drv_block_exit(void);
