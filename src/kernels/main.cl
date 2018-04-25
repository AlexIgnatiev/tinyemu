__kernel void kmain(__global int *data, int size) {
    int i = 0;

    while(data[0] != 50 && i++ < 100000000);
    printf("kdata: %d\n", data[0]);
    data[0] = 80;
}

__kernel void validate(__global int *global_lock, size_t global_lock_sz, __global int *readset, size_t rs_size, __global int *abort, int start_position) {
    int thread_num = get_global_size(0);
    int id = get_global_id(0);
    start_position = start_position * rs_size / sizeof(int);
    int i = 0, j = thread_num * i + id;
    //printf("%d\n", start_position);
    for(; j < rs_size / sizeof(int); j += thread_num * ++i + id) {
        if (readset[j] < global_lock[j + start_position]) {
            *abort = 1;
            return;
        }
    } 
}