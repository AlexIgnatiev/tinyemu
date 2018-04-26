#include <env.h>
#include <stdio.h>
#include <string.h>

#define _MAX_PLATFORMS 8
#define _MAX_DEVICES 5

#define _MAX_PLATFORM_VENDOR_NAME_SZ 64
#define _AMD_VENDOR "AMD Accelerated Parallel Processing"
#define _INTEL_VENDOR "Intel(R) OpenCL"
#define _INTEL_2_1_EXPERIMENTAL "Experimental OpenCL 2.1 CPU Only Platform"
#define _NVIDIA_VENDOR ""  //todo: fill this macro

#define PAGE_SIZE 4096
#define CACHE_LINE_SIZE 64

#ifdef ENABLE_KERNEL_PROFILER
typedef struct {
    env_kernel_t *kernel;
    cl_device_id dev;
} profiler_callback_arg_t;

static void CL_CALLBACK kernel_profiler_callback(cl_event event, cl_int event_command_exec_status, void *user_data) {
    cl_int ret_err = 0;
    profiler_callback_arg_t *args = (profiler_callback_arg_t *) user_data;
    ret_err = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), &(args->kernel->profile_info.queued), NULL);
    ret_err |= clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_SUBMIT, sizeof(cl_ulong), &(args->kernel->profile_info.submitted), NULL);
    ret_err |= clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &(args->kernel->profile_info.started), NULL);
    ret_err |= clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &(args->kernel->profile_info.finished), NULL);
    clGetDeviceInfo(args->dev, CL_DEVICE_PROFILING_TIMER_RESOLUTION, sizeof(size_t), &(args->kernel->profile_info.timer_resolution), NULL);
    free(args);
}
#endif  // ENABLE_KERNEL_PROFILER

static char *get_vendor_name(int vendor) {
    switch (vendor) {
        case AMD_PLATFORM: return _AMD_VENDOR;
        case INTEL_PLATFORM: return _INTEL_VENDOR;
        case NVIDIA_PLATFORM: return _NVIDIA_VENDOR;
        case INTEL_21_EXPERIMENTAL: return _INTEL_2_1_EXPERIMENTAL;
    }
    return "NA";
}

/// calculates the size of ``fp`` FILE and rewinds position back 
/// to 0.
/// On success returns 0 or a positive number representign the size
/// of ``fp``. On error returns a negative number.
static size_t get_file_size(FILE *fp) {
    long filesize;
    if(fseek(fp, 0, SEEK_END)) {
        return -ERROR_UNEXPECTED_IO_ERROR;
    }  
    filesize = ftell(fp);
    if (filesize < 0) {
        return -ERROR_UNEXPECTED_IO_ERROR;
    }
    rewind(fp);
    return filesize;
}

static cl_command_queue get_queue(env_t *env, queue_id_t id) {
    return env->queues[id];
}

int env_init(env_t *env, int platform) {
    int ret = 0;
    cl_platform_id platforms[_MAX_PLATFORMS];
    cl_uint ret_platform_cnt;

    cl_int cl_reterr = clGetPlatformIDs(_MAX_PLATFORMS, platforms, &ret_platform_cnt);
    if(cl_reterr != CL_SUCCESS) {
        ret = (int) cl_reterr;
        goto clean_exit;
    }

    env->platform = platforms[0]; // By default select the first entry in platforms array

    if (platform != DEFAULT_PLATFORM) { //set the chosen vendor
        //allocate buffer to query the name
        char buffer[_MAX_PLATFORM_VENDOR_NAME_SZ];
        memset(buffer, 0, _MAX_PLATFORM_VENDOR_NAME_SZ);
        char *vendor_name = get_vendor_name(platform);
        for (int i = 0; i < ret_platform_cnt; i++) {
            // Query platform name, if error set vendor to default 
            cl_reterr = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, _MAX_PLATFORM_VENDOR_NAME_SZ, buffer, NULL);
            if (cl_reterr != CL_SUCCESS) {
                ret = cl_reterr;
                goto clean_exit;
            }
            // if the name matches the requested one, set the platform id and proceed
            if (!strcmp(vendor_name, buffer)) {
                env->platform = platforms[i];
                break;
            }
        }
    }

    //get gpu device
    cl_reterr = clGetDeviceIDs(env->platform, CL_DEVICE_TYPE_GPU, 1, &(env->device), NULL);
    if (cl_reterr != CL_SUCCESS) {
        ret = cl_reterr;
        goto clean_exit;
    }

    env->context = clCreateContext(NULL, 1, &(env->device), NULL, NULL, &cl_reterr);
    if (cl_reterr != CL_SUCCESS) {
        ret = cl_reterr;
        goto clean_exit;
    }

    env->allocated_queues = 0;
    env->device_name = NULL;

