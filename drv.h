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

