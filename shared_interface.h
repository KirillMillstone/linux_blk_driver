#define N_GROUPS    (4)

typedef enum {
    GR_NOSYNCH,
    GR_MUTEX,
    GR_SEM,
    GR_SPIN
} group_type_t;

typedef struct {
    group_type_t group_id;
    int start;
} th_create_params_t;

typedef struct {
    unsigned int th_total;
    unsigned int th_running;
    unsigned int th_terminated;
    unsigned int th_gr_total[N_GROUPS];
    unsigned int th_gr_running[N_GROUPS];
    unsigned int th_gr_terminated[N_GROUPS];
} th_info_t;

#define IOCTL_CREATE_THREAD                     _IOW    (BLKDRV_MAGIC, 0xb0, group_type_t) 
#define IOCTL_RUN_THREAD                        _IOW    (BLKDRV_MAGIC, 0xb1, group_type_t)
#define IOCTL_START_THREAD_BY_ID                _IOW    (BLKDRV_MAGIC, 0xb2, int)
#define IOCTL_START_THREADS                     _IO     (BLKDRV_MAGIC, 0xb3)
#define IOCTL_TERMINATE_THREAD_BY_ID            _IOW    (BLKDRV_MAGIC, 0xb4, int)
#define IOCTL_TERMINATE_GROUP_THREADS           _IOW    (BLKDRV_MAGIC, 0xb5, int)
#define IOCTL_TERMINATE_ALL_THREADS             _IO     (BLKDRV_MAGIC, 0xb6)
#define IOCTL_CNT_THREADS                       _IOR    (BLKDRV_MAGIC, 0xb7, int)
#define IOCTL_CNT_RUNNING_THREADS               _IOR    (BLKDRV_MAGIC, 0xb8, int)
#define IOCTL_CNT_CREATED_THREADS               _IOR    (BLKDRV_MAGIC, 0xb9, int)
#define IOCTL_CNT_TERMINATED_THREADS            _IOR    (BLKDRV_MAGIC, 0xba, int)
#define IOCTL_CNT_THREADS_IN_GROUP              _IOR    (BLKDRV_MAGIC, 0xbb, int)
#define IOCTL_CNT_RUNNING_THREADS_IN_GROUP      _IOWR   (BLKDRV_MAGIC, 0xbc, int)
#define IOCTL_CNT_CREATED_THREADS_IN_GROUP      _IOWR   (BLKDRV_MAGIC, 0xbd, int)
#define IOCTL_CNT_TERMINATED_THREADS_IN_GROUP   _IOWR   (BLKDRV_MAGIC, 0xbe, int)
#define IOCTL_GET_SHARED_VARIABLE_VALUE         _IOWR   (BLKDRV_MAGIC, 0xbf, int)
#define IOCTL_THREADS_INFO                      _IOWR   (BLKDRV_MAGIC, 0xc0, th_info_t)
#define IOCTL_DBG_MSG_THREADS_INFO              _IOW    (BLKDRV_MAGIC, 0xc1, th_info_t)