clean_exit:
    return ret;
}

void env_destroy(env_t * env) {
    clReleaseContext(env->context);
    for(int i = 0; i < env->allocated_queues; i++) {
        clReleaseCommandQueue(env->queues[i]);
    }
    if(env->device) {
        free(env->device_name);
    }
}

int env_program_init(env_program_t *program, env_t *env, const char *filename, const char *compile_flags) {
    int ret = 0;
    FILE *file_handle;
    size_t filesize;
    char *file_buffer;
    size_t fread_ret;
    cl_int cl_reterr;
    cl_program cl_prog;
    program->env = env;
    program->built = 0;
    program->build_log = NULL;

    // Open file and allocate buffer to read it.
    file_handle = fopen(filename, "r");
    if(!file_handle) {
        return -ERROR_FILE_NOT_FOUND;
    }

    filesize = get_file_size(file_handle);
    if (filesize < 0) {
        ret = -ERROR_UNEXPECTED_IO_ERROR;
        goto cleanup;
    }

    file_buffer = (char *) malloc(sizeof(char) * filesize + 1);

    //read the file and null-terminate it
    fread_ret = fread(file_buffer, sizeof(char), filesize, file_handle);
    if(fread_ret != filesize) {
        fprintf(stderr, "WARNING: Could not read the entire program file. Expected to read: %zd; actually read: %zd\n", filesize, fread_ret);
    }
    file_buffer[filesize] = '\0';

    //create opencl program, handle error
    cl_prog = clCreateProgramWithSource(env->context, 1, (const char **) &file_buffer, &filesize, &cl_reterr);
    if(cl_reterr != CL_SUCCESS) {
        ret = cl_reterr;
        goto cleanup_buffer;
    }

    const char *default_flags = " -Werror -Iinclude -DOPENCL_COMPILER -DLIB_ENV_BUILD";
    //build opencl program, handle error
    compile_flags = compile_flags == NULL ? default_flags : compile_flags;
    cl_reterr = clBuildProgram(cl_prog, 0, NULL, compile_flags, NULL, NULL);
    program->program = cl_prog;
    if(cl_reterr != CL_SUCCESS) {
        ret = cl_reterr;
        goto cleanup_buffer;
    }
    program->built = 1;

cleanup_buffer:
    free(file_buffer);
cleanup:
    fclose(file_handle);
    return ret;
}

void env_program_destroy(env_program_t *program) {
    if(program->build_log) {
        free(program->build_log);
    }
    clReleaseProgram(program->program);
}

