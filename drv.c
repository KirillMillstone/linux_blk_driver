#include "drv.h"

static int my_block_open(struct block_device *bdev, fmode_t mode)
{
    printk(MY_BLKDEV_NAME ": Device opened\n");
    return 0;
}

static void my_block_release(struct gendisk *gd, fmode_t mode)
{
    printk(MY_BLKDEV_NAME ": Device released\n");
}

int thread_func(void* data)
{
    blk_thread_t* curr_th = (blk_thread_t*)data;
    group_type_t type = curr_th->group_type;
    int pid = curr_th->global_id;
    void* synch_obj = curr_th->synch_obj;
    int* pshared_var = curr_th->pshared_var;
    printk(MY_BLKDEV_NAME ": Thread woken up. Pid: %d. Group %d. Shared var: %d\n", pid, type, *pshared_var);

    while(!kthread_should_stop()) {
        switch (type)
        {
        case GR_NOSYNCH:
            (*pshared_var)++;
            break;
        case GR_MUTEX:
            mutex_lock((struct mutex *)synch_obj);
            (*pshared_var)++;
            mutex_unlock((struct mutex *)synch_obj);
            break;
        case GR_SEM:
            down((struct semaphore *)synch_obj);
            (*pshared_var)++;
            up((struct semaphore *)synch_obj);
            break;
        case GR_SPIN:
            spin_lock((spinlock_t *)synch_obj);
            (*pshared_var)++;
            spin_unlock((spinlock_t *)synch_obj);
            break;
        }
        if (kthread_should_stop()) {
            printk(MY_BLKDEV_NAME ": Thread should stop recieved\n");
            break;
        }
        msleep(10);
    }

    mutex_lock(&groups[type].list_mutex);
    groups[type].running_count--;
    groups[type].terminated_count++;
    curr_th->state = TERMINATED;
    mutex_unlock(&groups[type].list_mutex);

    printk(MY_BLKDEV_NAME ": Thread terminated. Pid: %d. Group %d. Shared var: %d\n", pid, type, *pshared_var);

    return 0;
}

// Create blk_thread
static int blk_create_thread(th_create_params_t params)
{
    blk_thread_t* new_th = kmalloc(sizeof(blk_thread_t), GFP_KERNEL);
    if (new_th == NULL) {
        printk(KERN_ERR MY_BLKDEV_NAME " Failed to allocate memory for thread\n");
        return -1;
    }
    new_th->global_id = thread_cnt; // Update global variable after adding thread to list
    new_th->group_type = params.group_id;
    new_th->state = CREATED;    // NOT running

    // Group id verified before in IOCTL handler
    new_th->synch_obj = groups[params.group_id].group_synch_obj;
    new_th->pshared_var = &groups[params.group_id].shared_var;

    // Create thread
    new_th->task = kthread_create(thread_func, new_th, "blk_thread_%d", thread_cnt);
    
    if (IS_ERR(new_th->task)) {
        printk(KERN_ERR MY_BLKDEV_NAME " kthread_create failed\n");
        kfree(new_th);
        return -1;
    }

    mutex_lock(&groups[params.group_id].list_mutex);
    thread_cnt++;
    mutex_unlock(&groups[params.group_id].list_mutex);

    new_th->global_id = new_th->task->pid;
    printk(MY_BLKDEV_NAME ": Thread created. Pid: %d. Group %d\n", new_th->global_id, new_th->group_type);

    // Add node to group list
    mutex_lock(&groups[params.group_id].list_mutex);

    list_add_tail(&new_th->thread_node, &groups[params.group_id].thread_head);
    groups[params.group_id].created_count++;

    mutex_unlock(&groups[params.group_id].list_mutex);

    if (params.start && (new_th->state == CREATED)) {
        mutex_lock(&groups[params.group_id].list_mutex);

        new_th->state = RUNNING;
        groups[params.group_id].created_count--;
        groups[params.group_id].running_count++;

        wake_up_process(new_th->task);

        mutex_unlock(&groups[params.group_id].list_mutex);
    }

    return new_th->global_id;
}

