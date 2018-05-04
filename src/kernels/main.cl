__kernel void validate(__global int *global_lock, size_t global_lock_sz, __global int *readset, size_t rs_size, __global int *abort, int start_position) {
    int thread_num = get_global_size(0);
    int id = get_global_id(0);

    start_position = start_position * rs_size / sizeof(int);

    int local_rs_size = rs_size / sizeof(int) / thread_num;

    //int i = 0, j = thread_num * i + id;
    //printf("%d\n", start_position);
    for(int j = local_rs_size * id; j < local_rs_size * (id + 1); j++) {
        if (readset[j] < global_lock[j + start_position]) {
            *abort = 1;
            return;
        }
        if (j > rs_size / sizeof(int)) return; 
    } 
}
