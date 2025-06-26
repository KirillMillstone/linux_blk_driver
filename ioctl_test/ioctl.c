#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "../shared_interface.h"

#define MAX_THREADS     (3)

int main()
{
    int fd = open("/dev/"MY_BLKDEV_NAME, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device\n");
        return -1;
    }

    const group_type_t groups[] = {
        GR_NOSYNCH,
        GR_MUTEX,
        GR_SEM,
        GR_SPIN
    };

    int thread_ids[N_GROUPS][MAX_THREADS] = {}; // Thread id's in groups

    int ret;
    printf("------- Test IOCTL 1: Create threads -------\n");
    for (size_t i = 0; i < sizeof(groups)/sizeof(group_type_t); i++) {
        th_create_params_t params = {
            .group_id = groups[i],
            .global_id = -1,
            .start = 0
        };

        ret = ioctl(fd, IOCTL_CREATE_THREAD, &params);
        if (ret < 0) {
            perror("IOCTL failed\n");
        }

        printf("Global id: %d\n", params.global_id);
        thread_ids[i][0] = params.global_id;
    }

    printf("------- Test IOCTL 2: Run thread by ID -------\n");
    ret = ioctl(fd, IOCTL_START_THREAD_BY_ID, &thread_ids[GR_NOSYNCH][0]);
    if (ret < 0) {
        perror("IOCTL failed\n");
    }

    printf("------- Test IOCTL 8: Count all created threads -------\n");
    int created_cnt = 0;
    ret = ioctl(fd, IOCTL_CNT_CREATED_THREADS, &created_cnt);
    if (ret < 0) {
        perror("IOCTL failed\n");
    }
    printf("Total created threads: %d\n", created_cnt);

    printf("------- Test IOCTL 3: Create & Run threads -------\n");
    for (size_t i = 0; i < sizeof(groups)/sizeof(group_type_t); i++) {
        th_create_params_t params = {
            .group_id = groups[i],
            .global_id = -1,
            .start = 0
        };

        ret = ioctl(fd, IOCTL_RUN_THREAD, &params);
        if (ret < 0) {
            perror("IOCTL failed\n");
        }

        printf("Global id: %d\n", params.global_id);
        thread_ids[i][1] = params.global_id;
    }

    printf("------- Test IOCTL 4: Run already created threads -------\n");
    ret = ioctl(fd, IOCTL_START_THREADS);
    if (ret < 0) {
            perror("IOCTL failed\n");
    }

    printf("------- Test IOCTL 5: Terminate thread in by ID -------\n");
    ret = ioctl(fd, IOCTL_TERMINATE_THREAD_BY_ID, &thread_ids[GR_MUTEX][1]);
    if (ret < 0) {
        perror("IOCTL failed\n");
    }

    printf("------- Test IOCTL 6: Terminate threads in group -------\n");
    // Getting group types in array of group types by group type. Terrific.
    ret = ioctl(fd, IOCTL_TERMINATE_GROUP_THREADS, &groups[GR_SEM]);
    if (ret < 0) {
        perror("IOCTL failed\n");
    }

    printf("------- Test IOCTL 7: Count all active threads -------\n");
    int active_cnt = 0;
    ret = ioctl(fd, IOCTL_CNT_THREADS, &active_cnt);
    if (ret < 0) {
        perror("IOCTL failed\n");
    }
    printf("Total active threads: %d\n", active_cnt);


    printf("------- Test IOCTL 8: Count all running threads -------\n");
    int running_cnt = 0;
    ret = ioctl(fd, IOCTL_CNT_RUNNING_THREADS, &running_cnt);
    if (ret < 0) {
        perror("IOCTL failed\n");
    }
    printf("Total running threads: %d\n", running_cnt);

    printf("------- Test IOCTL 9: Count all terminated threads -------\n");
    int terminated_cnt = 0;
    ret = ioctl(fd, IOCTL_CNT_TERMINATED_THREADS, &terminated_cnt);
    if (ret < 0) {
        perror("IOCTL failed\n");
    }
    printf("Total terminated threads: %d\n", terminated_cnt);

    printf("------- Test IOCTL 10-12: Count threads in groups -------\n");
    for (size_t i = 0; i < sizeof(groups)/sizeof(group_type_t); i++){
        int running_in_group = groups[i];
        int created_in_group = groups[i];
        int terminated_in_group = groups[i];
        ret = ioctl(fd, IOCTL_CNT_RUNNING_THREADS_IN_GROUP, &running_in_group);
        if (ret < 0) {
            perror("IOCTL failed\n");
        }
        ret = ioctl(fd, IOCTL_CNT_CREATED_THREADS_IN_GROUP, &created_in_group);
        if (ret < 0) {
            perror("IOCTL failed\n");
        }
        ret = ioctl(fd, IOCTL_CNT_TERMINATED_THREADS_IN_GROUP, &terminated_in_group);
        if (ret < 0) {
            perror("IOCTL failed\n");
        }
        printf("Group %ld. \n", i);
        printf("Running threads:    %d\n", running_in_group);
        printf("Created threads:    %d\n", created_in_group);
        printf("Terminated threads: %d\n", terminated_in_group);
    }

    for (size_t i = 0; i < sizeof(groups)/sizeof(group_type_t); i++) {
        int shared_var = groups[i];
        ret = ioctl(fd, IOCTL_GET_SHARED_VARIABLE_VALUE, &shared_var);
        if (ret < 0) {
            perror("IOCTL failed\n");
        }
        printf("Group %ld. Shared variable value: %d \n", i, shared_var);
    }

    printf("------- Test IOCTL 13: Thread info -------\n");
    th_info_t info = {};
    info.th_gr_total[GR_NOSYNCH] = 1;
    info.th_gr_total[GR_MUTEX] = 1;

    // Create some threads for count
    for (size_t i = 0; i < sizeof(groups)/sizeof(group_type_t); i++) {
        th_create_params_t params = {
            .group_id = groups[i],
            .global_id = -1,
            .start = 0
        };

        ret = ioctl(fd, IOCTL_CREATE_THREAD, &params);
        if (ret < 0) {
            perror("IOCTL failed\n");
        }
    }

    ret = ioctl(fd, IOCTL_THREADS_INFO, &info);
    if (ret < 0) {
        perror("IOCTL failed\n");
    }
    
    printf("Total created threads:    %d\n", info.th_total);
    printf("Total running threads:    %d\n", info.th_running);
    printf("Total terminated threads: %d\n", info.th_terminated);

    for (size_t i = 0; i < sizeof(groups)/sizeof(group_type_t); i++) {
        printf("Group %ld. \n", i);
        if (info.th_gr_total[i])
            printf("Created threads:    %d\n", info.th_gr_total[i]);
        printf("Running threads:    %d\n", info.th_gr_running[i]);
        printf("Terminated threads: %d\n", info.th_gr_terminated[i]);
    }

    printf("------- Test IOCTL 14: Debug message -------\n");
    th_info_t info2 = {};
    info2.th_gr_total[GR_NOSYNCH] = 1;
    info2.th_gr_total[GR_MUTEX] = 1;

    ret = ioctl(fd, IOCTL_DBG_MSG_THREADS_INFO, &info2);
    if (ret < 0) {
        perror("IOCTL failed\n");
    }

    printf("------- Test IOCTL 7: Terminate all threads -------\n");
    ret = ioctl(fd, IOCTL_TERMINATE_ALL_THREADS);
    if (ret < 0) {
            perror("IOCTL failed\n");
    }

    close(fd);
    return 0;
}