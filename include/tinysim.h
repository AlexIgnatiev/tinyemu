#ifndef __TINYSIM_H__
#define __TINYSIM_H__

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define get_num_cpus() sysconf(_SC_NPROCESSORS_ONLN)
#define TXS_NUM 1
#define READ_SET_PORTION TXS_NUM

#define pclock(_ts) printf("%ld.%09ld\n", _ts.tv_sec, _ts.tv_nsec / 1000000)
#define DEFAULT_KERNEL_PATH "src/kernels/main.cl"
#define DEFAULT_THREAD_NUM 1
#define DEFAULT_CACHE_FLUSH_PROB 100
#define DEFAULT_HOST_ONLY 0
#define DEFAULT_DATASET_SZ 0x600000 //6MB
#define USAGE "Usage: program [-k src/kernels/main.cl] [-t 1] [-s 6291456] [-d gpu] [-p 100]\n" \
"-k:string    - kernel path\n"\
"-t:int       - number of threads\n"\
"-s:int       - dataset size in bytes\n"\
"-d:{gpu|cpu} - device where validation should be done\n"\
"-p:[0-100]   - probability with which cache line of readset will be dropped before validation\n"

#define rdtsc(void) ({ \
    register unsigned long long res; \
    __asm__ __volatile__ ( \
        "xor %%rax,%%rax \n\t" \
        "rdtsc           \n\t" \
        "shl $32,%%rdx   \n\t" \
        "or  %%rax,%%rdx \n\t" \
        "mov %%rdx,%0" \
        : "=r"(res) \
        : \
        : "rax", "rdx"); \
    res; \
})

typedef struct {
    char *kernel_path;
    unsigned int thread_num;
    size_t dataset_size;
    unsigned char host_only;
    unsigned char flush_probability;
} options_t;

options_t parse_opts(int argc, char **argv) {
    options_t ret = {
        .kernel_path=DEFAULT_KERNEL_PATH,
        .thread_num=DEFAULT_THREAD_NUM,
        .dataset_size=DEFAULT_DATASET_SZ,
        .host_only=DEFAULT_HOST_ONLY,
        .flush_probability=DEFAULT_CACHE_FLUSH_PROB};
    
    int opt, p;
    while((opt = getopt(argc, argv, "k:t:s:d:p:")) != -1) {
        switch(opt) {
            case 'k':
                ret.kernel_path = optarg;
                break;
            case 't':
                ret.thread_num = atoi(optarg);
                break;
            case 's':
                ret.dataset_size = atoi(optarg);
                break;
            case 'd':
                ret.host_only = strncmp(optarg, "gpu", 3);
                break;
            case 'p':
                p = atoi(optarg);
                if(p > 100) ret.flush_probability = 100;
                else if(p < 0) ret.flush_probability = 0;
                else ret.flush_probability = p;
                break;
            case ':':
                fprintf(stderr, USAGE);
                exit(-1);
        }
    }
    return ret;
}


#endif