static int my_block_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
    int copy_ret = 0;
    th_create_params_t th_create_params;
    th_info_t thread_info = {};
    group_type_t type;
    int thread_id = 0;  // ID from user
    int i = 0;

    int th_active_cnt = 0;
    int th_running_cnt = 0;
    int th_created_cnt = 0;
    int th_terminated_cnt = 0;

    int shared_var_user = 0;

    switch (cmd)
    {
    case IOCTL_RUN_THREAD:
    case IOCTL_CREATE_THREAD:

        // Get group type from user
        copy_ret = copy_from_user(&th_create_params, (th_create_params_t __user *)arg, sizeof(th_create_params_t));

        if(copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy from user failed. Return: %d", copy_ret);
            return -EFAULT;
        }

        // Create or create & run thread based on IOCTL
        th_create_params.start = cmd == IOCTL_RUN_THREAD ? 1 : 0;

        if (!(0 <= th_create_params.group_id && th_create_params.group_id < N_GROUPS)) {
            printk(MY_BLKDEV_NAME ": Invalid group ID %d\n", th_create_params.group_id);
            return -EINVAL;
        }

        // Get id
        th_create_params.global_id = blk_create_thread(th_create_params);
        if (th_create_params.global_id == -1) {
            return -EFAULT;
        }

        // Return thread id to user
        copy_ret = copy_to_user((th_create_params_t __user *)arg, &th_create_params, sizeof(th_create_params_t));

        if(copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy to user failed. Return: %d", copy_ret);
            return -EFAULT;
        }
        break;

    case IOCTL_START_THREAD_BY_ID:

        copy_ret = copy_from_user(&thread_id, (int __user *)arg, sizeof(int));
        if(copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy from user failed. Return: %d", copy_ret);
            return -EFAULT;
        }

        // Find this thread
        // Could use find find_task_by_vpid, but we need to update thread counters and thread state.
        for (i = 0; i < N_GROUPS; i++) {
            blk_thread_t *curr;
            list_for_each_entry(curr, &groups[i].thread_head, thread_node) {
                if ((curr->state == CREATED) && (curr->global_id == thread_id)) {

                    mutex_lock(&groups[i].list_mutex);
                    curr->state = RUNNING;
                    groups[i].created_count--;
                    groups[i].running_count++;

                    wake_up_process(curr->task);

                    mutex_unlock(&groups[i].list_mutex);
                }
            }
        }
        break;

    case IOCTL_START_THREADS:
        // Basically, same as previous, but for all threads;
        for (i = 0; i < N_GROUPS; i++) {
            blk_thread_t *curr;
            list_for_each_entry(curr, &groups[i].thread_head, thread_node) {
                if (curr->state == CREATED) {

                    mutex_lock(&groups[i].list_mutex);
                    curr->state = RUNNING;
                    groups[i].created_count--;
                    groups[i].running_count++;

                    wake_up_process(curr->task);

                    mutex_unlock(&groups[i].list_mutex);
                }
            }
        }
        break;

        case IOCTL_TERMINATE_THREAD_BY_ID:
            copy_ret = copy_from_user(&thread_id, (int __user *)arg, sizeof(int));
            if(copy_ret) {
                printk(KERN_ERR MY_BLKDEV_NAME ": Copy from user failed. Return: %d", copy_ret);
                return -EFAULT;
            }

            printk(MY_BLKDEV_NAME ": Received thread id %d", thread_id);

            blk_thread_t* found_thread = NULL;
            int group_id = -1;

            // Remove this thread from existence (find thread that we need, then remove it from list. Stop it later.)
            for (i = 0; i < N_GROUPS; i++) {
                mutex_lock(&groups[i].list_mutex);
                blk_thread_t *curr;
                list_for_each_entry(curr, &groups[i].thread_head, thread_node) {
                    if (curr->global_id == thread_id) {
                        found_thread = curr;
                        group_id = i;

                        list_del_init(&curr->thread_node);

                        break;
                    }
                }
                mutex_unlock(&groups[i].list_mutex);

                if (found_thread) break;
            }

            // Stop found thread
            if (found_thread) {
                if (found_thread->task && (found_thread->state == RUNNING || found_thread->state == CREATED)) {
            
                    int thread_stop_ret = kthread_stop(found_thread->task);
                    if (thread_stop_ret) {
                        printk(KERN_ERR MY_BLKDEV_NAME ": kthread_stop failed for thread %d: %d\n", thread_id, thread_stop_ret);
                    }
                }
        
                kfree(found_thread);
            }
            else {
                printk(KERN_ERR MY_BLKDEV_NAME ": Thread %d not found\n", thread_id);
                return -ENOENT;
            }
            break;

    case IOCTL_TERMINATE_GROUP_THREADS:
        copy_ret = copy_from_user(&type, (group_type_t __user *)arg, sizeof(group_type_t));
        if (copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy from user failed. Return: %d", copy_ret);
            return -EFAULT;
        }
        printk(MY_BLKDEV_NAME ": Terminate threads in group %d", type);

        struct list_head temp_list;
        INIT_LIST_HEAD(&temp_list);

        // Move this group to temp list
        mutex_lock(&groups[type].list_mutex);

        list_splice_init(&groups[type].thread_head, &temp_list);
        mutex_unlock(&groups[type].list_mutex);

        // Stop group threads
        struct list_head *pos, *n;
        list_for_each_safe(pos, n, &temp_list) {
            blk_thread_t *curr = list_entry(pos, blk_thread_t, thread_node);
        
            list_del(pos);
        
            if (curr->task && (curr->state == RUNNING)) {
                kthread_stop(curr->task);
            }
            else if (curr->task && (curr->state == CREATED)) {
                groups[type].created_count--;
                groups[type].terminated_count++;
                kthread_stop(curr->task);
            }

            kfree(curr);
        }
        break;
    case IOCTL_TERMINATE_ALL_THREADS:
        cleanup_threads();
        break;

    case IOCTL_CNT_THREADS:
        for (i = 0; i < N_GROUPS; i++) {
            mutex_lock(&groups[i].list_mutex);
            th_active_cnt += groups[i].created_count;
            th_active_cnt += groups[i].running_count;
            mutex_unlock(&groups[i].list_mutex);
        }
        copy_ret = copy_to_user((int __user *)arg, &th_active_cnt, sizeof(int));

        if(copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy to user failed. Return: %d", copy_ret);
            return -EFAULT;
        }
        break;

    case IOCTL_CNT_RUNNING_THREADS:
        for (i = 0; i < N_GROUPS; i++) {
            mutex_lock(&groups[i].list_mutex);
            th_running_cnt += groups[i].running_count;
            mutex_unlock(&groups[i].list_mutex);
        }
        copy_ret = copy_to_user((int __user *)arg, &th_running_cnt, sizeof(int));

        if(copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy to user failed. Return: %d", copy_ret);
            return -EFAULT;
        }
        break;
    case IOCTL_CNT_CREATED_THREADS:
        for (i = 0; i < N_GROUPS; i++) {
            mutex_lock(&groups[i].list_mutex);
            th_created_cnt += groups[i].created_count;
            mutex_unlock(&groups[i].list_mutex);
        }
        copy_ret = copy_to_user((int __user *)arg, &th_created_cnt, sizeof(int));

        if(copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy to user failed. Return: %d", copy_ret);
            return -EFAULT;
        }
        break;
    case IOCTL_CNT_TERMINATED_THREADS:
        for (i = 0; i < N_GROUPS; i++) {
            mutex_lock(&groups[i].list_mutex);
            th_terminated_cnt += groups[i].terminated_count;
            mutex_unlock(&groups[i].list_mutex);
        }
        copy_ret = copy_to_user((int __user *)arg, &th_terminated_cnt, sizeof(int));

        if(copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy to user failed. Return: %d", copy_ret);
            return -EFAULT;
        }
        break;
    case IOCTL_CNT_RUNNING_THREADS_IN_GROUP:
        copy_ret = copy_from_user(&type, (int __user *)arg, sizeof(int));
        if (copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy from user failed. Return: %d", copy_ret);
            return -EFAULT;
        }

        mutex_lock(&groups[type].list_mutex);
        th_running_cnt += groups[type].running_count;
        mutex_unlock(&groups[type].list_mutex);

        copy_ret = copy_to_user((int __user *)arg, &th_running_cnt, sizeof(int));

        if(copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy to user failed. Return: %d", copy_ret);
            return -EFAULT;
        }
        break;
    case IOCTL_CNT_CREATED_THREADS_IN_GROUP:
        copy_ret = copy_from_user(&type, (int __user *)arg, sizeof(int));
        if (copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy from user failed. Return: %d", copy_ret);
            return -EFAULT;
        }

        mutex_lock(&groups[type].list_mutex);
        th_created_cnt += groups[type].created_count;
        mutex_unlock(&groups[type].list_mutex);

        copy_ret = copy_to_user((int __user *)arg, &th_created_cnt, sizeof(int));

        if(copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy to user failed. Return: %d", copy_ret);
            return -EFAULT;
        }
        break;
    case IOCTL_CNT_TERMINATED_THREADS_IN_GROUP:
        copy_ret = copy_from_user(&type, (int __user *)arg, sizeof(int));
        if (copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy from user failed. Return: %d", copy_ret);
            return -EFAULT;
        }

        mutex_lock(&groups[type].list_mutex);
        th_terminated_cnt += groups[type].terminated_count;
        mutex_unlock(&groups[type].list_mutex);

        copy_ret = copy_to_user((int __user *)arg, &th_terminated_cnt, sizeof(int));

        if(copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy to user failed. Return: %d", copy_ret);
            return -EFAULT;
        }
        break;
    case IOCTL_GET_SHARED_VARIABLE_VALUE:
        copy_ret = copy_from_user(&type, (int __user *)arg, sizeof(int));
        if (copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy from user failed. Return: %d", copy_ret);
            return -EFAULT;
        }

        mutex_lock(&groups[type].list_mutex);
        shared_var_user = groups[type].shared_var;
        mutex_unlock(&groups[type].list_mutex);

        copy_ret = copy_to_user((int __user *)arg, &shared_var_user, sizeof(int));

        if(copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy to user failed. Return: %d", copy_ret);
            return -EFAULT;
        }
        break;
    case IOCTL_THREADS_INFO:
    case IOCTL_DBG_MSG_THREADS_INFO:
        copy_ret = copy_from_user(&thread_info, (th_info_t __user *)arg, sizeof(th_info_t));

        if (copy_ret) {
            printk(KERN_ERR MY_BLKDEV_NAME ": Copy from user failed. Return: %d", copy_ret);
            return -EFAULT;
        }

        for (i = 0; i < N_GROUPS; i++) {
            mutex_lock(&groups[i].list_mutex);
            thread_info.th_total += groups[i].created_count;
            thread_info.th_running += groups[i].running_count;
            thread_info.th_terminated += groups[i].terminated_count;
            thread_info.th_gr_total[i] = thread_info.th_gr_total[i] ? groups[i].created_count : 0; // Only when requested
            thread_info.th_gr_running[i] = groups[i].running_count;
            thread_info.th_gr_terminated[i] = groups[i].terminated_count;
            mutex_unlock(&groups[i].list_mutex);
        }

        if (cmd == IOCTL_THREADS_INFO) {
            copy_ret = copy_to_user((th_info_t __user *)arg, &thread_info, sizeof(th_info_t));

            if(copy_ret) {
                printk(KERN_ERR MY_BLKDEV_NAME ": Copy to user failed. Return: %d", copy_ret);
                return -EFAULT;
            }
        }
        else {
            printk(MY_BLKDEV_NAME ": Total created threads:     %d\n", thread_info.th_total);
            printk(MY_BLKDEV_NAME ": Total running threads:     %d\n", thread_info.th_running);
            printk(MY_BLKDEV_NAME ": Total terminated threads:  %d\n", thread_info.th_terminated);
            for (i = 0; i < N_GROUPS; i++) {
                printk(MY_BLKDEV_NAME ": Group %d. \n", i);
                if (thread_info.th_gr_total[i])
                    printk(MY_BLKDEV_NAME ": Created threads:    %d\n", thread_info.th_gr_total[i]);
                printk(MY_BLKDEV_NAME ": Running threads:    %d\n", thread_info.th_gr_running[i]);
                printk(MY_BLKDEV_NAME ": Terminated threads: %d\n", thread_info.th_gr_terminated[i]);
            }
        }

        break;
    default:
        printk(KERN_ERR MY_BLKDEV_NAME ": Unknown IOCTL\n");
        return -ENOTTY;
        break;
    }

    return 0;
}

