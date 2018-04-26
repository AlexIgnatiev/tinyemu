#define _XOPEN_SOURCE 700 //for getting timestamps
#include <env.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#include <unistd.h>

#define get_num_cpus() sysconf(_SC_NPROCESSORS_ONLN)
#define TXS_NUM 8
#define READ_SET_PORTION TXS_NUM

#define pclock(_ts) printf("%ld.%04ld\n", _ts.tv_sec, _ts.tv_nsec / 1000000)

struct timespec ts_diff(struct timespec *start, struct timespec *end);
void *tx_validate(void* _arg);
void *tx_validate_host_only(void *_arg);

pthread_mutex_t lock;

typedef struct {
    shared_buf_t *glocks;
    int *ho_glocks;
    size_t ho_glocks_size;
    size_t readset_size;
    env_program_t *program;
    int tid;
} tx_args_t;

struct timespec exec_time;

int main(int argc, char *argv[]) {
    if(argc < 2) {
        fprintf(stderr, "usage: exec host_only:int");
        exit(-1);
    }
    pthread_mutex_init(&lock, NULL);
    int ret = 0;
    int host_only = atoi(argv[1]);

    struct timespec start, end;
    env_t env;
    env_program_t program;
    size_t cache_size;
    unsigned int thread_num = TXS_NUM;  //in case it's necessary to make it not be compile-time constant
    pthread_t threads[TXS_NUM];
    ret = env_init(&env, INTEL_PLATFORM);
    cache_size = get_cache_size(&env);
    size_t global_lock_tbl_size = sizeof(int) * cache_size * get_num_cpus();
    size_t read_set_sz =  global_lock_tbl_size / READ_SET_PORTION;

    if(!host_only) {
        ret |= env_program_init(&program, &env, "src/kernels/main.cl", NULL);
        if(ret) {
            fprintf(stderr, "%s", env_build_status(&program));
        }

        shared_buf_t *glocks = create_shared_buffer(global_lock_tbl_size, &env, SH_BUF_RW);
        queue_id_t qid = env_new_queue(&env);
        int *mapped_glocks = map_shbuf(glocks, qid, CL_MAP_WRITE);
        for (int i = 0; i < glocks->size / sizeof(int); i++) {
            mapped_glocks[i] = 0;
        }
        mapped_glocks[(read_set_sz * 1) / sizeof(int)] = 999999999;
        unmap_shbuf(glocks);

        clock_gettime(CLOCK_MONOTONIC, &start);
        tx_args_t tx_args[TXS_NUM];
        for(int i = 0; i < thread_num; i++) {
            tx_args[i].glocks = glocks;
            tx_args[i].readset_size = read_set_sz;
            tx_args[i].program=&program;
            tx_args[i].tid = i;
            pthread_create(threads + i, NULL, tx_validate, tx_args + i);
        }

        for(int i = 0; i < thread_num; i++) {
            pthread_join(threads[i], NULL);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        exec_time = ts_diff(&start, &end);
        destroy_shared_buffer(glocks);
        env_program_destroy(&program);
    } else {
        int *glocks = (int *) malloc(global_lock_tbl_size);
        memset(glocks, 0, global_lock_tbl_size);
        glocks[(read_set_sz * 4) / sizeof(int)] = 999999999;
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        tx_args_t tx_args[TXS_NUM];
        for(int i = 0; i < thread_num; i++) {
            tx_args[i].ho_glocks = glocks;
            tx_args[i].readset_size = read_set_sz;
            tx_args[i].tid = i;
            tx_args[i].ho_glocks_size = global_lock_tbl_size;
            pthread_create(threads + i, NULL, tx_validate_host_only, tx_args + i);
        }

        for(int i = 0; i < thread_num; i++) {
            pthread_join(threads[i], NULL);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        exec_time = ts_diff(&start, &end);
    }

    env_destroy(&env);
    pclock(exec_time);
    return ret;
}


void populate_readset(shared_buf_t *buf, queue_id_t q_id) {
    int *read_set = (int *) map_shbuf(buf, q_id, CL_MAP_WRITE);
    for(int i = 0; i < buf->size / sizeof(int); i++) {
        read_set[i] = i;
    }
    unmap_shbuf(buf);
}

void init_abort_flag(shared_buf_t *buf, queue_id_t q_id) {
    int *flag = (int *) map_shbuf(buf, q_id, CL_MAP_WRITE);
    *flag = 0;
    unmap_shbuf(buf);
}

void *tx_validate(void* _args) {
    int reterr;
    tx_args_t *args = (tx_args_t *)_args;

    pthread_mutex_lock(&lock);
    shared_buf_t *read_set = create_shared_buffer(args->readset_size, args->program->env, SH_BUF_READ);
    shared_buf_t *abort = create_shared_buffer(sizeof(int), args->program->env, SH_BUF_RW);
    queue_id_t q_id = env_new_queue(args->program->env);
    pthread_mutex_unlock(&lock);

    if(!valid_queue_id(q_id)) {
        fprintf(stderr, "Invalid queue_id");
        return NULL;
    }
    
    env_kernel_t validation_kernel;
    reterr = env_kernel_init(&validation_kernel, args->program, "validate", 1, 1);
    if(reterr) {
        fprintf(stderr, "Transaction failed to init the kernel");
    }
    populate_readset(read_set, q_id);
    init_abort_flag(abort, q_id);
    reterr |= env_set_sb_karg(&validation_kernel, args->glocks);
    reterr |= env_set_karg(&validation_kernel, sizeof(size_t), &(args->glocks->size));
    reterr |= env_set_sb_karg(&validation_kernel, read_set);
    reterr |= env_set_karg(&validation_kernel, sizeof(size_t), &(read_set->size));
    reterr |= env_set_sb_karg(&validation_kernel, abort);
    reterr |= env_set_karg(&validation_kernel, sizeof(int), &(args->tid));

    pthread_mutex_lock(&lock);
    reterr |= env_enqueue_kernel(&validation_kernel, q_id, 1);
    pthread_mutex_unlock(&lock);

    if(reterr) {
        fprintf(stderr, "Transaction failed to set kernel args");
        return NULL;
    }

    env_flush_queue(args->program->env, q_id);

    //printf("thread id=%d - abort=%d\n", args->tid, ((int *) abort->host_handler)[0]);

    return NULL;
}

void *tx_validate_host_only(void* _args) {
    tx_args_t *args = (tx_args_t *) _args;
    int abort = 0;
    int *read_set = (int *) malloc(args->readset_size);

    for(int i = 0; i < args->readset_size / sizeof(int); i++) {
        read_set[i] = i;
    }

    int offset = args->tid*args->readset_size / sizeof(int);
    for(int i = 0; i < args->readset_size / sizeof(int); i++) {
        if(read_set[i] < args->ho_glocks[i + offset]) {
            abort = 1;
            break;
        }
    }

    free(read_set);
    //printf("thread id=%d - abort=%d\n", args->tid, abort);

    return (void *)abort;
}

struct timespec ts_diff(struct timespec *start, struct timespec *end) {
    struct timespec ret = {.tv_nsec = 0, .tv_sec = 0};
    ret.tv_sec = end->tv_sec - start->tv_sec;
    if(end->tv_nsec - start->tv_nsec > 0) {
        ret.tv_nsec = end->tv_nsec - start->tv_nsec;
    } else {
        ret.tv_sec--;
        ret.tv_nsec = 1000000000 - (start->tv_nsec - end->tv_nsec);
    }
    return ret;
}