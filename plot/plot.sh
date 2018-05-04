#!/bin/bash

cd opencl/tinysim
make clean
make

sizes=(1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608 16777216 33554432 67108864 134217728)
iters=11
echo $iters > cpu.txt
echo $iters > gpu.txt
for SIZE in $sizes
do
	for ((IT=0; IT<$iters; IT++))
    do
        echo -n $(LD_LIBRARY_PATH=/opt/intel/opencl-sdk/lib64/ ./program 1 $SIZE)"," >> cpu.txt
        echo -n $(LD_LIBRARY_PATH=/opt/intel/opencl-sdk/lib64/ ./program 0 $SIZE)"," >> gpu.txt
    done
done