static blk_status_t my_queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd)
{
    blk_mq_end_request(bd->rq, BLK_STS_OK);
    return BLK_STS_OK;
}

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

    groups[GR_NOSYNCH].group_synch_obj = NULL;
    groups[GR_MUTEX].group_synch_obj = &group_mutex;
    groups[GR_SEM].group_synch_obj = &group_sem;
    groups[GR_SPIN].group_synch_obj = &group_spinlock;

    thread_cnt = 0;
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

    dev->gd = alloc_disk(1);
    if (!dev->gd) {
        printk(KERN_NOTICE "Alloc disk failed\n");
        unregister_blkdev(block_major, MY_BLKDEV_NAME);
        return -ENOMEM;
    }

    dev->gd->major          = block_major;
    dev->gd->first_minor    = 0;
    dev->gd->fops           = &my_block_ops;
    dev->gd->queue          = dev->queue;
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
        struct list_head temp_list;
        INIT_LIST_HEAD(&temp_list);

        mutex_lock(&groups[i].list_mutex);
        list_splice_init(&groups[i].thread_head, &temp_list);
        mutex_unlock(&groups[i].list_mutex);

        struct list_head *pos, *n;
        list_for_each_safe(pos, n, &temp_list) {
            blk_thread_t *curr = list_entry(pos, blk_thread_t, thread_node);
            list_del(pos);

            if (curr->task && (curr->state == RUNNING || curr->state == CREATED)) {
                kthread_stop(curr->task);
            }

            kfree(curr);
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
