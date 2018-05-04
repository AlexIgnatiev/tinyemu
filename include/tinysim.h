#ifndef __TINYSIM_H__
#define __TINYSIM_H__

#define get_num_cpus() sysconf(_SC_NPROCESSORS_ONLN)
#define TXS_NUM 1
#define READ_SET_PORTION TXS_NUM

#define pclock(_ts) printf("%ld.%09ld\n", _ts.tv_sec, _ts.tv_nsec / 1000000)

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

#endif