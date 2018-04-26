#ifndef __ENV_H__
#define __ENV_H__

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#define DEFAULT_PLATFORM 0
#define AMD_PLATFORM 1
#define INTEL_PLATFORM 2
#define NVIDIA_PLATFORM 3
#define INTEL_21_EXPERIMENTAL 4

#define MAX_QUEUES 64

#define ERROR_FILE_NOT_FOUND 1000
#define ERROR_UNEXPECTED_IO_ERROR 1001
#define ERROR_INVALID_PROGRAM 1002

#define SH_BUF_READ CL_MEM_READ_ONLY
#define SH_BUF_WRITE CL_MEM_WRITE_ONLY
#define SH_BUF_RW CL_MEM_READ_WRITE

typedef struct {
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queues[MAX_QUEUES];
    unsigned char allocated_queues;
    char *device_name;
} env_t;

typedef struct {
    env_t *env;
    cl_program program;
    unsigned char built;
    char *build_log;
} env_program_t;

typedef int queue_id_t;

#ifdef ENABLE_KERNEL_PROFILER
typedef struct {
    cl_ulong queued;
    cl_ulong submitted;
    cl_ulong started;
    cl_ulong finished;
    cl_ulong timer_resolution;
} profile_info_t;
#endif

typedef struct {
    env_program_t *program;
    cl_kernel kernel;
    cl_command_queue q;
    queue_id_t q_id;
    size_t global_sz;
    size_t local_sz;
    unsigned char args_passed;
    cl_event event;
#if ENABLE_KERNEL_PROFILER
    profile_info_t profile_info;
#endif
} env_kernel_t;

typedef struct {
    size_t size;  // Actual requested size during allocation
    size_t total_size;  // buffer may be padded to be multiple of cache line size. always >= requested_size.
    env_t *env;
    cl_mem device_handler;
    void *host_handler;
    void *mapped_ptr;
    cl_command_queue queued_on;
} shared_buf_t;

#ifdef ENABLE_KERNEL_PROFILER
#define NS_IN_SEC 1000000000
#define get_time_to_submit(_kernel) (_kernel.profile_info->submitted - _kernel.profile_info->queued)
#define get_time_to_start(_kernel) (_kernel.profile_info->started - _kernel.profile_info->queued)
#define get_time_to_finish(_kernel) (_kernel.profile_info->finished - _kernel.profile_info->queued)
#define get_error_margin(_kernel) (_kernel.profile_info->timer_resolution)
#define print_kexec_time(_kernel) (printf("%.4f\n", (double)(_kernel.profile_info->finished - _kernel.profile_info->started) / NS_IN_SEC))
#else
#define get_time_to_submit(_kernel) -1
#define get_time_to_start(_kernel) -1
#define get_time_to_finish(_kernel) -1
#define get_error_margin(_kernel) -1
#define print_kexec_time(_kernel) 
#endif

#define default_queue(_env) (_env.queues[0])

int env_init(env_t *env, int platform);
void env_destroy(env_t *env);

int env_program_init(env_program_t *program, env_t *env, const char *filename, const char *compile_flags);
void env_program_destroy(env_program_t *program);
char *env_build_status(env_program_t *prog);

queue_id_t env_new_queue(env_t *env);
#define valid_queue_id(_q_id) (_q_id >= 0)
#define env_flush_queue(_env, _q_id) (clFinish(_env->queues[_q_id]))

char *get_device_name(env_t *env);

int env_kernel_init(env_kernel_t *kernel, env_program_t *program, const char *kfn, size_t global_sz, size_t local_sz);
void env_kernel_destroy(env_kernel_t *kernel);
int _env_set_karg(env_kernel_t *kernel, unsigned int index, size_t arg_sz, const void *val);
#define env_set_karg(_kern, _arg_sz, _arg_val) _env_set_karg(_kern, (_kern)->args_passed, _arg_sz, _arg_val)
#define env_set_sb_karg(_kern, _shbuf) _env_set_karg(_kern, (_kern)->args_passed, sizeof(cl_mem), &(_shbuf->device_handler))
int env_enqueue_kernel(env_kernel_t *kernel, queue_id_t q_id, unsigned int work_dim);

size_t get_cache_size(const env_t *env);

shared_buf_t *create_shared_buffer(size_t size, env_t *env, cl_mem_flags);
void destroy_shared_buffer(shared_buf_t *buf);
void *map_shbuf(shared_buf_t *buf, queue_id_t q_id, cl_map_flags flags);
void unmap_shbuf(shared_buf_t *buf);

#endif