char *env_build_status(env_program_t *prog) {
    if(prog->built && prog->build_log) {
        return prog->build_log;
    }
    size_t log_size = 100000;
    // this first call is just to get the size
    cl_int cl_reterr = clGetProgramBuildInfo(prog->program, prog->env->device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
    if(cl_reterr != CL_SUCCESS) {
        return NULL;
    }
    prog->build_log = (char *) malloc(sizeof(char) * log_size + 1);
    prog->build_log[log_size] = '\0';
    // second call is to actually get the logs
    cl_reterr = clGetProgramBuildInfo(prog->program, prog->env->device, CL_PROGRAM_BUILD_LOG, log_size + 1, prog->build_log, NULL);
    if(cl_reterr != CL_SUCCESS) {
        free(prog->build_log);
        prog->build_log = NULL;
        return NULL;
    }
    return prog->build_log;
}

queue_id_t env_new_queue(env_t *env) {
    if(env->allocated_queues == MAX_QUEUES) {
        return -2;
    }
    cl_int cl_reterr;
#ifdef ENABLE_KERNEL_PROFILER
    const cl_command_queue_properties q_properties = CL_QUEUE_PROFILING_ENABLE; //{CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
#else
    const cl_command_queue_properties q_properties = 0;
#endif
    env->queues[env->allocated_queues] = clCreateCommandQueue(env->context, env->device, q_properties, &cl_reterr);
    if (cl_reterr != CL_SUCCESS) {
        return -1;
    }

    return env->allocated_queues++;
}

char *get_device_name(env_t *env) {
    if(!env->device_name) {
        size_t size;
        clGetDeviceInfo(env->device, CL_DEVICE_NAME, 0, NULL, &size);
        env->device_name = (char *) malloc (sizeof(char) * size + 1);
        clGetDeviceInfo(env->device, CL_DEVICE_NAME, sizeof(char) * size + 1, env->device_name, NULL);
    }
    return env->device_name;
}

int env_kernel_init(env_kernel_t *kernel, env_program_t *program, const char *kfn, size_t global_sz, size_t local_sz) {
    if(program->built) {
        cl_kernel cl_kern;
        cl_int cl_reterr;
        cl_kern = clCreateKernel(program->program, kfn, &cl_reterr);
        if(cl_reterr == CL_SUCCESS) {
            kernel->kernel = cl_kern;
            kernel->global_sz = global_sz;
            kernel->local_sz = local_sz;
            kernel->args_passed = 0;
            kernel->program = program;
#if ENABLE_KERNEL_PROFILER
            kernel->profile_info.started 
            = kernel->profile_info.finished
            = kernel->profile_info.queued
            = kernel->profile_info.submitted = 0; 
#endif
        }
        return 0;
    } else {
        return -ERROR_INVALID_PROGRAM;
    }
}

void env_kernel_destroy(env_kernel_t *kernel) {
    clReleaseKernel(kernel->kernel);
}

int _env_set_karg(env_kernel_t *kernel, unsigned int index, size_t arg_sz, const void *val) {
    cl_int cl_reterr = clSetKernelArg(kernel->kernel, index, arg_sz, val);
    if(cl_reterr == CL_SUCCESS) {
        kernel->args_passed++;
    }
    return (int) cl_reterr;
}

int env_enqueue_kernel(env_kernel_t *kernel, queue_id_t queue_id, unsigned int work_dim) {
    cl_command_queue q = get_queue(kernel->program->env, queue_id);
    int ret = 0;
    cl_event event;
    cl_int cl_reterr = clEnqueueNDRangeKernel(q,
                                            kernel->kernel,
                                            work_dim,
                                            NULL,
                                            &(kernel->global_sz),
                                            &(kernel->local_sz),
                                            0,
                                            NULL, &event);                
    if(cl_reterr != CL_SUCCESS) {
        ret = cl_reterr;
        goto clean_exit;
    }
    kernel->event = event;
    kernel->q = q;
    kernel->q_id = queue_id;

#ifdef ENABLE_KERNEL_PROFILER
    profiler_callback_arg_t *args = (profiler_callback_arg_t *) malloc(sizeof(profiler_callback_arg_t));
    args->kernel = kernel;
    args->dev = kernel->program->env->device;
    clSetEventCallback(event, CL_COMPLETE, kernel_profiler_callback, (void *) args);
#endif

clean_exit:
    return ret;
}

size_t get_cache_size(const env_t *env) {
    size_t ret;
    cl_int cl_reterr = clGetDeviceInfo(env->device, CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, sizeof(size_t), &ret, NULL);
    if(cl_reterr == CL_SUCCESS) {
        return ret;
    }
    return 0;
}

shared_buf_t *create_shared_buffer(size_t size, env_t *env, cl_mem_flags flags) {
    cl_int cl_reterr;
    shared_buf_t *ret = (shared_buf_t *) malloc(sizeof(shared_buf_t));
    if(!ret) return ret;  // if allocation fails return NULL

    // If necessary increase size to be a multiple of CACHE_LINE_SIZE.
    // This is required to create zero-copy buffer
    size_t requested_size = size;
    if(size % CACHE_LINE_SIZE) {
        size += (CACHE_LINE_SIZE - size % CACHE_LINE_SIZE);
    }
    ret->host_handler = aligned_alloc(PAGE_SIZE, size);
    if(!(ret->host_handler)) {  // if aligned allocation fails return NULL
        free(ret);
        return NULL;
    }

    ret->device_handler = clCreateBuffer(env->context, flags | CL_MEM_USE_HOST_PTR, size, ret->host_handler, &cl_reterr);
    if(cl_reterr != CL_SUCCESS) {
        free(ret->host_handler);
        free(ret);
        return NULL;
    }
    ret->env = env;
    ret->mapped_ptr = NULL;
    ret->queued_on = NULL;
    ret->size = requested_size;
    ret->total_size = size;
    return ret;
}

void destroy_shared_buffer(shared_buf_t *buf) {
    free(buf->host_handler);
    clReleaseMemObject(buf->device_handler);
    free(buf);
}

void *map_shbuf(shared_buf_t *buf, queue_id_t q_id, cl_map_flags flags) {
    cl_int ret;
    cl_command_queue queue = get_queue(buf->env, q_id);
    buf->mapped_ptr = clEnqueueMapBuffer(queue, buf->device_handler, CL_TRUE, flags, 0, buf->size, 0, NULL, NULL, &ret);
    buf->queued_on = queue;
    return buf->mapped_ptr;
}

void unmap_shbuf(shared_buf_t *buf) {
    clEnqueueUnmapMemObject(buf->queued_on, buf->device_handler, buf->mapped_ptr, 0, NULL, NULL);
    buf->mapped_ptr = NULL;
    buf->queued_on = NULL;
}